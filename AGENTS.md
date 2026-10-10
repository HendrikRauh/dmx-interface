# dmx-interface — AGENTS.md

<!-- cspell:ignore xtal libstdc libexpat libz libvtk vtkmodules zlib expat cadquery occt manylinux patchelf soname PYTHONPATH build123d ocp_vscode vscode uv python313 libPrefix virtualenv sdist pyproject novtk unpatchelf dont IGES -->

Rust firmware for ESP32-S2 (Lolin S2 Mini) — DMX512 over WiFi AP with JSON config persisted to NVS.

**Current state**: no_std pure Rust with esp-hal + embassy. Phases 1–3 implemented (LED + Button, Config, NVS Storage) and USB-CDC-ACM with esptool auto-reset (Phase 4, partial) — see `TODO.md` for the remaining phases (DMX UART, System, WiFi, Web server, Integration).

Wired into `src/main.rs` today: the status LED (own embassy task, `hardware::led`), USB-CDC, logging/panic report, and boot-time config load (`storage::load().apply()`).
`boards`, `hardware::button` and `hardware::efuse` are implemented but **not used yet** (hidden by `#![allow(dead_code)]`); wiring them up is part of the open phases.

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
| `inv monitor` | Raw serial monitor, reconnecting, colored levels (in `tasks.py`) |
| `inv check` | Strict clippy for the xtensa target (flags in `tasks.py`) |
| `inv clean` | `cargo clean` |
| `inv format` | `pre-commit run --all-files` |
| `inv docs` | Generate rustdoc + redirect — **fails on any rustdoc warning** (`RUSTDOCFLAGS=-D warnings` in `tasks.py`) |
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
| `src/main.rs` | Entry point, peripheral init, task spawns | LED task + USB + logging wired |
| `src/config.rs` | Config data model (heapless, serde, postcard), `apply()` propagation | `apply()` wired (boot load → LED brightness), rest not yet |
| `src/storage.rs` | NVS persistent storage (esp-nvs), global singleton + panic snapshot keys | `init()` + boot-time `load()` wired, `save()`/`clear()` not yet used |
| `src/logging.rs` | `log` backend → static 4 KiB ring → CDC flush API | Wired |
| `src/panic_report.rs` | Own `#[panic_handler]`, RTC/NVS persist, boot-loop guard, replay | Wired |
| `src/boards/mod.rs` | Pin definitions (cfg-gated) | Implemented, not wired |
| `src/boards/s2_mini.rs` | S2 Mini pin constants | Implemented, not wired |
| `src/hardware/led.rs` | Status LED: `LedStatus` enum + embassy task (LEDC PWM, 14-bit raw duty via `set_duty_hw`) | Wired |
| `src/hardware/button.rs` | Debounced button | Implemented, not wired |
| `src/hardware/efuse.rs` | MAC address helpers (esp-hal::efuse) | Implemented, not wired |
| `src/hardware/usb.rs` | USB-CDC-ACM + esptool auto-reset (DTR/RTS → bootloader) + log drain | Wired |

## Key crate versions

| Crate | Version | Why |
| --- | --- | --- |
| `heapless` | 0.7 | 0.8 incompatible with postcard (heapless type mismatch) |
| `libm` | 0.2 | `sinf` for the LED breathing curve — `no_std` core has no float math; used by `hardware::led` |
| `esp-alloc` | 0.7 | Global allocator via `HEAP.add_region(HeapRegion::new(...))` — no `.init()` method, no chip features |
| `esp-nvs` | 0.5 | Takes `&Key` refs, `Key::from_str()` for const keys |
| `static_cell` | 2.1 | `StaticCell::init()` for `'static` embassy-usb buffers (edition 2024 forbids `&mut static mut`); already a transitive dep of esp-hal |
| `log` | 0.4 | Logging facade; backend in `src/logging.rs` uses `set_logger_racy`/`set_max_level_racy` in a critical section (Xtensa has no CAS, so the plain setters are cfg'd out) |
| `esp-backtrace` | 0.19 | `Backtrace::capture()` only — `panic-handler` feature **off** (own handler in `panic_report.rs`); needs `println` feature to satisfy `build.rs` |

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
- **Rust docs**: rustdoc (`invoke docs`, fails on any rustdoc warning) + `scripts/check-rust-docs.sh` (LEVEL/FILE_DOC toggles)
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

### Module docs: `//!` only — never also `///` on `mod x;`

A module documented **both** with `///` on its declaration (e.g. in `main.rs`)
**and** with `//!` at the top of its file gets its `//!` docs resolved by
rustdoc in the **parent module's scope**. Every unqualified intra-doc link then
breaks (a link like `[task]` reports "no item named … in scope") and links to
private children cannot resolve at all (`[dhcp]`, `[MAX_PANICS]`); absolute
public paths like `crate::net::task` still work. Clippy's `doc_markdown` also
silently stops firing on the `//!` text while the `///` duplicate exists — the
problem only shows once the duplicate is removed. Reproduced on stable rustdoc
1.98 and the esp nightly alike.

So: declare modules bare in `main.rs` / `boards/mod.rs`, module docs live only
as `//!` in the module file. `inv docs` (and the `rustdoc` pre-commit hook /
CI) builds docs with `RUSTDOCFLAGS=-D warnings` (`tasks.py`), so broken links
fail instead of scrolling by.

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

The Lolin S2 Mini has **two** LEDs. The firmware drives the **external** LED-Button module on **GPIO7** (`hardware::led`); the onboard LED sits on **GPIO15** (active-low, `ONBOARD_LED_GPIO` in `boards/s2_mini.rs`) and is **not driven** — it stays dark, so don't use it to verify firmware state.
Debug output goes over USB-CDC (`inv monitor`).

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

**esptool's CLI takes exactly one operation per invocation** (argparse in v4, click in v5) — chaining `write-flash 0x0 file.bin write-mem 0x3F408128 0 0x1` dies with `Invalid value for '<address> <filename>...': Address "write-mem" must be a number`
on every version; the old one-liner in `tasks.py` never parsed. `tasks.py` therefore runs two invocations:
the flash run ends with `--after no-reset-stub` (chip stays in the stub, no reset), the second reconnects with `--before no-reset` (a walk would re-arm the flag), clears bit 0, does `--after watchdog-reset` and runs `--silent` (banner/chip block are noise; the progress bar lives in run 1).
Both runs set `ESPTOOL_OPEN_PORT_ATTEMPTS=5` so an Errno 71 on the port open is retried instead of aborting. `inv flash` picks the port by scanning `/sys/bus/usb/devices` for Espressif USB devices (VID `303a` — ROM bootloader `0002` and firmware `3001` both match; `--port` overrides):

- exactly one → use it
- none → fail fast before build/conversion/esptool and print the first-flash BOOT+RESET hint
- several → list them and require `--port`

The port may re-enumerate between the two runs (reset while the stub was resident — port-close glitch or button press): `inv flash` waits for the node and skips run 2 if the firmware (PID `3001`) came back, since an app boot implies the flag is already clear.

The hint also prints when the connect fails.

`inv monitor` resolves the port the same way (single device only, no first-flash hint), waiting up to 8 s for the device to (re-)enumerate after a flash, and streams through pyserial inside the task.
Do **not** point espflash's `monitor --before no-reset-no-sync` at the running app instead: its connect path queries chip registers over SLIP (`detect_sdm`, `device_info`, `xtal_frequency`),
which an application never answers — the command dies with an I/O error after ~19 s (the flags only skip the reset, not the queries; the default connect dance would reset the app anyway).

Do **not** switch the state machine to poll the CDC `control_changed()` event — it fires per-state-change and misses the bootloader-reset semantics; 10 ms polling of `dtr()`/`rts()` is deliberate.

### Debug output: log over USB-CDC, not UART

The Lolin S2 Mini has **no USB-UART bridge** on GPIO43/44 — raw `esp-println` (kept at `no-op` feature only to satisfy esp-backtrace's `build.rs`) produces no output.
All diagnostics go through `src/logging.rs`: the `log` crate backend writes `[<ms>:>7ms][<L>] <msg>` lines into a static 4 KiB ring (drop-oldest).
`control_and_log` in `src/hardware/usb.rs` drains the ring to the CDC-ACM class — visible in `inv monitor` (raw passthrough in `tasks.py`, which reconnects across panic resets).

**Panics**: `src/panic_report.rs` owns the `#[panic_handler]` (esp-backtrace's `panic-handler` feature is _disabled_; the crate stays a dep only for `Backtrace::capture()` + `println` feature).
It logs Panic + Location + backtrace frames to the ring, then persists a snapshot twice — `.rtc_slow.persistent` RTC section (survives soft reset, zeroed only on power-on) and NVS `diag/last_panic` (if flash is idle, guarded against a concurrent writer).
It clears the RTC boot-loop counter (`mark_stable()` arms it via a 30 s task in `main.rs`), resets via `esp_hal::system::software_reset()`, and halts instead after **3 panics without a stable 30 s run**.
On the next boot `report_last_panic()` replays the report into the ring (`=== LAST PANIC (previous run) ===`) — it shows up in `inv monitor` after reconnect; there is no live panic output (the executor owns the USB pipeline).
Symbolize the `0x…` frame addresses with `addr2line -e target/xtensa-esp32s2-none-elf/debug/dmx-interface`.

Snapshot details (hard-won): the replay is verbatim (`push_raw`) — re-splitting the stored bytes on `\\n` and pushing the pieces without them strips every line break.
Snapshots only capture ring bytes appended after `mark_snapshot_floor()` (set at the end of `report_last_panic()`); without the floor, `drain_snapshot()` re-swallows the replayed report and every stored report grows into a loop of its predecessors.

`inv flash` writes the full 4 MB merged image over `0x0000–0x400000`, covering the NVS partition at `0x9000`: persisted data (the stored panic report, later the config)
does **not** survive flashing — the report replays across reboots only, and the RTC fallback did not survive the flash reset chain either (observed, mechanism not fully traced).

### Python/CAD env: `python` is pinned to 3.13 on purpose

`flake.nix` has exactly **one** `python` binding (`python = pkgs.python313;`) and everything derives from it: `buildInputs` (`python`, `python.pkgs.invoke`, `python.pkgs.pyserial`), `UV_PYTHON`,
and the venv path inside `LD_LIBRARY_PATH` (`${virtualenv}/lib/${python.libPrefix}/site-packages/vtkmodules` — note the `lib/`, `libPrefix` alone is just `python3.13`).
Do not "simplify" any of these back to `pkgs.python3` — on current nixpkgs that is 3.14, and the locked wheel stack is cp313-only:

- `cadquery-ocp` **7.8.1.1.post1** ships cp313-only manylinux wheels and **no sdist**, and the locked `build123d` 0.10 requires cadquery-ocp 7.8 — so `uv` fails at lock time with `No compatible wheel, nor sdist found for package 'cadquery-ocp'`.
  `requires-python = ">=3.13,<3.14"` in `pyproject.toml` keeps that failure explicit instead of mysterious.
- Upstream now has cp314 wheels (cadquery-ocp 8.0, build123d 0.13), but that is a major bump, not a flake tweak: build123d ≥ 0.13 depends on `cadquery-ocp-novtk>=8.0` (a _different_ package, and VTK-free, which would drop the `vtkmodules` entry below), plus `ocp-vscode` 4.x and a port of `assets/case/src/main_case.py`.

The failure mode that cost the most time (reproduced twice): the nixpkgs python setup hook puts a buildInput's `site-packages` on `PYTHONPATH` **only when its interpreter version matches**.
With nixpkgs `python3` = 3.14 and a 3.13 venv, `dmx-env` never reaches `PYTHONPATH` and `python -c "import build123d"` dies with `ModuleNotFoundError` — while the shell's `python` itself looks perfectly fine (`python -V` prints 3.14.7).
On a nixpkgs where `python3` is still 3.13 it works by accident, which is why the `refactor/case` branch behaved differently at identical flake text.

Second trap, even with that bridge in place: bare `python` is **not** guaranteed to be the pinned interpreter.
`pre-commit-check.enabledPackages` puts nixpkgs' python3 (3.14) and its hook packages on `PATH` _ahead_ of `python313`.
So `python -c "import build123d"` finds the pure-python package via `PYTHONPATH` and then fails deep inside with `ModuleNotFoundError: No module named 'OCP.OCP'` — a cp313 extension module is invisible to a 3.14 ABI.
Therefore the `shellHook` prepends `${virtualenv}/bin` to `PATH`: the venv interpreter must lead, and PATH order elsewhere is not to be trusted.

### OCP/build123d: native libs come from `LD_LIBRARY_PATH`, not the system

The venv is built from unpatchelf'ed manylinux wheels (`dontAutoPatchelf` for cadquery-ocp), so OCP's ELF dependencies resolve only through `LD_LIBRARY_PATH`; nix's `ld.so` consults the store's `ld.so.cache` and **never** `/usr/lib`, so a working host toolchain does not leak in.
Each entry below is load-bearing — every single one was removed and re-added after an `ImportError`:

| `LD_LIBRARY_PATH` entry | Needed by | Error without it |
| --- | --- | --- |
| `stdenv.cc.cc.lib` | `OCP.so` links libstdc++ directly | `ImportError: libstdc++.so.6` |
| `expat` | OCCT STEP/IGES readers | `ImportError: libexpat.so.1` |
| `zlib` | OCCT | `ImportError: libz.so.1` |
| `libGL`, `libX11` | OCCT visualization (TKService / TKOpenGl) | `ImportError: libGL.so.1` |
| `…/site-packages/vtkmodules` | bundled VTK 9.3 (`libvtk*-9.3.so`) shipped _inside_ the cadquery-ocp wheel | `ImportError: libvtkWrappingPythonCore3.13-9.3.so` |

Do **not** substitute nixpkgs `vtk` or `occt` for the last row: nixpkgs ships VTK 9.5 / OCCT 7.9 with different SONAMEs and cannot satisfy those `NEEDED` entries.
`site-packages/cadquery_vtk` (referenced by an older revision of this flake) does not exist at all — a nonexistent dir in `LD_LIBRARY_PATH` is silently ignored.
Sanity check after touching any of this: `nix develop -c python -c "import build123d, ocp_vscode"`.
