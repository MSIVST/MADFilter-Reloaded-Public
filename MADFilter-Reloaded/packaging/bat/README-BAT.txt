MADFilter Reloaded - simple setup

Keep these scripts beside MADFilterReloaded.ax and MADFilterRegister.exe.

1. Double-click Status.bat to check registration.
2. Right-click Register.bat and choose Run as administrator.
3. Start your game normally. Registration stays active until removed.
4. Close the game and run Unregister.bat as Administrator to remove registration.

For one session instead, start unregistered and run Run-Game.bat as Administrator.
Enter the actual game executable path. Keep the window open until the game exits
and cleanup finishes. Cleanup removes registration rather than restoring an old
installation. The script does not discover games or configure Steam/mod loaders.
For mod managers and script extenders, register once and launch normally through
your usual tool instead. GECK integration is not runtime-qualified.
See HOW_TO_USE.md for the tested Steam New Vegas setup.

All registration affects compatible 32-bit applications across the system.
Scripts do not elevate themselves. From a prompt, /nopause skips the final key
press. Run-Game accepts it before or after one quoted executable path.

If interrupted, close the game, run Unregister.bat as Administrator, and check
Status.bat. Normal-priority registration and forced game settings are advanced
helper commands documented in Technicals.md.

One x86 package targets Windows XP through Windows 11. On XP, use an
Administrator account or Run as with administrator credentials. The batch
workflow needs no PowerShell. Older-Windows runtime validation remains pending.
