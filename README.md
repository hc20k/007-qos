# 007-qos
some hacks for 007: Quantum of Solace (2008) (Singleplayer)

Supported versions:
* `JB 1.0 build 12795 ACOLE Wed Aug 13 14:15:57 2008 win-x86`

GSC menus!!!
![image](assets/showcase.png)

* Supports rawfile overriding (script loading)
* Dump rawfiles with `dumpraw` command. (Dumps to `<game dir>/dump`)
* Load zone with `loadzone` command
* Unlocked dev console. Commands also work in the `qol` console.
* Forced windowed mode

## Usage
1. Install polyhook2 (x86) with vcpkg.
2. Compile with VS2022, make sure to copy the PolyHook_2.dll, Zydis.dll, asmtk.dll, and asmjit.dll to the game directory. (This will probably change in the future.)
3. Copy the output dll `d3d9.dll` from the `build` dir to the game directory
3. Create a folder named `filesystem` in the game directory. This is where you should store the rawfiles you want to override.
4. Have fun.


## Commands
* `dumpraw <filename>`: Dumps a rawfile to `<game dir>/dump`
* `loadzone <zone>`: Loads a zone. Example: `loadzone siena`

## Notes
* The code is kinda messy, but it works.
* Why did I decide to do this? I don't know. I am married to the IW engine.
* You can get the game from [here](https://www.myabandonware.com/game/007-quantum-of-solace-ev4) since it's abandonware.

**TODO:**
* Uncap FPS
* Fix `dvar_s` struct
* maybe the same thing for 007: Legends if i'm feeling sadistic enough