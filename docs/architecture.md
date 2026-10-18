---
title: Architecture
description: Module map, boot sequence, task topology and the data-flow of the no_std embassy firmware.
slug: architecture
---

Pure Rust `no_std` firmware for `xtensa-esp32s2-none-elf` (edition 2024),
built on esp-hal + embassy. The 128 KiB heap must live in `.dram2_uninit`
(the main DRAM segment overflows otherwise).

## File structure :badge[OUTDATED]{variant=caution}

```file-tree
src/
  main.rs — entry point
  config.rs — config model
  storage.rs — NVS storage
  logging.rs — log ring -> USB-CDC
  panic_report.rs — panic handler
  boards/
    mod.rs — re-exports
    s2_mini.rs — pin definitions
  hardware/
    led.rs — status LED task
    button.rs — button actions
    efuse.rs — MAC + AP SSID
    usb.rs — USB-CDC + auto-reset
    mod.rs
  net/
    mod.rs — WiFi bring-up
    dhcp.rs — DHCPv4 server
    http.rs — HTTP server
    ws.rs — WebSocket server
```

## Boot sequence :badge[SOON]{variant=note}

## Task topology :badge[SOON]{variant=note}

## Config data flow :badge[SOON]{variant=note}
