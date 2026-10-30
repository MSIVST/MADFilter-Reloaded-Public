# Security and problem reporting

## 1. Know what is being installed

MADFilter Reloaded runs inside the application that uses it. A decoder fault
can therefore affect that application. The current binaries are unsigned.
Compare downloaded files with the checksums supplied by the release owner.

Registration makes the filter available to compatible 32-bit applications
across the system. Follow [HOW_TO_USE.md](HOW_TO_USE.md) to remove it after a
test or an interrupted session.

## 2. Understand the testing limits

The single XP-through-11 release build passed automated checks on Windows 11.
The same packaged binaries also completed Fallout 3 and New Vegas checks with
positive feedback and verified cleanup. Testing does not guarantee that all defects have been
found. There has been no independent security audit, and older Windows versions
have not been confirmed to work.

Build protections, test coverage, and remaining technical limits are recorded
in [Technicals.md](Technicals.md#security-and-validation).

## 3. Prepare a private report

Report a suspected security problem privately to the project maintainers.
Do not post the triggering file or a crash dump in a public issue.

Include the filter version, Windows version, affected application, steps to
reproduce, and the error message. If possible, provide a small test file you
are allowed to share. Avoid distributing game assets.

## 4. Check attachments before sharing

Logs can contain local paths. Crash dumps and system reports may contain other
private information. Remove personal details and share only what is needed.
Diagnostic logging is off by default and can add lag when enabled.
