# sample_rate_matrix.ps1 - exercise every MPEG sample-rate/channel pair.
#
# Generates short fixtures with ffmpeg, runs the decoder offset suite, then
# builds a direct-show graph for each fixture and verifies the negotiated
# output format. The graph phase registers /fallout3 and always unregisters.

[CmdletBinding()]
param(
    [string]$MadDecTest,
    [string]$GraphTest,
    [string]$RegisterTool,
    [string]$WorkDir = (Join-Path $env:TEMP "madfilter-rates"),
    [int]$GraphTimeoutSeconds = 20,
    [switch]$SkipGraph
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

if (-not $MadDecTest) {
    $MadDecTest = Join-Path $repo "build-work\maddec_test.exe"
}
if (-not $GraphTest) {
    $GraphTest = Join-Path $repo "build-msvc\Release\filter_graph_test.exe"
}
if (-not $RegisterTool) {
    $RegisterTool = Join-Path $repo "build-msvc\Release\MADFilterRegister.exe"
}

$cases = @(
    @{ Rate = 48000; Channels = 2; Bitrate = 128 },
    @{ Rate = 48000; Channels = 1; Bitrate = 64 },
    @{ Rate = 44100; Channels = 2; Bitrate = 128 },
    @{ Rate = 44100; Channels = 1; Bitrate = 64 },
    @{ Rate = 32000; Channels = 2; Bitrate = 96 },
    @{ Rate = 32000; Channels = 1; Bitrate = 48 },
    @{ Rate = 24000; Channels = 2; Bitrate = 64 },
    @{ Rate = 24000; Channels = 1; Bitrate = 32 },
    @{ Rate = 22050; Channels = 2; Bitrate = 64 },
    @{ Rate = 22050; Channels = 1; Bitrate = 56 },
    @{ Rate = 16000; Channels = 2; Bitrate = 48 },
    @{ Rate = 16000; Channels = 1; Bitrate = 32 },
    @{ Rate = 12000; Channels = 2; Bitrate = 24 },
    @{ Rate = 12000; Channels = 1; Bitrate = 16 },
    @{ Rate = 11025; Channels = 2; Bitrate = 24 },
    @{ Rate = 11025; Channels = 1; Bitrate = 16 },
    @{ Rate = 8000;  Channels = 2; Bitrate = 16 },
    @{ Rate = 8000;  Channels = 1; Bitrate = 8 }
)

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    throw "ffmpeg was not found on PATH."
}
if (-not (Test-Path $MadDecTest)) {
    throw "Decoder test not found: $MadDecTest"
}
if (-not $SkipGraph -and -not (Test-Path $GraphTest)) {
    throw "Graph test not found: $GraphTest"
}
if (-not $SkipGraph -and -not (Test-Path $RegisterTool)) {
    throw "Register tool not found: $RegisterTool"
}

New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null

$fixtures = @()
foreach ($case in $cases) {
    $name = "rate-$($case.Rate)-$($case.Channels)ch-$($case.Bitrate)k.mp3"
    $path = Join-Path $WorkDir $name
    if (-not (Test-Path $path)) {
        & ffmpeg -hide_banner -loglevel error -y `
            -f lavfi -i "sine=frequency=440:sample_rate=$($case.Rate):duration=3" `
            -c:a libmp3lame -b:a "$($case.Bitrate)k" -ac $case.Channels $path
        if ($LASTEXITCODE -ne 0) {
            throw "ffmpeg failed for $name"
        }
    }
    $fixtures += [pscustomobject]@{
        Name = $name
        Path = $path
        Rate = $case.Rate
        Channels = $case.Channels
    }
}

$failures = 0

Write-Host "Decoder offset suite"
foreach ($fixture in $fixtures) {
    & $MadDecTest --test $fixture.Path *> $null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  FAIL $($fixture.Name)"
        ++$failures
    } else {
        Write-Host "  PASS $($fixture.Name)"
    }
}

if (-not $SkipGraph) {
    Write-Host "DirectShow graph matrix"
    & $RegisterTool /fallout3 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Filter registration failed."
    }

    try {
        foreach ($fixture in $fixtures) {
            $stdout = Join-Path $WorkDir "graph-$($fixture.Name).out"
            $stderr = Join-Path $WorkDir "graph-$($fixture.Name).err"
            $process = Start-Process -FilePath $GraphTest `
                -ArgumentList @($fixture.Path, "$($fixture.Rate)", "$($fixture.Channels)") `
                -RedirectStandardOutput $stdout `
                -RedirectStandardError $stderr `
                -WindowStyle Hidden -PassThru

            $process | Wait-Process -Timeout $GraphTimeoutSeconds `
                -ErrorAction SilentlyContinue
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
                Write-Host "  HUNG $($fixture.Name)"
                ++$failures
                continue
            }
            if ($process.ExitCode -ne 0) {
                Write-Host "  FAIL $($fixture.Name) exit=$($process.ExitCode)"
                ++$failures
                continue
            }

            $output = Get-Content -LiteralPath $stdout -ErrorAction SilentlyContinue
            $format = ($output | Select-String "^Output:").Line
            if (-not ($output | Select-String "^PASS:")) {
                Write-Host "  FAIL $($fixture.Name) (no PASS)"
                ++$failures
            } else {
                Write-Host "  PASS $($fixture.Name) $format"
            }
        }
    } finally {
        & $RegisterTool /unregister | Out-Null
    }
}

if ($failures -ne 0) {
    Write-Host "$failures failure(s)."
    exit $failures
}
Write-Host "All sample-rate cases passed."
exit 0
