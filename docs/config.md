---
title: Configuration
description: Where the ChaosDMX config lives, factory defaults, factory reset, user button and how updates are applied over HTTP/WebSocket.
slug: config
---

Everything a user can change lives in one JSON `Config` (defined in
`src/config.rs`): network mode, credentials, DMX port directions, LED
brightness and button actions.

The config is stored in NVS (postcard), so it survives reboots and power loss.
Request handlers re-read it per request, and `Config::apply()` pushes every
change into the drivers. Network fields are the exception: the network stack is
configured once at boot, so those take effect on the next reboot.

## Factory defaults

<!-- Mirrors Config::default() in src/config.rs and web/mock/config.mock.ts. -->

Out of the box the device comes up as its own access point with an SSID derived
from the MAC address, so a freshly flashed board is always reachable without
prior setup. The derived name is written into the stored config when the factory
defaults are created (it is not recomputed at boot), and the config API rejects
an empty AP SSID — an AP without an SSID would be unreachable:

| Setting | Value |
| --- | --- |
| Connection | WiFi AP |
| SSID | `ChaosDMX-□□□□` (last 4 hex digits of MAC) |
| Password | `ChaosDMX` |
| IP address | 192.168.4.1 |
| DMX0 | Output, Universe 1 |
| DMX1 | Input, Universe 2 |
| LED brightness | 25 (0–255, ≈ 10 %) |
| Button: single click | switch [LED](./troubleshooting.md#status-led) brightness |
| Button: triple click | Reboot |

## Factory reset

Hold the user button while powering the device for 3 seconds: the LED breathes
fast (Resetting), settings are cleared and the device reboots with factory
defaults. Releasing early aborts (Warning blink) and keeps the config. Because
it runs at boot, the reset works without network or UI.

A factory reset is also available at runtime via the WebSocket `reset`
envelope (not yet exposed in the UI).

## User button

The external button has multiple functions that can be configured in the web-ui.

Possible events are:

- single click
- double click
- triple click
- long press (≥ 1 s)

You can assign any of these events to a function:

- switch the LED brightness (0–255)
- reboot the device

<!-- TODO: add more actions -->

> [!NOTE]
> Regardless of the configuration, [factory reset](./config.md#factory-reset) is always available by holding the button down while powering the device for 3 s.
