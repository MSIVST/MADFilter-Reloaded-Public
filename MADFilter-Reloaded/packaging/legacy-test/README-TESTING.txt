MADFilter Reloaded - historical Windows test package
Original package date: 2026-07-28

This guide belongs to the older Win7/XP/Source test bundle. Those folders and
test tools are not supplied by the current Windows x86 release candidate.
For the current candidate, follow HOW_TO_USE.md. Actual Windows XP or Vista
compatibility has not been established by testing on Windows 11.

1. Use a disposable or backed-up test computer and the complete historical
   package. The intended test systems were Windows 7 SP1, Windows XP x86 SP3,
   or Windows XP x64 SP2. The filter remains 32-bit.

2. Open Command Prompt as Administrator. Change to the package's Win7 folder
   and run RUN-WIN7-TEST.cmd, or change to XP and run RUN-XP-TEST.cmd.
   In XP, COLLECT-XP-SYSTEM-INFO.cmd is optional; review its output for personal
   information before sharing it.

3. Read MADFilter-Test-Results.txt. A successful run ends with RESULT=PASS.
   Keep any failure message; do not treat an incomplete run as a pass.

4. The scripts attempt to remove registration after testing. If interrupted,
   run MADFilterRegister.exe /unregister as Administrator, then run
   MADFilterRegister.exe /status and expect NOT REGISTERED.

5. Keep the results with the tested package version and computer details.
   Read XP-COMPATIBILITY-NOTICE.txt before interpreting the XP result.

See the current project's Technicals.md for current build instructions and
qualification limits. This historical bundle is separate from the current release.
