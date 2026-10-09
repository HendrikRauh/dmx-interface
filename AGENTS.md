# dmx-interface — AGENTS.md

Rust firmware for ESP32-S2 (Lolin S2 Mini) — DMX512 over WiFi AP with JSON config persisted to NVS.

**Current state**: no_std pure Rust with esp-hal + embassy. Phases 1–3 implemented (LED + Button, Config, NVS Storage) and USB-CDC-ACM with esptool auto-reset (Phase 4, partial) — see `TODO.md` for the remaining phases (DMX UART, System, WiFi, Web server, Integration).

Only LED, USB and the diagnostic blink are wired into `src/main.rs` today. `config`, `storage`, `boards`, `hardware::button` and `hardware::efuse` are implemented but **not used yet** (hidden by `#![allow(dead_code)]`); wiring them up is part of the open phases.

## Dev shell

```sh
nix develop                    # enter shell (direnv also works)
nix develop -c inv build       # one-shot build
```

All `inv` commands **must** run inside `nix develop` (or prefixed with `nix develop -c`), as they depend on `esptool`, `espflash`, `cargo`, and other flake-provided tools.

## Tasks (invoke)

| Command | What |
| --- | --- |
| `inv build` | Debug build → `target/xtensa-esp32s2-none-elf/debug/dmx-interface` |
| `inv build --release` | Release build (LTO, `opt-level=s`) |
| `inv flash` | Build → convert to bin → flash via esptool (USB CDC) |
| `inv monitor` | Serial monitor via espflash |
| `inv check` | Strict clippy for the xtensa target (flags in `tasks.py`) |
| `inv clean` | `cargo clean` |
| `inv format` | `pre-commit run --all-files` |
| `inv docs` | Generate rustdoc + redirect |
| `inv docspath` | Print doc output path (for CI) |
| `inv web:build` | Vite build → `web/dist/` (single HTML) |
| `inv web:dev` | Vite dev server with mock API |

## Hard constraints

- **Toolchain**: nightly esp-rs fork via `RUSTUP_TOOLCHAIN` (set in flake, sourced from `esp-rs-nix`).
- **Target**: `xtensa-esp32s2-none-elf` (no_std, pure Rust).
- **build-std**: `["alloc", "core"]` in `.cargo/config.toml` — do NOT pass `-Zbuild-std` on command line.
- **Linker**: `linkall.x` + `nostartfiles` via `.cargo/config.toml` rustflags (no `ldproxy`).
- **esp_bootloader_esp_idf::esp_app_desc!()**: required in `main.rs` even with no_std (bootloader compatibility).
- **Clippy**: works on the xtensa target (the old ICE is gone) and runs strict — `-D warnings` plus `-D clippy::pedantic` and cherry-picked restriction/nursery lints for
  no_std firmware (arithmetic overflow, undocumented `unsafe`, `as` conversions, indexing/slicing, `unwrap`, needless `&mut`, std-instead-of-core drift, etc.). Flags live
  in `tasks.py` (`_CLIPPY_ARGS`), used by `inv check` and by the pre-commit hook (`entry = "invoke check"`). Never pass `--all-targets`: the `test` crate does not exist
  for this target, so `cargo test`/`--all-targets` cannot build.

## Flake env vars

| Var | Value | Why |
| --- | --- | --- |
| `RUSTUP_TOOLCHAIN` | esp-rs path | Xtensa Rust nightly |
| `LIBCLANG_PATH` | libclang lib path | Bindgen |
| `GERMAN_DICT_PATH` | cspell de-de dict | Spell-checking |

## Architecture

| Module | Role | Status |
| --- | --- | --- |
| `src/main.rs` | Entry point, peripherals, main loop | LED + USB wired |
| `src/config.rs` | Config data model (heapless, serde, postcard) | Implemented, not wired |
| `src/storage.rs` | NVS persistent storage (esp-nvs) | Implemented, not wired |
| `src/boards/mod.rs` | Pin definitions (cfg-gated) | Implemented, not wired |
| `src/boards/s2_mini.rs` | S2 Mini pin constants | Implemented, not wired |
| `src/hardware/led.rs` | LED effect logic (LEDC PWM) | Wired |
| `src/hardware/button.rs` | Debounced button | Implemented, not wired |
| `src/hardware/efuse.rs` | MAC address helpers (esp-hal::efuse) | Implemented, not wired |
| `src/hardware/usb.rs` | USB-CDC-ACM + esptool auto-reset (DTR/RTS → bootloader) | Wired |

## Key crate versions

| Crate | Version | Why |
| --- | --- | --- |
| `heapless` | 0.7 | 0.8 incompatible with postcard (heapless type mismatch) |
| `esp-alloc` | 0.7 | Global allocator via `HEAP.add_region(HeapRegion::new(...))` — no `.init()` method, no chip features |
| `esp-nvs` | 0.5 | Takes `&Key` refs, `Key::from_str()` for const keys |
| `static_cell` | 2.1 | `StaticCell::init()` for `'static` embassy-usb buffers (edition 2024 forbids `&mut static mut`); already a transitive dep of esp-hal |

## USB CDC flashing (ESP32-S2)

Lolin S2 Mini uses native USB-OTG CDC — no external UART, no DTR/RTS pins. The firmware itself exposes the USB port as a CDC-ACM device and implements the classic DTR/RTS reset protocol in software (see `src/hardware/usb.rs`). This gives esptool **auto-reset** support via `ESPTOOL_BEFORE=usb-reset`.

**First flash** (no firmware yet): Hold **BOOT** + press **RESET** to enter the ROM bootloader manually, then run `inv flash`.

**Subsequent flashes**: firmware is already running, so `inv flash` resets into the bootloader itself — no button pressing needed.

After flashing, `inv flash` clears `RTC_CNTL_OPTION1.FORCE_DOWNLOAD_BOOT` via esptool `write-mem 0x3F408128 0 0x1` (through the stub) and resets via the RTC watchdog (`--after watchdog-reset`) — the device boots the application and re-enumerates as USB-CDC automatically. `inv monitor` (or
`inv flash`) can be re-run without pressing RESET.

## Web frontend (`web/`)

- **Stack**: Preact + TypeScript + Vite + SCSS
- **Build**: `vite build` → single HTML file (`vite-plugin-singlefile`) + gzip (`vite-plugin-compression`)
- **Dev**: `web/dev/` mock server (`vite-plugin-mock-dev-server`) — mocks `/api/config`

## Pre-commit hooks (Nix-generated, do not edit `.pre-commit-config.yaml`)

Configured in `flake.nix` via `git-hooks.nix`. Key formatters/linters:

- **Rust**: rustfmt + cargo-clippy (`inv check`: warnings as errors, pedantic, plus firmware restriction/nursery lints — full set in `tasks.py` `_CLIPPY_ARGS`, replaces cargo-check)
- **Rust docs**: `scripts/check-rust-docs.sh` (LEVEL/FILE_DOC toggles)
- **Python**: ruff + ruff-format
- **Nix**: alejandra + deadnix + statix + flake-checker
- **TS**: oxfmt + oxlint
- **CSS/SCSS**: prettier
- **Markdown**: markdownlint + mdformat + cspell
- **Shell**: shellcheck + shfmt
- **Secrets**: ripsecrets + detect-private-keys

Run `inv format` (or `pre-commit run --all-files`) to re-run all hooks.

## CI

| Workflow | Trigger | What |
| --- | --- | --- |
| `check.yml` | Push to main/idf/rust/no-std, all PRs | pre-commit lint + `invoke build` |
| `release.yml` | Tag `v*.*.*` | `invoke release` → GitHub Release with `dist/*.bin` |
| `deploy-docs.yml` | Push to main | `invoke docs` → gh-pages |

## Pitfalls (hard-won)

These are non-obvious issues encountered during development. **Read this before changing firmware or build logic.**

### Flash: `--merge` is mandatory

`espflash save-image` **must** use `--merge`. Without it, only the bare app binary is produced — **no bootloader, no partition table** — and the firmware silently never boots (LED stays off, no crash output).

```sh
# Correct — produces merged image (bootloader @ 0x1000 + part-table @ 0x8000 + app @ 0x10000):
espflash save-image --chip esp32s2 --flash-size 4mb --merge target/.../dmx-interface target/.../dmx-interface.bin

# Wrong — only app binary, no bootloader:
espflash save-image --chip esp32s2 --flash-size 4mb target/.../dmx-interface target/.../dmx-interface.bin
```

Flash the merged binary at offset `0x0000` — the internal offsets are already embedded.

### Two LEDs: GPIO7 (external) vs GPIO15 (onboard)

The Lolin S2 Mini has **two** status LEDs. The onboard LED is on **GPIO15**, not GPIO7. GPIO7 is for an external LED-Button module.

- `ONBOARD_LED_GPIO = 15` — what you see on the board
- `LED_GPIO = 7` — external module (if wired)

Use GPIO15 for debugging/verification.

### `#[esp_rtos::main]` does NOT call `esp_rtos::start()`

The macro only sets up the embassy executor. You **must** call `esp_rtos::start()` manually inside the async main:

```rust
let timg0 = TimerGroup::new(peripherals.TIMG0);
let sw_int = SoftwareInterruptControl::new(peripherals.SW_INTERRUPT);
esp_rtos::start(timg0.timer0, sw_int.software_interrupt0);
```

### `esp-alloc` HeapRegion API

`HeapRegion::new()` takes `*mut u8` (not `*mut [u8; N]`). Use a cast:

```rust
static mut HEAP_MEM: [u8; HEAP_SIZE] = [0u8; HEAP_SIZE];
unsafe {
    esp_alloc::HEAP.add_region(HeapRegion::new(
        core::ptr::addr_of_mut!(HEAP_MEM) as *mut u8,
        HEAP_SIZE,
        esp_alloc::MemoryCapability::Internal.into(),
    ));
}
```

### `InputConfig` is a struct (builder pattern)

Not `InputConfig::with_pull(...)`. Use:

```rust
Input::new(pin, InputConfig::default().with_pull(Pull::Up))
```

### USB-CDC buffers must be `'static`

The S2 USB peripheral types are lifetime-invariant (`PhantomData<&'a mut ()>`, and `UsbPeripheral` config forces `'static`), so the `embassy-usb` driver is pinned to `'static`. That means all device buffers (`EP_OUT_BUFFER`, `CONFIG_DESCRIPTOR`, `BOS_DESCRIPTOR`, `CONTROL_BUF`,
`State`) must be `'static` too. Edition 2024 forbids `&mut static mut`, so use `static_cell::StaticCell<T>` with `.init(...)` (returns `&'static mut T`,
panics on double init — safe, the task runs once):

```rust
static EP_OUT_BUFFER: StaticCell<[u8; 1024]> = StaticCell::new();
let driver = Driver::new(usb, EP_OUT_BUFFER.init([0u8; 1024]), Config::default());
```

### esptool auto-reset: DTR/RTS are simulated in software

esptool's `--before usb-reset` walk is `(DTR,RTS) = (0,0) → (1,0) → (0,1) → (0,0)`. The firmware mirrors it onto the ROM strapping semantics:

| DTR | RTS | Action |
| ----- | ----- | -------- |
| 1 | 0 | set `RTC_CNTL_OPTION1.FORCE_DOWNLOAD_BOOT` (simulates IO0 low) |
| 0 | 1 | plain `software_reset()` — boot target decided by the flag (simulates EN low) |
| 0 | 0 | clear the flag |
| 1 | 1 | no-op |

The `(0,1)` state must **not** set the force flag itself: the `--before` walk arms it via the preceding `(1,0)`. Setting the flag unconditionally was the original bug — but even the "clean" `(0,1)`-reset design leaves the device stuck after flashing, because the flag survives the
whole download session:

**The ROM bootloader does NOT clear `FORCE_DOWNLOAD_BOOT` on entry.** The flag lives in the RTC domain and survives software resets, so after esptool's `--before` walk set it, it is still set when the post-flash reset runs. With `--after hard-reset`, esptool's S2-specific
`hard_reset()` (esptool/targets/esp32s2.py) checks the flag — if set, it skips the clean watchdog reset and falls back to an RTS-only pulse that leaves the chip in the bootloader (USB PID 0x0002). The next flash then hits `OSError: [Errno 71] Protocol error` on the first modem
ioctl, because the idle ROM bootloader drops the first control request.

The fix lives in `inv flash` (`tasks.py`): after `write-flash`, it clears bit 0 via `write-mem 0x3F408128 0 0x1` through the stub and resets via `--after watchdog-reset` (register-based, ungated), which boots the application. Only a power-on reset clears the flag by hardware.

Do **not** switch the state machine to poll the CDC `control_changed()` event — it fires per-state-change and misses the bootloader-reset semantics; 10 ms polling of `dtr()`/`rts()` is deliberate.

### No debug output without UART

The Lolin S2 Mini has **no USB-UART bridge** on GPIO43/44. `esp-println` with `uart` feature produces no visible output. Use `no-op` for release or `println!` (goes to no-op) — the panic handler uses `esp-backtrace` with `println` feature (also no-op). There is no way to see
serial output unless you wire a UART adapter to
GPIO43/44.
