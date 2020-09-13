# GE Patch Plugin

This is an experimental plugin for [Adrenaline](https://github.com/TheOfficialFloW/Adrenaline) that allows you to play a few games in native resolution.

## Compatibility List

Please help testing games and filling out the [spreadsheet](https://docs.google.com/spreadsheets/d/1aZlmKwELcdpCb9ezI5iRfgcX9hoGxgL4tNC-673aKqk/edit#gid=0).

## Changelog v0.14

- Fixed another issue that causes games to show black screen only.

## Changelog v0.13

- Fixed issue where some games would render a black screen only.
- Fixed issue where some games would crash because vertices were updated multiple times.

## Changelog v0.12

- Fixed issue where some games would be inverted or upsidedown.

## Changelog v0.11

- Fixed issue where black rectangles would cover the screen in lots of games.

## Installation

- Before you start, make sure you have Adrenaline 6.9 or higher and disable all other plugins in `ux0:pspemu/seplugins/game.txt` (remove all lines or set to 0).

- Download [ge_patch.prx](https://github.com/TheOfficialFloW/GePatch/releases).

- Copy it to `ux0:pspemu/seplugins/`.

- Write this line to `ux0:pspemu/seplugins/game.txt`:

  ```
  ms0:/seplugins/ge_patch.prx 1
  ```

## Known Issues

Some games may:

- Not display cutscenes.
- Have a black screen.
- Not display all textures.

## Donation

If you like my work and want to support future projects, you can make a donation:

- via bitcoin `361jRJtjppd2iyaAhBGjf9GUCWnunxtZ49`
- via [paypal](https://www.paypal.me/flowsupport/20)
- via [patreon](https://www.patreon.com/TheOfficialFloW)
