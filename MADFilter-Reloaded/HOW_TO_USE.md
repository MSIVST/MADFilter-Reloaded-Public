# Set up, test, and remove MADFilter Reloaded

These steps apply to the Windows x86 package for 2.1.0-rc.2. Testing has been
performed on Windows 11. One x86 build targets Windows XP through Windows 11;
older Windows runtime validation remains pending. On XP, use an Administrator
account or Run as with administrator credentials. Batch installation requires
no PowerShell; the PowerShell examples below are for systems that have it. Use the actual game program, not a separate launcher
that closes after opening the game.

## Simple batch-file setup

Keep the supplied scripts beside the filter and executable helper.

1. Double-click `Status.bat` to check the current registration.
2. Right-click `Register.bat` and choose **Run as administrator**.
3. Start the game normally. Installation remains active after the game exits.
4. To remove it, close the game, right-click `Unregister.bat`, choose
   **Run as administrator**, and check status again.

Register enables preferred selection priority; Unregister removes registration.
Neither operation copies or deletes the extracted files.

For a temporary session instead, start unregistered, right-click `Run-Game.bat`
and choose **Run as administrator**. Enter the full path to the actual game
program. Keep the window open until the game exits and cleanup finishes.
Cleanup removes registration; it does not restore a previous installation.

Use the actual game executable, not a launcher that exits early. The generic
script name does not add game discovery, mod-loader support, or Steam setup.
For Steam New Vegas, use the documented PowerShell setup below; the script does
not configure Steam automatically.

The scripts do not request elevation themselves. Registration affects compatible
32-bit applications across the system. The PowerShell steps below provide the
same helper operations directly. Normal-priority registration and forced game
settings are documented in [Technicals.md](Technicals.md#registration-and-diagnostics).

## Mod managers, script extenders, and GECK

1. Extract MADFilter into a permanent folder outside a mod manager's managed files.
2. Run `Register.bat` as Administrator, then check `Status.bat`.
3. Start the game or GECK through your usual MO2, Vortex, FOSE, or xNVSE entry.
   Keep your existing mod-manager launch settings. Registration does not require
   you to run the mod manager as Administrator.
4. Leave registration enabled between sessions if desired. To remove it, close
   games and editors, run `Unregister.bat` as Administrator, then check status.

Use this persistent workflow for loaders. `Run-Game.bat` waits only for the
program it starts; a launcher that exits early can cause early unregistration.
Launching the game directly can also bypass your mod manager or script extender.
Registration is system-wide, not tied to a mod-manager profile.

These integrations have not been runtime-qualified. GECK uses generic decoder
settings if it selects MADFilter for compatible audio; it does not automatically
receive the game's special settings. See the
[compatibility details](Technicals.md#modding-tools-and-geck).

## 1. Put the files in a permanent folder

Extract the package. Keep `MADFilterReloaded.ax` and `MADFilterRegister.exe`
together. You do not need to copy them into the game folder or replace Windows
files. Do not move them while the filter is registered.

## 2. Open PowerShell as Administrator

Open an Administrator PowerShell window and change to the extracted folder.
Replace this example path with your own:

```powershell
Set-Location "C:\Tools\MADFilter-Reloaded"
```

Check the starting state:

```powershell
.\MADFilterRegister.exe /status
```

For a fresh test, expect `Status: NOT REGISTERED`. If it says `REGISTERED`,
check the displayed file location before changing an existing installation.
To remove that registration for a fresh test, close applications using it,
run `/unregister` as shown in step 6, and check status again.

## 3. Start one game session

For Fallout 3, replace the example path with the location of your game:

```powershell
.\MADFilterRegister.exe /launch "C:\Games\Fallout 3\Fallout3.exe"
```

For the Steam edition of New Vegas, start Steam first. The tested direct-launch
setup used the following temporary settings in the same PowerShell window:

```powershell
$oldSteamAppId = $env:SteamAppId
$oldSteamGameId = $env:SteamGameId
try {
    $env:SteamAppId = '22380'
    $env:SteamGameId = '22380'
    .\MADFilterRegister.exe /launch "C:\Games\Fallout New Vegas\FalloutNV.exe"
} finally {
    $env:SteamAppId = $oldSteamAppId
    $env:SteamGameId = $oldSteamGameId
}
```

Replace the New Vegas path too. These settings apply to this PowerShell window
and programs started from it; the commands restore the previous values when
the session ends.

The helper registers the filter, starts the game, waits for it to exit, and
unregisters it. Registration applies to compatible 32-bit Windows applications
across the system during this time. Leave the helper running until the game
closes. If you need a mod loader, its process lifetime needs separate checking;
the direct-game instructions above do not establish mod-loader support.

## 4. Check playback

1. Load a save and listen to music or radio.
2. Move into another area and note any loading stutter.
3. Talk to an NPC while the radio is playing, then finish the conversation.
4. Try skipping dialogue and note any delay or audio cutoff.
5. Exit the game normally.

These are suggested checks, not a claim that every item has been separately
verified in the recorded sessions. Leave diagnostic file logging off for normal
play; it added lag in earlier testing.

## 5. Confirm cleanup

After the helper finishes, run:

```powershell
.\MADFilterRegister.exe /status
```

Expect `Status: NOT REGISTERED`. This reports registration status; it does not
prove which decoder the game used. If the game did not start or the helper
reported an error, keep the message and complete the cleanup step below.

## 6. Remove registration or uninstall

If the helper was closed early, the computer lost power, or status still shows
registered, close the game and run from the package folder as Administrator:

```powershell
.\MADFilterRegister.exe /unregister
.\MADFilterRegister.exe /status
```

Once status says `NOT REGISTERED` and applications using the filter are closed,
you can delete the extracted folder to uninstall it.

## 7. Report a problem

Record the game and Windows versions, the action immediately before the issue,
and whether it happened with music, radio, or NPC speech. Include the helper's
error message if there was one. Review files for personal information before
sharing them; see [SECURITY.md](SECURITY.md).

For persistent installation, developer diagnostics, and helper
scripts, see [Technicals.md](Technicals.md#registration-and-diagnostics).
Batch scripts are included. Their former self-relaunch path was removed to
avoid dropping arguments. From an elevated prompt, `/nopause` skips the final
key press; game scripts accept it before or after the quoted executable path.
They accept one executable path, not additional game command-line arguments.
Use the executable helper directly if you need those arguments.
