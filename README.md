# GE Patch Plugin

This is an experimental plugin for [Adrenaline](https://github.com/TheOfficialFloW/Adrenaline) that allows you to play a few games in native resolution.

## Compatibility List

Please help testing games and filling out the [spreadsheet](https://docs.google.com/spreadsheets/d/1aZlmKwELcdpCb9ezI5iRfgcX9hoGxgL4tNC-673aKqk/edit#gid=0).

## Changelog v0.2

- Changed framebuffer copy algorithm.
- Changed behavior of sync opcode.
- Disabled forced dithering again.

## Changelog v0.19.1

- Removed optimization introduced earlier since it's not working.
- Forced dithering on.

## Changelog v0.19

- Fixed a small bug that was introduced earlier.
- Fixed a few bugs that caused certain games to crash.

## Changelog v0.18.1

**This must be used with Adrenaline-7, not Adrenaline-6.9!**

- Fixed bug that enables more games to render without smear.

## Changelog v0.18

**This must be used with Adrenaline-7, not Adrenaline-6.9!**

- Changed fake vram address to allow more games to work.
- Added patch to allow games to use more memory of fake vram to store textures.
- Added optimization to prevent double patching of vertices. May increase performance in some games and prevent overzoomed textures.

## Changelog v0.17.1

- Fixed indexed draws which caused some games to render at 480x272 only.

## Changelog v0.17

- Fixed artifacts, flickering and black screens in some games.
- Fixed some regressions introduced in earlier versions.

## Changelog v0.16

- Added behavior of signal commands.
- Optimized draws to ignored framebuffers.

## Changelog v0.15

- Switched to using dfs algorithm to traverse the display list.
- Fixed a few commands and changed their stopping criteras.
- Added indexed draws support.

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

- Before you start make sure that you have

  - Adrenaline 7 or higher.
  - The option `Recovery Menu->Advanced->Advanced configuration->Force high memory layout` **DISABLED**.
  - All plugins in `ux0:pspemu/seplugins/game.txt` and `ux0:pspemu/seplugins/vsh.txt` disabled (you can gradually enable them if you think they should not interfere with GePatch. Please be aware that plugins that print stuff to the screen may not be visible with GePatch since the framebuffer is redirected.

- Download [ge_patch.prx](https://github.com/TheOfficialFloW/GePatch/releases) and copy it to `ux0:pspemu/seplugins/`.

- Write this line to `ux0:pspemu/seplugins/game.txt` (`ux0:pspemu` is mounted as `ms0:` in the PSP emu):

  ```
  ms0:/seplugins/ge_patch.prx 1
  ```

  You can also do the same change in file `ux0:pspemu/seplugins/vsh.txt` to get a XMB in higher resolution, but be aware that the VSH menu will be invisible.

## Known Issues

Some games may:

- Not display cutscenes.
- Have a black screen.
- Not display all textures.
- Contain clipping/culling.

## Donation

If you like my work and want to support future projects, you can make a donation:

- via bitcoin `361jRJtjppd2iyaAhBGjf9GUCWnunxtZ49`
- via [paypal](https://www.paypal.me/flowsupport/20)
- via [patreon](https://www.patreon.com/TheOfficialFloW)
