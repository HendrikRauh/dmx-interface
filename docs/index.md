---
title: ChaosDMX
description: Open-source WiFi DMX512 interface on an ESP32-S2 — firmware, web UI, hardware and development docs.
template: splash
hero:
  actions:
    - text: Getting started
      link: /getting-started/
      variant: primary
    - text: Development
      link: /development/
      variant: minimal
---

ChaosDMX is an open-source DIY [DMX512](https://en.wikipedia.org/wiki/DMX512)
interface on an ESP32-S2 (Lolin S2 Mini). It is meant to bridge
lighting-control software (e.g. [QLC+](https://www.qlcplus.org/)) to stage
equipment — fixtures, moving heads, fog machines — over WiFi. The firmware
serves the web UI, stores the settings in NVS and joins your network in
access-point or station mode; no companion app or cloud service is involved.

> [!WARNING]
> This is a rewrite in rust of our first version which used the Arduino Framework. The legacy
> Arduino-based firmware lives in the
> [legacy/arduino branch](https://github.com/HendrikRauh/dmx-interface/tree/legacy/arduino) and will not be maintained by us anymore. If you have a bugfix feel free to submit a PR.
>
> Have a look at the [roadmap](./roadmap.md) to see what is planned and what is already working in this rewrite.

Feel free to [report issues](https://github.com/HendrikRauh/dmx-interface/issues) or submit a pull request. We are happy to receive contributions.
