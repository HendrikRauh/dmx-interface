---
title: Flashing
description: Flash the ChaosDMX firmware via web flasher or inv flash — first flash, auto-reset and troubleshooting.
slug: flashing
---

## Web flasher

The [web flasher](https://hendrikrauh.github.io/dmx-interface/flasher/) is the
fastest route onto the board. Open it in a browser that supports web serial, select your board and version and click **"Flash"**.

> [!TIP]
> You see in the web flasher itself if your browser supports web serial. If it does, you can flash the firmware without installing any tools via USB. If you don't have a browser that supports web serial, you can use the command line flasher below.

## First flash

Without firmware on the board nothing handles the auto-reset handshake, so
enter the ROM bootloader manually: hold **BOOT**, press **RESET**, release
**BOOT**, then start the flash (web flasher or `inv flash`). This is the only
manual button sequence; every later flash auto-resets without buttons
(the firmware drives the DTR/RTS handshake esptool expects).

> [!CAUTION]
> Always flash a merged image, never a bare app binary: a bare image has
> no bootloader or partition table and the board silently never boots.

## Command line

During [development](./development.md) you flash from the repo:

```sh
nix develop
inv flash        # build (if needed) → convert → flash via esptool
```
