---
title: Troubleshooting
description: Status LED effect table, known open bugs from GitHub `bug` issues at build time, and pointers to flashing problems.
slug: troubleshooting
---

## Status LED

The firmware reports its state through the external LED on GPIO7 with six
effects:

| LED | Meaning |
| --- | --- |
| ![off solid](../assets/led/off_solid.svg) | No power / brightness = 0 |
| ![breathing at 1 s period](../assets/led/breathing_1000ms.svg) | Startup |
| ![solid](../assets/led/on_solid.svg) | Ok |
| ![blinking at 1 s period](../assets/led/blinking_1000ms.svg) | Warning |
| ![breathing at 300 ms period](../assets/led/breathing_300ms.svg) | Resetting to [factory defaults](./config.md#factory-defaults) |
| ![blinking at 200 ms period](../assets/led/blinking_200ms.svg) | Error |

> [!NOTE]
> The brightness of the LED is configurable but Startup, Resetting and Error run at full brightness regardless of the setting.

## Known open bugs

Known open bugs on [GitHub issues](https://github.com/HendrikRauh/dmx-interface/issues) and listed here below. The Table might be outdated, check the issues via the link directly for more information.

```issues
label=bug
exclude=legacy
```
