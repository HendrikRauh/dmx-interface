# TODO

## Target: no_std pure Rust (esp-hal + embassy)

> ✅ means implemented and compiling, **not** necessarily wired into `main.rs`.
> Still unused: `boards`, `hardware::button`, `hardware::efuse`.
> Partially wired: `config` + `storage` (boot-time `storage::load().apply()`, panic snapshot).

## Phase 1 — LED + Button ✅

- [x] LED breathing/blink/solid via LEDC PWM (GPIO7)
- [x] Button debounce via GPIO5
- [x] Extract LED effects into `src/hardware/led.rs`
- [x] Extract debounce into `src/hardware/button.rs`
- [x] LED as embassy task with status enum (`LedStatus`, timings from `assets/led/*.svg` + old IDF firmware)
- [x] `tasks.py` für no_std target

## Phase 2 — Config ✅

- [x] Config data model (`heapless` types, `serde`)
- [x] MAC-Address helper (`esp-hal::efuse`)

## Phase 3 — Storage ✅

- [x] NVS persistent storage (`esp-nvs`)
- [x] Config load/save/clear

## Phase 4 — DMX

- [x] USB-CDC-ACM device (`src/hardware/usb.rs`) with esptool auto-reset
- [ ] DMX512 UART output (`esp-hal::uart`)
- [ ] Break signal timing (250kbaud → 100kbaud → 250kbaud)
- [ ] DE/RE GPIO direction control (RS-485)

## Phase 5 — System

- [x] Logging over USB-CDC (`src/logging.rs` → ring → `inv monitor`)
- [x] Panic report + persistence (RTC + NVS, auto-reset, boot-loop guard)
- [ ] System info (heap, uptime, reset reason, chip info)
- [ ] Factory reset via button hold (3s)
- [ ] Boot-time button detection

## Phase 6 — WiFi

- [ ] WiFi AP mode (`esp-radio` + `embassy-net`)
- [ ] WiFi STA mode with reconnection + fallback to AP
- [ ] DHCP
- [ ] Switch LED to `LedStatus::Ok` once the network is ready (currently set right after boot)

## Phase 7 — Web Server

- [ ] Hand-rolled HTTP server on `embassy-net` TCP
- [ ] REST API: `GET/POST /api/config`, `POST /api/reset`, `GET /api/system`
- [ ] Static file serving (`include_bytes!` for web UI)

## Phase 8 — Integration

- [ ] Embassy task architecture (WiFi, web, DMX, main loop)
- [ ] Config change propagation (web → NVS → WiFi/DMX)
- [ ] OTA update support (optional)

## Architecture

```text
src/
├── main.rs              ← entry point, peripheral init, task spawns
├── config.rs            ← Config data model (heapless, serde, postcard)
├── storage.rs           ← NVS persistent storage (esp-nvs)
├── boards/
│   ├── mod.rs           ← cfg-gated board re-exports
│   └── s2_mini.rs       ← pin definitions (Lolin S2 Mini)
└── hardware/
    ├── led.rs           ← status LED task (`LedStatus` effects, LEDC PWM)
    ├── button.rs        ← debounced button
    ├── efuse.rs         ← MAC address helpers (esp-hal::efuse)
    ├── usb.rs           ← USB-CDC-ACM + esptool auto-reset (DTR/RTS → bootloader)
    └── mod.rs
```

## Documentation

100% coverage for all modules, including examples and usage instructions.
