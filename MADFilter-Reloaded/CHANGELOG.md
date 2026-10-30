# Changes in MADFilter Reloaded

## 2.1.0 - final release, 2026-10-29

- Promote rc.2 to the final release with the exact tested filter, helper,
  four batch entry points, and libmad dependency source.
- Update release documentation and retain the scoped Windows 11 results
  and unverified older-Windows compatibility limits.
- Add a technical comparison matrix for original MADFilter, Reloaded and the
  Windows Media MP3 Decoder, with source/documentation and test-scope limits.
- Keep the self-contained Windows package and separate project/dependency
  source downloads.

## 2.1.0-rc.2 - prerelease, 2026-10-26

- Public packaging revision: remove historical reference folders from the public
  tree and project source download; supply exact libmad sources separately.
  The Windows package retains the unchanged statically linked binaries.

- Keep only Register.bat, Unregister.bat, Status.bat, and Run-Game.bat.
  Remove Install/Uninstall aliases without changing registration behavior.
- Document the persistent-registration workflow for mod managers and script
  extenders, and the unverified GECK playback scope.
- Verify the four-script layout with 16 isolated batch checks.

## 2.1.0-rc.1 - prerelease, 2026-10-26

Release preparation now uses one x86 build targeting XP through Windows 11.
The modern release variant has been removed from packaging. The rebuilt
artifacts pass automated tests and scoped Fallout 3/New Vegas checks on Windows
11. Legacy-OS runtime testing remains pending.

1. Added automatic playback settings for Fallout 3, New Vegas, and Oblivion.
2. Improved support for different audio rates, mono and stereo tracks, and
   audio supplied in small pieces.
3. Corrected audio loss at input boundaries and improved recovery from damaged
   audio without shortening the tested damaged-frame playback duration.
4. Fixed acceptance of incompatible audio formats, changes of format within a
   track, and handling of track metadata. Also fixed a buffer utility that the
   current playback decoder does not use.
5. Improved game launch and cleanup handling. Restored the familiar register and
   unregister batch-file names and included the scripts in the binary package.
   Removed self-relaunching, fixed optional argument handling and failure status,
   and passed 22 isolated batch checks. Simplified the package to Install,
   Uninstall, Status, and Run-Game, plus matching register/unregister aliases.
   Both installation names now select preferred priority.
6. Added targeted automated checks. The corrected build passed short Fallout 3
   and New Vegas sessions with positive feedback and verified cleanup.
7. Reorganized the documentation into setup steps, plain-language results, and
   a release-facing technical reference. Internal notes stay outside release archives.

See [release notes](RELEASE_NOTES.md) for current limits and
[Technicals.md](Technicals.md#current-candidate) for the detailed fixes and tests.

## 2.0.0 - earlier unreleased work

Rebuilt the original project with a newer audio decoder and build tools, kept
each playback instance separate, added limits and input checks, and introduced
automated playback and damaged-input tests. Removed the old settings page and
processor-specific copying routines. The project remained 32-bit and GPL v2.

## 1.0.0.3 - original project

The original MADFilter is retained as a historical source and distribution
reference. Required credits and license notices remain unchanged.

See [Technicals.md](Technicals.md#current-candidate) for current validation.
Earlier measurements apply to their dated builds, not automatically to this candidate.
