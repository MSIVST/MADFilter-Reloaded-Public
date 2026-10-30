# MADFilter Reloaded technical reference

Updated 2026-10-29. This is the developer reference. For installation and game
testing, start with [HOW_TO_USE.md](HOW_TO_USE.md).

## Reading order

1. [Current release and evidence](#current-candidate)
2. [Build from source](#build-from-source)
3. [Decoder contract](#decoder-contract)
4. [Host policy](#host-policy)
5. [Registration and diagnostics](#registration-and-diagnostics)
6. [Security and validation](#security-and-validation)

For a side-by-side overview, see the [decoder comparison matrix](#decoder-comparison-matrix).

<a id="current-candidate"></a>
## Current release

Release: **2.1.0**, tag `v2.1.0`; binary version `2.1.0.0`. There is one distributable x86 build targeting Windows XP through Windows 11,
using v141_xp and a static CRT. No separate modern release is produced.
Platform targeting is distinct from runtime qualification.

| Artifact | SHA-256 |
|---|---|
| Current filter | `C7107489B42BBAE1127D8BD4996946E555B241ABFC33FBE6EF825BFDC1399E51` |
| Registration helper | `63EF5E3AECA3B6B13573DD7595379A15A05DF0C695A0A3E7EFF028857BA01A9D` |
| Previous candidate filter | `3CE4C32D1AF1FEA28437DDF45A64205CE931FDB9A86130CE462A087C56B62F17` |

The current filter fixes five reproduced findings:

1. Shared output-format validation rejects incompatible PCM in Release,
   including ordinary, carry-drain, and EOF delivery.
2. Each successfully decoded frame is checked against the established rate
   and channel count before mixing PCM. A format change fails until full reset.
3. Sync acquisition preserves partial candidate frames and split headers
   across the 8 KiB input window. Guard bytes are refreshed before libmad input.
4. `BoundedBuffer::write` compacts consumed space before a physical-tail
   overflow. This utility is not used by the production decoder.
5. Xing/Info/VBRI metadata is discarded independently of audio preroll and
   contributes neither audio duration nor an audio-preroll slot.

| Check | Evidence and scope |
|---|---|
| Single release build, 2026-10-26 | Fresh v141_xp Release, static CRT; CTest 4/4 on Windows 11 |
| Current binary inspection | Both x86, OS/subsystem 5.1; no API-set or modern VC runtime DLL imports |
| Current boundary suite | Included in CTest; latest five-fix regressions retained |
| First-party ASan, earlier diagnostic build | CTest 4/4; not a distributable build |
| Current generated rate/channel fixtures | 18 fixtures, 198 offset/chunk cases passed |
| Current registered playback graphs | 18 formats; selection, run, pause, seek, resume, stop passed |
| Packaged Install/Status/Uninstall scripts | Live registration and cleanup passed |
| Current Fallout 3 session | About 54m26s elapsed; positive feedback, launcher exit 0 |
| Current New Vegas retry | About 11m39s elapsed; positive feedback, launcher exit 0 |

Both games used the exact unified filter/helper through packaged Run-Game.bat.
File diagnostic logging was off. The filter module was observed in each game;
this is not per-stream graph/PCM inspection. No matching crash events were
recorded. Both sessions passed independent dual-view registration cleanup with
positive controls. Elapsed time is not a continuous-active-gameplay measurement;
user feedback was overall, not confirmation of each individual checklist item.

The initial New Vegas attempt failed initialization before MADFilter loaded.
It cleaned up successfully and does not count as an audio pass. The unchanged
retry passed. No cause for the initial startup failure was established.
The scoped Windows 11 qualification is complete on this computer. Other PCs,
longer sessions, all gameplay scenarios, and legacy Windows remain unqualified.
PE/import checks do not establish XP/Vista/7/8/10 runtime compatibility.

<a id="build-from-source"></a>
## Build from source

1. Download the project source ZIP and the matching
   `MADFilter-Reloaded-2.1.0-dependency-source.zip` from the same release.
   Extract the project ZIP first, then extract the dependency ZIP into the
   resulting project root so `MADFilter-Reloaded/` and `libmad/` are siblings.
   For a Git clone, extract the dependency ZIP into the repository root.
   Use this exact dependency snapshot rather than the latest upstream branch.
   Install Visual Studio with the v141_xp C++ toolset,
   its XP SDK support, CMake 3.16+, and DirectShow base-class headers plus an
   x86 `strmbase.lib` rebuilt with v141_xp and the static CRT.
2. Open PowerShell in `MADFilter-Reloaded/`. Substitute your actual SDK paths
   for the two example `C:/SDK` paths below.
3. Configure, build, then run the tests:

```powershell
cmake -G "Visual Studio 17 2022" -A Win32 -T v141_xp -S . -B build-release `
  -DTARGET_WINDOWS_XP=ON `
  -DDIRECTSHOW_INCLUDE_DIR="C:/SDK/baseclasses" `
  -DSTRMBASE_LIB="C:/SDK/lib/x86/strmbase.lib"
cmake --build build-release --config Release
ctest --test-dir build-release -C Release --output-on-failure
```

4. Check `build-release/src/filter/Release/MADFilterReloaded.ax` and
   `build-release/Release/MADFilterRegister.exe`. The single release uses the static CRT.
5. Keep these artifacts distinct from any previously game-tested binary until
   their identity and required tests have been checked.

Release configuration rejects non-v141_xp MSVC filter builds and disabling
TARGET_WINDOWS_XP. Internal ENABLE_ASAN diagnostics may use a modern compiler
with TARGET_WINDOWS_XP=OFF; they are never release assets. The packaging gate
checks the canonical build-release toolset, binary identities, x86 machine type,
PE 5.1 versions, and absence of modern VC runtime DLL imports.

Windows XP use requires an Administrator account or XP Run as with administrator
credentials. PowerShell examples are development/newer-Windows conveniences;
the packaged batch scripts and native helper do not require PowerShell.

<a id="decoder-contract"></a>
## Decoder contract

The x86 DirectShow transform owns a separate libmad decoder per instance and
statically links Tenacity libmad 0.16.4. Input is MPEG-1/2/2.5 Layers I, II, III;
output is signed 16-bit PCM, mono/stereo, at the source rate. Supported MPEG
rates are 48, 44.1, 32, 24, 22.05, 16, 12, 11.025, and 8 kHz.

| Limit | Value |
|---|---|
| Decoder input | 8 KiB plus libmad guard bytes |
| Compressed carry | 256 KiB per filter instance |
| Work per transform | 64 decoded/discarded frames |
| Output allocation | Clamped to 192-256 KiB |
| Consecutive errors | 128 |
| Sync search | 512 KiB |
| Preroll search cap | 32 frames |
| Consecutive concealment | 8 frames |

Recoverable parsed audio-frame errors receive replacement duration per damaged
frame. Header/sync loss without a trustworthy frame retains one replacement
per error burst. This preserves the tested timeline; it cannot recover missing
audio or establish the cause of every live cutoff. Bounds constrain resource
use but do not prove that every path is safe or terminating.

<a id="host-policy"></a>
## Host policy

Construction uses `GetModuleFileNameW(nullptr, ...)` and a case-insensitive
basename match for `Fallout3.exe`, `FalloutNV.exe`, or `Oblivion.exe`.
There is no archive inspection, upstream graph walk, or game-version validation.

| Policy | Generic host | Recognized Bethesda host |
|---|---|---|
| Output timestamps | Segment-relative | Suppressed |
| New segment | Reset decoder and carry | Retain state for monotonic delta 0-600 seconds |
| Input discontinuity | Reset | Ignore per-sample flags |
| Initial audio preroll | Existing threshold outputs fifth frame, withholding four | Zero; recovery may adapt |
| Backward jump or Stop | Reset | Backward jump resets; Stop forces reset on next segment |

Metadata-frame discard is separate from audio preroll. Historical descriptions
of "5-frame preroll" refer to the threshold; they do not mean five audio frames
are actually withheld by the current ordinary path.

`MADFILTER_BSAUDIO=1` forces the full policy for an unrecognized host.
`MADFILTER_NO_TIMESTAMPS=1` suppresses timestamps only. These are diagnostic
overrides. Any nonempty value enables the respective flag, including `0`;
remove the environment variable to stop forcing it.

The filter accepts MPEG media types, not Ogg decoding. Do not generalize the
inspected game assets into a claim about every possible NPC audio source.
The filter contains no NPC-conversation or radio-volume control. Reported
Fallout 3 muting and New Vegas volume reduction during dialogue are observations;
their precise engine implementation was not established by these tests.

<a id="registration-and-diagnostics"></a>
## Registration and diagnostics

| Helper command | Effect |
|---|---|
| `/status` | Read-only class registration report; no elevation required |
| `/register` | System-wide 32-bit registration at normal merit |
| `/fallout3` | System-wide 32-bit registration at preferred merit |
| `/launch <exe> [args...]` | Preferred registration, CreateProcessW, wait, exit-code retrieval, unregister |
| `/unregister` | Remove registration |

Mutating commands require Administrator rights. `/launch` returns the game exit
code when cleanup succeeds; cleanup failure makes the helper fail. Registration
is not process-isolated. A launcher that exits before its child ends the helper's
registration lifetime too soon. Forced termination or power loss requires
manual cleanup. `/status` is not the independent multi-key/dual-view cleanup
audit used in the recorded game validation.

Steam New Vegas direct launch was tested with Steam running and process-local
`SteamAppId=22380` and `SteamGameId=22380`. The user guide shows a temporary
PowerShell setup that restores prior values. This does not qualify arbitrary
mod-loader launch chains.

For a deliberate short diagnostic capture, in the same PowerShell window used
to launch the game:

```powershell
$env:MADFILTER_DIAG_LOG = "$env:TEMP\madfilter.log"
# Run the helper and let the game exit before removing the variable.
Remove-Item Env:MADFILTER_DIAG_LOG
```

Snapshots are written synchronously during Stop, EOF, and destruction. File
logging is off by default because disabling it improved reported lag/cutoffs.
Inspect logs for paths before sharing. `policy a=0 b=1` and `source-bsa` identify
the special policy; `source-generic` identifies the ordinary policy.

Release 2.1.0 includes four batch entry points:

| Script | Helper call |
|---|---|
| `Register.bat` | `/fallout3` (preferred merit) |
| `Unregister.bat` | `/unregister` |
| `Status.bat` | `/status` |
| `Run-Game.bat` | `/launch <exe>` (preferred merit, wait, unregister) |

The Install/Uninstall aliases have been removed. Register and Unregister retain
identical contents to the corresponding rc.1 scripts; no C++ helper or decoder
behavior changed. Users choose Run as administrator for mutating operations.
The scripts do not self-elevate. Earlier release-candidate archives remain unchanged.

Scripts disable delayed expansion, quote paths, use `shift /1` to preserve the
script path when consuming `/nopause`, initialize failure status, and propagate
helper exit codes. Run-Game accepts one executable path, prompted if absent,
and optional `/nopause` before or after it. Additional game arguments require
the native helper. Start unregistered: cleanup removes registration and does
not restore a pre-existing installation. Steam setup and mod-loader lifetimes
are not handled by the generic script.

`python tests/integration/test_batch_helpers.py` passes 16 isolated Windows
cases using a compiled stand-in helper. It also checks
the exact four-script inventory. Cases cover modes, failures, missing files, quoted
prompted paths, argument order, and special-character paths. No registration,
interactive elevation, or live game run is performed by those checks.

### Modding tools and GECK

These are source-based compatibility expectations, not runtime qualification.
MADFilter is an x86 DirectShow COM filter, not an ESP/ESM or script-extender
plugin. Registration is system-wide and persists across mod-manager profiles.
A 64-bit tool cannot load this x86 filter itself, but can launch a 32-bit game
that uses it. Selection still depends on the application's audio graph.

| Workflow | Expected behavior and limits |
|---|---|
| Mod Organizer 2 | Register separately, then launch through MO2 to retain its virtual file system. Do not replace that launch with a direct Run-Game invocation. |
| Vortex or FOMM | Register separately and retain the existing game/script-extender launch entry. MADFilter needs no deployment into Data and has no plugin load-order position. |
| FOSE or xNVSE | The eventual game process determines the automatic policy. Standard Fallout3.exe/FalloutNV.exe names remain recognized regardless of the loader. |
| 4GB-patched games or TTW | If the running executable retains a recognized name, the same host policy applies. This is not a compatibility test of the patch or mod list. |
| GECK / GECK Extender | GECK.exe is not on the automatic Bethesda-policy list. A compatible MPEG DirectShow preview may select MADFilter with generic settings; its actual playback path and filter selection require a separate trace. |
| Audio previews in other tools | A preview is affected only if it uses a compatible 32-bit DirectShow MPEG graph and selects MADFilter. Ogg decoding and audio editing/encoding are outside this filter's scope. |

The launch helper uses CreateProcessW and waits on that process handle only;
it does not track descendant processes. An early-exiting loader can therefore
trigger unregistration while the game is still running. Persistent registration
avoids this lifetime dependency. It does not require elevating the mod manager.
Do not force MADFILTER_BSAUDIO for GECK as a default workaround: the editor's
seek/timestamp requirements have not been qualified against that policy.

MO2 documents its launch/VFS requirement in the
[Executables window guide](https://github.com/ModOrganizer2/modorganizer/wiki/Executables-window).
[FOSE](https://fose.silverlock.org/) describes its scripting extension role;
[xNVSE's author documentation](https://www.nexusmods.com/newvegas/mods/67883?tab=docs)
describes the editor launch option and GECK Extender alternative. These sources
describe the tools, not a certification of MADFilter integration.

### Advanced registration and policy overrides

For normal-priority registration, from an Administrator PowerShell window in
the package folder:

```powershell
.\MADFilterRegister.exe /register
```

For a deliberate forced Bethesda-policy test with an unrecognized host:

```powershell
$previousPolicy = $env:MADFILTER_BSAUDIO
try {
    $env:MADFILTER_BSAUDIO = '1'
    .\MADFilterRegister.exe /launch "C:\Games\Example\Game.exe"
} finally {
    $env:MADFILTER_BSAUDIO = $previousPolicy
}
```

Recognized Fallout 3, New Vegas, and Oblivion executables need no override.
Unregister with `.\MADFilterRegister.exe /unregister` after a persistent
installation or an interrupted session.

<a id="decoder-comparison-matrix"></a>
### MADFilter vs MADFilter Reloaded vs Windows decoder

The Windows column refers specifically to the **Windows Media MP3 Decoder**
(`mp3dmod.dll`, `CLSID_CMP3DecMediaObject`) used as a DMO through DirectShow's
DMO Wrapper. Windows also has other MPEG audio components; this column does
not describe every installed decoder or guarantee which one a game selects.
Microsoft documents this decoder's formats and DMO/MFT interfaces in its
[MP3 decoder reference](https://learn.microsoft.com/en-us/windows/win32/medfound/windows-media-mp3-decoder)
and the DirectShow integration in its
[DMO Wrapper reference](https://learn.microsoft.com/en-us/windows/win32/directshow/dmo-wrapper-filter).

The original column describes the inspected 1.0.0.3 source and package readme.
Its preserved libmad header identifies version 0.15.0 (beta); the original
distributed binary has not been rebuilt or requalified for this comparison.
"Not evaluated" identifies an evidence limit, rather than a missing feature.

| Area | MADFilter 1.0.0.3 | MADFilter Reloaded 2.1.0 | Windows decoder (`mp3dmod.dll`) |
|---|---|---|---|
| Integration | Native x86 DirectShow filter | Native x86 DirectShow filter | DMO via the DirectShow DMO Wrapper; MFT on applicable Windows versions |
| Decoder core | Embedded libmad; preserved header reports 0.15.0 (beta) | Statically linked Tenacity libmad 0.16.4 | Microsoft component (`mp3dmod.dll`); internals not inspected |
| Compressed formats | MPEG audio through the historical libmad wrapper; full format coverage not requalified | MPEG-1/2/2.5 Layers I, II and III | Microsoft documents MPEG-1 and MPEG-2 Layer III (MP3) |
| PCM output | 16-bit stereo; duplicates mono into both channels | Signed 16-bit PCM, mono or stereo at source rate | Documented DMO output includes 8/16-bit PCM, source rate/channels, and optional mono/downsampling; floating-point output is an MFT feature |
| Decoder state | File-static stream, frame and synthesis state shared by filter instances | Decoder state and buffers owned by each instance | Internal state ownership not evaluated here |
| Fallout/Oblivion policy | No executable-name policy found in inspected source | Recognizes Fallout 3, New Vegas and Oblivion; applies scoped timestamp/continuation policy | No equivalent host-name policy described in the cited Microsoft documentation; internal behavior not evaluated |
| Split input and format changes | Not qualified against the current boundary and format-change regressions | Retains split headers/partial frames; checks decoded rate/channels and negotiated PCM format | Not run through the same regression harness |
| Damaged audio and recovery limits | Historical libmad recovery loop; not qualified against the current timeline tests | Bounded recovery, scan/carry/frame budgets and replacement silence for recoverable damaged frames | Recovery and timeline behavior not compared with the current harness |
| Registration and selection | `regsvr32`/`regsvr32 /u`; source registers `MERIT_PREFERRED + 0x2E90000` | Four batch entry points and x86 helper; normal merit or preferred merit + 1; optional session cleanup | Windows component; actual graph selection depends on available filters, media types and merit |
| Installation | Separate MADFilter package and registration | Self-contained Windows x86 package; no libmad or VC runtime DLL download | No MADFilter package required to use the Windows component |
| Validation recorded for this release | No matched current game or automated retest | CTest 4/4, 16 batch checks, generated decoder/graph cases, and scoped prior Fallout 3/New Vegas sessions with the exact packaged binaries | No matched current game or automated retest |
| Windows qualification | Historical package; current OS compatibility not requalified | Targets XP through 11; scoped runtime checks passed on Windows 11 only | Availability and behavior depend on Windows version and installed components; not qualified by this release |

No matched quality, CPU-time, latency, or long-session reliability benchmark
establishes a winner among these three. The matrix describes implementation,
documented capabilities and test scope, rather than measured audio superiority.
Module presence alone does not establish which component decoded a particular
stream. See [Working with Codecs](https://learn.microsoft.com/en-us/windows/win32/directshow/working-with-codecs)
for Microsoft's discussion of the different codec forms and system-dependent
filter inventory.

<a id="security-and-validation"></a>
## Security and validation

The decoder executes in the host process without a sandbox. First-party build
targets use warnings as errors, stack checks, SafeSEH, DEP, ASLR, and CFG.
Vendor libmad has stack/CFG mitigations without the first-party warning policy.
AddressSanitizer instrumentation covers first-party code, not all vendor code.
The binaries are not Authenticode-signed.

Targeted regression tests establish the reproduced fixes. There has been
no sustained coverage-guided campaign or independent security audit. Short
successful game sessions are compatibility observations, not security proof.

Release snapshots exclude internal maintainer notes, game media, raw traces, dumps, symbols, local tools,
build caches, credentials, and agent state. The package audit checks entry bytes,
ZIP CRCs, file links, hashes, and known private-path/credential patterns. Legal
attribution is retained. Pattern checks cannot prove absence of every secret.
ZIP hashes are supplied in `SHA256SUMS.txt`; they change when package docs change.
