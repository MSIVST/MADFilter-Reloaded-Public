# MADFilter Reloaded 2.1.0-rc.2

Prerelease dated 2026-10-26. One x86 build targets Windows XP through Windows 11.
Windows 11 qualification is complete for the recorded scope; legacy-Windows
runtime testing remains pending.

## 1. What changed

This packaging/documentation update retains the exact rc.1 filter and helper
binaries. It keeps only Register.bat, Unregister.bat, Status.bat, and Run-Game.bat
and documents mod-manager and GECK expectations; those integrations remain
unqualified. The existing decoder improvements and validation follow.

- Added automatic playback settings for Fallout 3, New Vegas, and Oblivion.
- Improved handling of small pieces of audio, damaged audio, and track changes.
- Fixed five issues found in the latest review, including lost audio at input
  boundaries and acceptance of an incompatible audio format.
- Added a helper that starts a game and removes registration when the game exits.
- Kept diagnostic file logging off by default and expanded automated checks.

## 2. What passed

The single x86 release build targets Windows XP through Windows 11. Its freshly
built filter and helper passed all four automated test suites on Windows 11.
Binary checks confirmed the expected 32-bit format and XP-era target versions,
with no imports from modern Visual C++ runtime DLLs.

The exact unified release also completed Fallout 3 (about 54 minutes) and
New Vegas (about 12 minutes) sessions with positive user feedback, successful
launcher exits, and independently verified cleanup. No matching crash events
were recorded. An initial New Vegas startup failed before the filter loaded;
the unchanged retry passed. These are scoped results on one Windows 11 computer.

## 3. What remains unconfirmed

Long sessions, other computers, every gameplay scenario, and older Windows
versions remain unconfirmed. Earlier stutter and cutoff reports had multiple
possible causes. This release does not promise to fix every game audio problem.

Use `Register.bat`, `Unregister.bat`, `Status.bat`, and `Run-Game.bat`.
Register uses preferred priority. Use **Run as administrator** for registration
changes. The four-script layout passed 16 isolated command-handling checks.
The unchanged helper previously passed live registration/removal and game-session
cleanup checks. No new game session was needed for these script-name/doc changes.

## 4. Release packages

- `MADFilter-Reloaded-2.1.0-rc.2-Windows-x86.zip`: the single XP-through-11 filter, helper, instructions,
  technical reference, license, and notices.
- `MADFilter-Reloaded-2.1.0-rc.2-source.zip`: project source and notices, without
  historical reference folders or vendored dependencies.
- `MADFilter-Reloaded-2.1.0-rc.2-dependency-source.zip`: the exact libmad source
  snapshot needed to build, including its original license and notices.
- `SHA256SUMS.txt`: checksums for all three archives.

The Windows package remains self-contained. libmad and the C/C++ runtime are
statically linked; no separate dependency DLL or source download is needed for
playback. Only developers need both source archives. This packaging revision
changes documentation and source layout, not the tested binaries.

Follow [HOW_TO_USE.md](HOW_TO_USE.md) for setup and
[Technicals.md](Technicals.md) for exact test results.
Internal investigations and maintainer notes are excluded from all archives.
