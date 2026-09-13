NMRIH Trainer
=============
Internal trainer (DLL) + injector for the game "No More Room in Hell" (Source engine, 32-bit).

Contents
--------
  build\NMRIHCheat.dll   the trainer DLL (prebuilt, x86)
  build\loader.exe       injector - finds nmrih.exe and injects the DLL
  build\dumper.dll       netvar/interface dumper tool (for developers)
  src\                   full source (cheat, dumper, loader)
  external\imgui-docking Dear ImGui (docking branch) required to rebuild
  build.bat              builds dumper.dll + loader.exe
  build_cheat.bat        builds NMRIHCheat.dll

Usage
-----
  1. Start No More Room in Hell and join/start a game.
  2. Run loader.exe (keep it in the same folder as NMRIHCheat.dll,
     or pass a path:  loader.exe C:\path\to\NMRIHCheat.dll).
  3. In-game controls:
       INSERT   toggle the menu
       END      unload the cheat cleanly
     Keybinds and all options are configured in the menu
     (Aimbot / ESP / Misc / Exploit tabs, EN + Chinese UI).

Features
--------
  - ESP: boxes, names, distance, HP bars for teammates, infected,
    zombies, items and objectives; brightness overlay
  - Aimbot with FOV circle, smooth, target players and/or zombies
  - Misc: bhop, speed multiplier, HUD (own HP/stamina/speed/FOV)
  - Exploit: sv_cheats bypass (client-side gate bypass)

Notes
-----
  - Offsets are for the current nmrih.exe/client.dll build. If the game
    updates, some features may stop working until offsets are updated.
  - Server-authoritative features (god mode, infinite ammo, speed)
    only work when you host the server yourself (listen server).
    On dedicated servers the server overwrites client writes.
  - Anti-cheat: none, but use at your own risk.

Rebuilding from source
----------------------
  Requirements: Visual Studio 2022 or newer with the "Desktop
  development with C++" workload, and a Windows 10/11 SDK.
  The build scripts auto-detect the toolchain (override with the
  VS_DIR / MSVC_DIR / WINSDK_DIR environment variables if needed).

  Build everything:
      build.bat            (dumper.dll, loader.exe)
      build_cheat.bat      (NMRIHCheat.dll)

  Build outputs go to the build\ folder next to the scripts.

License
-------
  Dear ImGui (external\) is licensed under the MIT license,
  see external\imgui-docking\LICENSE.txt.
  This trainer is provided for educational purposes only.