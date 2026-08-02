
# re-plants-vs-zombies

A project focused on decompiling the latest functionality from the first PvZ title and expand upon the game and its engine

The SexyAppFramework dating as back as 2005 is a very old game engine and it does not follow proper C++ conventions as per modern standards nor does it use a modern renderer backend

This project aims to modernize the engine by using features from the latest C++ standards aswell as replacing the old legacy DirectDraw and Direct3D7 renderers for the modern [GLFW](https://www.glfw.org/) cross-platform wrapper aswell as expanding upon an old (now deleted) decompilation project of PvZ version 0.9.9 by [Miya aka Kopie](https://github.com/rspforhp) to get the best possible PvZ experience both for modders and players alike

# DISCLAIMER

This project does not condone piracy

This project does not include any IP from PopCap outside of their open source game engine, this will only output the executable for a decompiled, fan version of PvZ

To play the game using this project you need to have access to the original game files by [purchasing it](https://store.steampowered.com/app/3590/Plants_vs_Zombies_GOTY_Edition/)

## Roadmap

#### Currently focused on
- [x] Add x64 support for the base game **(Partial)**
- [ ] Replace the old renderer backend for GLFW **(WIP)**
- [ ] Replace all Windows only code for cross-platform GLFW counterparts **(WIP)**

#### Left for when we have a working x64 build using GLFW
- [ ] Add all functionality from the GOTY version of the game
  - [x] Achievements **(Partial)**
  - [ ] Zombatar

#### Possible future features
- [ ] Create an easy to use modding API for the game
  - [ ] Parse zombies from files
  - [ ] Parse plants from files
  - [ ] Parse maps from files
  - [ ] Add scripting for custom sequences

## Native portability renovation

The native port is being developed on the `port` branch. Its architecture,
phases, dependency rules, data-layout policy, and validation gates are defined
in [docs/PORTING_PLAN.md](docs/PORTING_PLAN.md).

The portable targets are intentionally independent from the reconstructed
Win32 executable. On Windows, the legacy target remains enabled by default as a
behavioral reference. On other platforms, only the portable engine, game, tools,
and tests are enabled.

Configure, build, and test the portable targets:

```sh
cmake -S . -B out/portable -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPVZ_BUILD_PORTABLE_ENGINE=ON \
  -DPVZ_BUILD_LEGACY_WINDOWS=OFF
cmake --build out/portable
ctest --test-dir out/portable --output-on-failure
```

Run the deterministic headless Adventure replay and state-hash contract:

```sh
./out/portable/game/pvz_game_headless
```

The versioned replay format and transcript-hash policy are documented in
[docs/REPLAY_FORMAT.md](docs/REPLAY_FORMAT.md).

Record a gameplay session from the macOS application:

```sh
PVZ_RECORD_SESSION_PATH=/absolute/path/session.pvzc \
  ./script/build_and_run.sh
```

After quitting the game, inspect one capture or compare two captures:

```sh
./build/macos/engine/pvz_replay_inspect session.pvzc
./build/macos/engine/pvz_replay_inspect first.pvzc second.pvzc
```

The reconstructed Windows reference can record its logical 100 Hz input when
the portable targets are included in the Windows build:

```bat
path\to\LawnProject.exe -nosound ^
  -recordreplay="C:\captures\legacy-input.pvzr"
```

For behavior-level differential testing, record input and normalized
post-update observations together:

```bat
path\to\LawnProject.exe -nosound ^
  -referencefreshprofile=Codex ^
  -recordbehavior="C:\captures\legacy-behavior.pvzb"
```

The output path must not already exist. After exiting the Windows game
normally, replay either capture through the portable game:

```sh
./out/portable/engine/pvz_replay_inspect legacy-input.pvzr
./out/portable/game/pvz_game_headless \
  --replay legacy-behavior.pvzb \
  --write-behavior portable-behavior.pvzb
./out/portable/parity/pvz_behavior_inspect \
  legacy-behavior.pvzb portable-behavior.pvzb
```

The exact evidence boundary and comparison workflow are documented in
[docs/WINDOWS_REFERENCE_CAPTURE.md](docs/WINDOWS_REFERENCE_CAPTURE.md).

Validate migrated game behavior against independent legacy reference fixtures:

```sh
./script/validate_parity.sh
```

Fixture provenance, evidence layers, and the reference-first update policy are
documented in
[docs/PARITY_VALIDATION.md](docs/PARITY_VALIDATION.md).

Validate a user-owned PopCap PAK without extracting it:

```sh
./out/portable/engine/pvz_pak_inspect /path/to/main.pak
```

Validate every source XML and reanimation definition in the PAK:

```sh
./out/portable/engine/pvz_pak_inspect \
  /path/to/main.pak --validate-xml
```

Validate every encoded retail image or every bitmap-font descriptor and atlas:

```sh
./out/portable/engine/pvz_pak_inspect \
  /path/to/main.pak --validate-images
./out/portable/engine/pvz_pak_inspect \
  /path/to/main.pak --validate-fonts
```

Validate the portable sound manifest and decode every shipped OGG effect:

```sh
./out/portable/engine/pvz_pak_inspect \
  /path/to/main.pak --validate-sounds
```

MO3 module music support uses the BSD-licensed libopenmpt backend. On macOS,
install it with Homebrew and configure with
`-DPVZ_BUILD_MODULE_MUSIC=ON` (the native application enables it by default):

```sh
brew install libopenmpt
```

Validate every shipped MO3 file, including order/row seeking, channel muting,
looping, and float-stereo rendering:

```sh
./out/portable/engine/pvz_pak_inspect \
  /path/to/main.pak --validate-music
```

Original game data is not part of this repository and must not be committed.

### Current native interaction slice

The macOS application now provides a portable, input-driven path from the
title screen to a main menu and then to the first daytime lawn scene. Adventure
is the currently enabled mode; Minigames, Puzzle, and Survival remain visible
but report that their gameplay is still being ported. This is an engine and
early gameplay-flow milestone, not yet a complete Plants vs. Zombies level.

- Press Enter, Space, or click to leave the title screen.
- Use the arrow keys or pointer to select a menu item.
- Select Adventure to enter the daytime lawn.
- Level 1 starts with the legacy 150 sun and one 100-sun Peashooter packet.
  Click the packet, then click an empty cell in the center lawn row. Keyboard
  control uses Space/Enter once to select the packet and again to plant at the
  arrow-key focus. Dirt rows and occupied cells reject placement.
- A planted packet enters the legacy 750-tick recharge cycle. With retail data
  mounted, occupied cells render animated Peashooters from
  `PeaShooterSingle.reanim`; headless or incomplete resource sets retain the
  colored fallback marker.
- After the first plant, collect two falling 25-sun pickups to fund a second
  Peashooter. Planting it starts the source-audited 99-tick tutorial countdown
  and the first normal zombie. Peashooters then acquire the center lane, fire
  peas, apply damage, and can clear all four Level 1 waves. The lawn mower,
  loss state, and final level award are implemented. Complete visual parity
  and the remaining game modes are not ported yet.
- Press Escape on the lawn to return to the menu.

## Installation

### Visual Studio Community

Open the folder containing the `CMakeSettings.json`, wait until cache finishes generating and build the project

### Other (Sublime, Visual Studio Code, etc..)

Run the following commands (assuming you have CMake installed with Ninja) where the `CMakeSettings.json` file is located

`cmake -G Ninja -B cmake-build`

`cmake --build cmake-build`

If running these commands does not create a successful build please [create an issue](https://github.com/Patoke/re-plants-vs-zombies/issue) and detail your problem

After you build, the output executable should be in the `Debug` or `Release` (depending on your build target) folder inside `SexyAppFramework`

Then you want to copy that executable inside of the original game's root folder (or copy the contents of the original game folder inside the previously mentioned folder)

After that you should be able to just open the built executable and enjoy re-pvz!

## Contributing

When contributing please follow the following guides:

<details><summary>SexyAppFramework coding philosophy</summary>

#### From the SexyAppFramework docs:

<br>
The framework differs from many other APIs in that some class properties are not wrapped in accessor methods, but rather are made to be accessed directly through public member data.   The window caption of your application, for example, is set by assigning a value to the std::string mTitle in the application object before the application’s window is created.  We felt that in many cases this reduced the code required to implement a class.  Also of note is the prefix notation used on variables: “m” denotes a class member, “the” denotes a parameter passed to a method or function, and “a” denotes a local variable.
</br>
</details>

<details><summary>Contributor markings</summary>

<br>
Whenever you need to leave a comment for other developers to find you should do so with the following grammar:

* Always include the name of the contributor as in:
  * `@Contributor`
* For todos include the todo marking as in:
  * `@Contributor todo`
* Always add a colon to specify that the start of the comment starts there
  * `@Contributor todo: Thing went wrong!`
* If a new function has been reversed and you have found the address in the latest version of the game (or have reversed a certain class member offset) please note it as follows:
  * `@Contributor GOTY: 0xADDRESS`
</br>
</details>


## Thanks to

- [@rspforhp](https://www.github.com/octokatherine) for their amazing work decompiling the 0.9.9 version of PvZ
- [@ruslan831](https://github.com/ruslan831) for archiving the [0.9.9 decompilation of PvZ](https://github.com/ruslan831/PlantsVsZombies-decompilation)
- The GLFW team for their amazing work
- PopCap for creating the amazing PvZ franchise (and making their game engine public)
- All the contributors which have worked or are actively working in this amazing project
