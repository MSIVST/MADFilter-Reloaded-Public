# Automatic game settings

## 1. Make the filter available

Start with [HOW_TO_USE.md](HOW_TO_USE.md). Registering the filter makes it
available to compatible applications. It does not prove that a particular
game has chosen it.

## 2. Let the filter recognize the game

After an application loads the filter, it checks the program's filename:

| Program | Automatic game settings |
|---|---|
| `Fallout3.exe` | Enabled |
| `FalloutNV.exe` | Enabled |
| `Oblivion.exe` | Enabled |
| Other names | Ordinary playback settings |

Letter case does not matter. Renaming a game program can prevent recognition.
Recognition selects playback settings; it does not verify the game version or
guarantee compatibility. No manual override is needed for the listed names.

## 3. Understand the limits

The game settings adjust how audio is continued and timed. They do not change
the game's radio volume, detect conversations, inspect game archives, or decode
Ogg audio. Other compatible applications can still use the registered filter.

For the exact behavior and developer overrides, read
[Technicals.md](Technicals.md#host-policy).
