# GE Patch Plugin

This is an experimental plugin for [Adrenaline](https://github.com/TheOfficialFloW/Adrenaline) that allows you to play a few games in native resolution.

## Compatibility List

Please help testing games and filling out the [spreadsheet](https://docs.google.com/spreadsheets/d/1aZlmKwELcdpCb9ezI5iRfgcX9hoGxgL4tNC-673aKqk/edit#gid=0).

## Installation

- Before you start, make sure you have Adrenaline 6.9 or higher and disable all other plugins in `ux0:pspemu/seplugins/game.txt` (remove all lines or set to 0).

- Download [ge_patch.prx](https://github.com/TheOfficialFloW/GePatch/releases/download/v0.11/ge_patch.prx).

- Copy it to `ux0:pspemu/seplugins/`.

- Write this line to `ux0:pspemu/seplugins/game.txt`:

  ```
  ms0:/seplugins/ge_patch.prx 1
  ```

## Known Issues

Some games may:

- Not display cutscenes.
- Have wrong color format / line width.
