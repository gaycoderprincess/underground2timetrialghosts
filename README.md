# Underground 2 Time Trial Ghosts

A mod for Need for Speed: Underground 2 that replaces Quick Race opponents with TrackMania-like replays

## Installation

- Make sure you have v1.2 of the game, as this is the only version this plugin is compatible with. (exe size of 4800512 bytes)
- Plop the files into your game folder.
- Start the game and launch any race, you will now have a ghost to race against instead of AI opponents.
- Press F5 to open the mod's menu and play the included Challenge Series.
- Enjoy, nya~ :3

## Features

- A built-in set of events in the Challenge Series to compete with other players on
- Ghosts include a checksum of the player's game files, allowing you to check if someone's made any potentially unfair modifications
- Ghosts include player inputs, and while they're not directly usable for playback (the game is not deterministic) it can still be used to verify legitimacy
- The mod includes a basic anti-cheat, preventing common methods of cheating

## Building

Building is done on an Arch Linux system with CLion and vcpkg being used for the build process. 

Before you begin, clone [nya-common](https://github.com/gaycoderprincess/nya-common), [nya-common-nfsug2](https://github.com/gaycoderprincess/nya-common-nfsug2) and [CwoeeMenuLib](https://github.com/gaycoderprincess/CwoeeMenuLib) to folders next to this one, so they can be found.

Required packages: `mingw-w64-gcc`

You should be able to build the project now in CLion.
