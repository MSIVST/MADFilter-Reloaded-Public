# MADFilter Reloaded

![Logo](https://i.imgur.com/zUwZldz.png)

**MPEG audio playback for classic 32-bit Windows games, with automatic Fallout
and Oblivion settings, tested decoder fixes, and familiar batch-file setup.**

MADFilter Reloaded converts MP3 and other MPEG audio into sound that compatible
Windows applications can play. It is a DirectShow filter: an audio component
that a game can select as part of its playback system.

**Release:** 2.1.0-rc.2 (prerelease). One x86 build targets Windows XP through
Windows 11. Scoped automated and game checks passed on Windows 11; older
Windows runtime verification remains pending. Licensed under GPL v2.

[Usage guide](MADFilter-Reloaded/HOW_TO_USE.md) · [Release notes](MADFilter-Reloaded/RELEASE_NOTES.md) ·
[Technical reference](MADFilter-Reloaded/Technicals.md) · [Changelog](MADFilter-Reloaded/CHANGELOG.md)

## Benefits

- **Automatic settings for Fallout 3, New Vegas, and Oblivion.** Recognizes the
  game executable and adjusts playback timing and continuity without manual
  configuration for the standard game names.
- **Preserves audio delivered in small pieces.** Fixes reproduced cases where
  split input boundaries caused opening audio frames to be lost.
- **Keeps damaged audio from shortening the tested playback timeline.** Adds
  replacement silence for recoverable damaged frames, within a fixed limit,
  so those frames do not simply disappear from the track's duration.
- **Handles mono and stereo tracks at their original rate.** Checks that the
  decoded audio matches the format the application expects and rejects
  incompatible changes instead of passing them downstream.
- **Keeps simultaneous decoder instances separate.** Each instance has its own
  playback state and buffers, avoiding shared decoder state between streams.
- **Limits memory use and recovery work.** Fixed buffer sizes and bounded error
  recovery prevent these operations from growing indefinitely with input.
- **Familiar batch-file setup.** Use `Register.bat`, `Unregister.bat`,
  `Status.bat`, and `Run-Game.bat`.
- **Optional cleanup after one game session.** The session helper starts the
  game, waits for it to exit, then removes registration automatically.
- **Tested fixes you can inspect.** Source, regression tests, build instructions,
  and qualification limits are included. Diagnostic file logging stays off
  during normal use.

## Quick start

Use the Windows x86 package, not the source ZIP. Extract it into a permanent
folder and keep the filter, executable helper, and batch files together.

1. Double-click `Status.bat` to check for an existing registration.
2. Right-click **`Register.bat` â†’ Run as administrator** to prefer MADFilter
   for compatible game playback.
3. Start your game normally and check music or radio playback.
4. To remove registration, close the game and run
   **`Unregister.bat` â†’ Run as administrator**. Check status again.

Registration applies to compatible 32-bit applications across the system and
stays active until removed. It does not guarantee that every audio stream will
select MADFilter. No game files need to be replaced.

### Try one game session

Start from an unregistered state. Right-click **`Run-Game.bat` â†’ Run as
administrator**, then enter the full path to the actual game executable, such as `Fallout3.exe`. Keep the window
open until the game exits and cleanup finishes. If interrupted, run
`Unregister.bat` as Administrator and check status.

The scripts do not request Administrator rights themselves. The
[usage guide](MADFilter-Reloaded/HOW_TO_USE.md) includes full cleanup steps, command-line options,
and the tested Steam New Vegas direct-launch setup.

## Compatibility and testing

- **Applications:** 32-bit Windows applications using DirectShow MPEG audio,
  including those running on 64-bit Windows.
- **One release build:** the same x86 filter and helper target Windows XP
  through Windows 11. There are no separate XP and modern downloads.
- **Current automated checks:** the freshly rebuilt release passed all four
  test suites on Windows 11. Batch command handling passed 16 isolated checks.
- **Current game checks:** the exact unified release completed Fallout 3
  (about 54 minutes) and New Vegas (about 12 minutes) sessions on Windows 11.
  Both received positive feedback, returned success, and passed cleanup checks.
- **Qualification limits:** one computer and the recorded checks only. An initial
  New Vegas startup failed before the filter loaded; its retry passed. Other
  Windows versions remain unverified. XP-through-11 is the target range.

On Windows XP, run installation/removal from an Administrator account or use
**Run as** with administrator credentials. The batch workflow needs no PowerShell.

MADFilter handles MPEG audio, not Ogg decoding or the game's dialogue/radio
volume controls. These results do not establish a universal stutter fix.
See [Technicals.md](MADFilter-Reloaded/Technicals.md#current-candidate) for exact test scope.

## Documentation and development

- [HOW_TO_USE.md](MADFilter-Reloaded/HOW_TO_USE.md): installation, game testing, cleanup, removal.
- [AUTO_DETECTION.md](MADFilter-Reloaded/AUTO_DETECTION.md): recognized game names and settings.
- [Technicals.md](MADFilter-Reloaded/Technicals.md): build instructions, decoder behavior, test results.
- [RELEASE_NOTES.md](MADFilter-Reloaded/RELEASE_NOTES.md): package contents and current limits.
- [CHANGELOG.md](MADFilter-Reloaded/CHANGELOG.md): changes by version.
- [SECURITY.md](MADFilter-Reloaded/SECURITY.md): private security reporting and safe attachment handling.

The Windows package is self-contained: libmad and the C/C++ runtime are statically
linked. Players do not need either source download or a separate libmad DLL.

To build from source, download both the project source and
`MADFilter-Reloaded-2.1.0-rc.2-dependency-source.zip` from the release. Extract
the dependency ZIP beside `MADFilter-Reloaded/`, creating a sibling `libmad/`, and
follow the [build instructions](MADFilter-Reloaded/Technicals.md#build-from-source).

## Report a problem or contribute

Tell the repository maintainers which version you tested, your Windows and game
versions, the steps that reproduce the issue, and any error message. State
whether the problem affects music, radio, or NPC speech. Remove personal details
from attachments and do not upload game assets. Report security issues privately
as described in [SECURITY.md](MADFilter-Reloaded/SECURITY.md).

For a proposed fix, explain the failure it addresses and include the relevant
reproduction or test result. Keep unrelated changes separate.

## License and credits

[GNU GPL v2](MADFilter-Reloaded/LICENSE). Original MADFilter: Armin Gerritsen.
libmad: Underbit Technologies, Inc.; Tenacity fork: Tenacity contributors.
See [THIRD_PARTY_NOTICES.md](MADFilter-Reloaded/THIRD_PARTY_NOTICES.md) for required upstream notices.
