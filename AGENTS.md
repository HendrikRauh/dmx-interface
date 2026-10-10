# dmx-interface — AGENTS.md

<!-- cspell:ignore xtal -->

Rust firmware for ESP32-S2 (Lolin S2 Mini) — DMX512 over WiFi (access point
or station mode), JSON config persisted to NVS.

**Current state** (branch `no-std`): no_std pure Rust with esp-hal + embassy.
Wired into `src/main.rs`: status LED task, USB-CDC + esptool auto-reset, log
ring drain, panic report, boot-time `storage::load().apply()`, and `net::task`
(WiFi AP + DHCP server — verified on hardware, SSID `ChaosDMX-<last two MAC bytes>` — or station mode with a 15 s AP fallback) and an HTTP/WebSocket
server (reference API: static UI + `GET/POST /api/config` + WS envelopes on
`/ws`). **Scope split**: this repo owns the network + server infrastructure;
UI logic, tests and UX polish are handed to an external contributor —
see “Web tier” below. Open phases (see `TODO.md`): DMX UART output,
button-driven factory reset.

## Dev shell

```sh
nix develop                    # enter shell (direnv also works)
nix develop -c inv build       # one-shot build
```

All `inv` commands **must** run inside `nix develop` (or prefixed with
`nix develop -c`), as they depend on `esptool`, `espflash`, `cargo`, and other
flake-provided tools.

## Tasks (invoke)

| Command | What |
| --- | --- |
| `inv build` | Debug build → `target/xtensa-esp32s2-none-elf/debug/dmx-interface` |
| `inv build --release` | Release build (LTO, `opt-level="s"`) |
| `inv flash` | Build → convert to bin → flash via esptool (USB CDC) |
| `inv monitor` | Raw serial monitor, reconnecting, colored levels (in `tasks.py`) |
| `inv check` | Strict clippy for the xtensa target (flags in `tasks.py`) — **this is the test gate; there is no `cargo test` (no harness for this target)** |
| `inv clean` | `cargo clean` |
| `inv format` | `pre-commit run --all-files` |
| `inv docs` | Generate rustdoc + redirect — **fails on any rustdoc warning** (`RUSTDOCFLAGS=-D warnings` in `tasks.py`) |
| `inv docspath` | Print doc output path (for CI) |
| `inv web:build` | Vite build → `web/dist/` (single HTML) |
| `inv web:dev` | Vite dev server with mock API |

## Hard constraints

- **Toolchain**: nightly esp-rs fork via `RUSTUP_TOOLCHAIN` (set in flake,
  sourced from `esp-rs-nix`).
- **Target**: `xtensa-esp32s2-none-elf` (no_std, pure Rust, edition 2024).
- **build-std**: `["alloc", "core"]` via `[unstable]` in `.cargo/config.toml` —
  do NOT pass `-Zbuild-std` on the command line.
- **Linker**: `linkall.x` + `nostartfiles` via `.cargo/config.toml` rustflags
  (no `ldproxy`).
- **`esp_bootloader_esp_idf::esp_app_desc!()`** required in `main.rs` even with
  no_std (bootloader compatibility).
- **Clippy**: strict — `-D warnings` plus pedantic and cherry-picked
  restriction/nursery lints (arithmetic overflow, undocumented `unsafe`, `as`
  conversions, indexing/slicing, `unwrap`, std-vs-core drift, …); full set in
  `tasks.py` `_CLIPPY_ARGS`, used by `inv check` and the pre-commit hook
  (`entry = "invoke check"`). Never pass `--all-targets`: the `test` crate does
  not exist for this target.
- **Profiles**: dev `opt-level = 1`; release LTO + `codegen-units = 1` +
  `opt-level = "s"` + strip. **`package.esp-radio` is pinned to
  `opt-level = 3` in both profiles** — WiFi timing breaks at lower opt levels
  (esp-radio README); do not "optimize" that override away.
- **esp-alloc must keep its default features**: `compat` supplies the C symbols
  (`malloc`, `malloc_internal`, `free`, `free_internal`, `realloc_internal`,
  `calloc`, `calloc_internal`, `get_free_internal_heap_size`) that esp-radio
  links against; disabling defaults yields undefined-symbol link errors.

## Flake env vars

| Var | Value | Why |
| --- | --- | --- |
| `RUSTUP_TOOLCHAIN` | esp-rs path | Xtensa Rust nightly |
| `LIBCLANG_PATH` | libclang lib path | Bindgen |
| `GERMAN_DICT_PATH` | cspell de-de dict | Spell-checking |

## Architecture

| Module | Role | Status |
| --- | --- | --- |
| `src/main.rs` | Entry point, heap init, peripheral init, task spawns | LED + USB + logging + panic + net wired |
| `src/config.rs` | Config data model (heapless, serde, postcard for NVS); JSON wire shape = web contract (`config.d.ts`), `ConfigPatch` + `merge_patch` for partial updates | `ap_config`/`station_config`/`connection` consumed by `net`, LED brightness via `apply()` |
| `src/storage.rs` | NVS storage (esp-nvs), global singleton + panic snapshot keys | `init()` + boot-time `load()` wired; `save()`/`clear()` wired into HTTP/WS config handlers |
| `src/logging.rs` | `log` backend → static 4 KiB ring → CDC flush API | Wired; max level baked at build time (`build.rs` sets `LOG_LEVEL`: release=`info`, debug=`debug`) |
| `src/panic_report.rs` | Own `#[panic_handler]`, RTC/NVS persist, boot-loop guard, replay | Wired |
| `src/boards/mod.rs` + `s2_mini.rs` | Pin definitions (cfg-gated) | Implemented, not wired |
| `src/hardware/led.rs` | Status LED: `LedStatus` enum + embassy task (LEDC PWM, GPIO7) | Wired |
| `src/hardware/button.rs` | Debounced button (GPIO5) | Implemented, not wired |
| `src/hardware/efuse.rs` | MAC helpers (`mac_bytes`, `default_ap_ssid`) | Wired via `net` |
| `src/hardware/usb.rs` | USB-CDC-ACM + esptool auto-reset (DTR/RTS) + log drain | Wired |
| `src/net/mod.rs` | `WiFi` AP **or** station bring-up (`esp-radio` + `embassy-net`): AP static `192.168.4.1/24`, station DHCP with 15 s AP fallback; event loop = controller keep-alive | Wired (`net::task` from main) |
| `src/net/dhcp.rs` | Minimal DHCPv4 server (`DISCOVER`/`REQUEST` → `OFFER`/`ACK`/`NAK`, 8 leases from `.10`) | Wired (spawned by `net::task` in AP mode) |
| `src/net/http.rs` | HTTP/1.1 server: 4 fixed connection-slot tasks (static UI + `GET/POST /api/config`), `/ws` upgrade handoff | Wired (spawned by `net::task` in AP **and** STA mode) |
| `src/net/ws.rs` | RFC 6455 subset + JSON envelopes (`config.get`/`config.set`/`system.get`/`reset`/`ping` as reference handlers) | Wired (invoked from `http` on `/ws` upgrade) |

## Key crate versions

| Crate | Version | Why / gotcha |
| --- | --- | --- |
| `heapless` | 0.7 | 0.8 incompatible with postcard |
| `esp-alloc` | 0.10 | `HEAP.add_region(HeapRegion::new(...))` (no `.init()`); default features **on** — C malloc shims for esp-radio |
| `esp-radio` | 0.18 | WiFi driver; `default-features = false`, features `log-04`/`unstable`/`wifi`; requires `opt-level = 3` (see Hard constraints) |
| `esp-rtos` | 0.3 | Executor + WiFi glue; features `["embassy", "esp-radio", "log-04"]` |
| `embassy-net` | 0.9 | TCP/IP; every protocol opt-in via features (see `Cargo.toml`) — enabled: `dhcpv4`/`udp`/`icmp`/`tcp`; sockets share `StackResources<8>` (4 TCP slots + DHCP + headroom) |
| `embassy-time` | 0.5 | `Instant`/`Timer` (LED, USB poll, DHCP leases); `Instant::from_nanos(0)` for const zero (no `ZERO` const) |
| `embassy-futures` | 0.1 | `join!`/`select!` helpers |
| `embassy-executor` | 0.10 | Task fn call returns `Result<SpawnToken, SpawnError>` → `.expect(...)`/`match` then `spawner.spawn(token)` |
| `static_cell` | 2 | `StaticCell::init()` for `'static` embassy-usb buffers (edition 2024 forbids `&mut static mut`); transitive dep of esp-hal |
| `log` | 0.4 | Backend in `src/logging.rs` uses `set_logger_racy`/`set_max_level_racy` in a critical section (Xtensa has no CAS) |
| `esp-backtrace` | 0.19 | `Backtrace::capture()` only — `panic-handler` feature **off** (own handler in `panic_report.rs`); needs `println` for `build.rs` |
| `esp-nvs` | 0.5 | Takes `&Key` refs, `Key::from_str()` for const keys |
| `libm` | 0.2 | `sinf` for the LED breathing curve (`no_std` has no float math) |
| `base64` | 0.22 | `default-features = false, features = ["alloc"]`; WS accept key via `encode_slice` (no heap) |
| `sha1` | 0.10 | `default-features = false`; WS accept key digest |
| `serde-json-core` | 0.6 | `no_std` JSON; `from_slice` returns `(value, bytes_consumed)` — a tuple, not the value; **no internally tagged enums** (needs serde alloc buffering) |
| `embedded-io-async` | 0.7 | `Write`/`write_all` on `TcpSocket` (reads are inherent methods) |

## USB CDC flashing (ESP32-S2)

Lolin S2 Mini uses native USB-OTG CDC — no external UART, no DTR/RTS pins. The
firmware exposes the USB port as a CDC-ACM device and implements the classic
DTR/RTS reset protocol in software (see `src/hardware/usb.rs`), giving esptool
**auto-reset** via `ESPTOOL_BEFORE=usb-reset`.

- **First flash** (no firmware yet): hold **BOOT** + press **RESET** for the
  ROM bootloader, then `inv flash`.
- **Subsequent flashes**: `inv flash` resets into the bootloader itself.
- esptool reports the chip as ESP32-S2FNR2 with **2 MB embedded PSRAM**
  (not enabled in firmware yet).

After flashing, `inv flash` clears `RTC_CNTL_OPTION1.FORCE_DOWNLOAD_BOOT` via
esptool `write-mem 0x3F408128 0 0x1` (through the stub) and resets via the RTC
watchdog (`--after watchdog-reset`) so the app boots and re-enumerates as
USB-CDC automatically. `inv monitor` can be re-run without pressing RESET.

## Web tier — server ours, UI + tests handed off

The firmware **serves HTTP/WebSocket itself** (`src/net/http.rs` +
`src/net/ws.rs`, reference API below). What lives in the tree:

- **Server (this repo, hardware-verified)**: embedded single-file UI at `/`,
  font at `/fonts/Fredoka.ttf`, `GET/POST /api/config` (merge-patch semantics
  via `ConfigPatch`), WebSocket `/ws` with JSON envelopes —
  `config.get`/`config.set`/`system.get`/`reset`/`ping` as working examples.
  The wire JSON matches `web/src/types/config.d.ts` exactly.
- **Backbone (this repo)**: routing, fixed connection slots, WS framing +
  envelope parsing, persistence hook (`ConfigPatch` → NVS → `apply()`).
  The routes/envelopes below are a **replaceable reference**, not a contract.
- **Contributor scope**: API design (the shape beyond these reference
  endpoints), UI logic/UX, config diffing, `inv web:dev` mock upkeep, log
  streaming (not implemented server-side), web tests, transport refinements
  (keep-alive, extra routes, envelope extensions).
- **Stack**: Preact + TypeScript + Vite + SCSS
- **Build**: `vite build` → single HTML file (`vite-plugin-singlefile`) + gzip
  (`vite-plugin-compression`)
- **Dev**: `web/mock/config.mock.ts` (`vite-plugin-mock-dev-server`) mocks
  `GET/POST /api/config` — `inv web:dev` runs the UI against the mock
- **Firmware needs `web/dist/`**: `net/http.rs` `include_bytes!`s
  `web/dist/index.html` + `web/dist/fonts/Fredoka.ttf` — run `inv web:build`
  before `inv build`/`inv flash` (compile error otherwise).

## Pre-commit hooks (Nix-generated, do not edit `.pre-commit-config.yaml`)

Configured in `flake.nix` via `git-hooks.nix`: rustfmt, cargo-clippy
(`invoke check`), rustdoc (`invoke docs`, warning-free docs), `scripts/check-rust-docs.sh`
(doc coverage), ruff + ruff-format, alejandra + deadnix + statix + flake-checker,
oxfmt + oxlint, prettier, markdownlint + mdformat + cspell, shellcheck + shfmt,
ripsecrets + detect-private-keys.

New project jargon (chip/protocol spellings like `dhcpv4`, `smoltcp`) must be
added to `.cspell.json` — the hook checks every file, including Rust sources.

Run `inv format` (or `pre-commit run --all-files`) to re-run all hooks.

## CI

| Workflow | Trigger | What |
| --- | --- | --- |
| `check.yml` | Push to main/idf/rust/no-std, all PRs | pre-commit lint + `invoke build` |
| `release.yml` | Tag `v*.*.*` | `invoke release` → GitHub Release with `dist/*.bin` |
| `deploy-docs.yml` | Push to main | `invoke docs` → gh-pages |

## Pitfalls (hard-won)

Read this before changing firmware or memory/build logic.

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

### Heap placement: `.dram2_uninit`, not the default data segment

Adding WiFi statics overflows the main DRAM segment (171 KiB) — the linker
fails with `stack.x:11 cannot move location counter backwards (from 3ffeb388 to 3ffde000)`. The 128 KiB heap therefore lives in esp-hal's
`.dram2_uninit` (a 136 KiB region that is free after boot, `dram2.x`):

```rust
#[unsafe(link_section = ".dram2_uninit")] // edition 2024: unsafe attribute
static mut HEAP_MEM: [u8; HEAP_SIZE] = [0u8; HEAP_SIZE];
let heap_ptr = core::ptr::addr_of_mut!(HEAP_MEM).cast::<u8>();
```

Resulting layout: stack ~80 KiB (`_stack_end` 0x3FFCA0D8 → `_stack_start`
0x3FFDE000, grows down), heap 0x3FFDE000–0x3FFFE000 (grows up), 8 KiB margin to
RAM top. `HeapRegion::new(ptr, size, MemoryCapability::Internal)` then
`esp_alloc::HEAP.add_region(region)` — 0.10 has no `.init()` and no chip
features. New large statics will hit the same linker error; move them to
`.dram2_uninit` or shrink something.

### Flash: `--merge` is mandatory

`espflash save-image` **must** use `--merge`, otherwise only the bare app
binary is produced — **no bootloader, no partition table** — and the firmware
silently never boots (LED stays off, no crash output). Flash the merged binary
at offset `0x0000` (internal offsets are embedded).

### Two LEDs: GPIO7 (external) vs GPIO15 (onboard)

The firmware drives the **external** LED-Button module on **GPIO7**
(`hardware::led`); the onboard LED sits on **GPIO15** (active-low,
`ONBOARD_LED_GPIO` in `boards/s2_mini.rs`) and is **not driven** — it stays
dark, so don't use it to verify firmware state. Debug output goes over
USB-CDC (`inv monitor`).

### `#[esp_rtos::main]` does NOT call `esp_rtos::start()`

The macro only sets up the embassy executor. You **must** call
`esp_rtos::start()` manually inside the async main:

```rust
let timg0 = TimerGroup::new(peripherals.TIMG0);
let sw_int = SoftwareInterruptControl::new(peripherals.SW_INTERRUPT);
esp_rtos::start(timg0.timer0, sw_int.software_interrupt0);
```

### `InputConfig` is a struct (builder pattern)

Not `InputConfig::with_pull(...)`. Use:

```rust
Input::new(pin, InputConfig::default().with_pull(Pull::Up))
```

### USB-CDC buffers must be `'static`

The S2 USB peripheral types are lifetime-invariant, so the `embassy-usb`
driver — and all its device buffers (`EP_OUT_BUFFER`, descriptors,
`CONTROL_BUF`, `State`) — must be `'static`. Edition 2024 forbids
`&mut static mut`; use `static_cell::StaticCell<T>` with `.init(...)`
(returns `&'static mut T`, panics on double init — safe, tasks run once):

```rust
static EP_OUT_BUFFER: StaticCell<[u8; 1024]> = StaticCell::new();
let driver = Driver::new(usb, EP_OUT_BUFFER.init([0u8; 1024]), Config::default());
```

### esptool auto-reset: DTR/RTS are simulated in software

esptool's `--before usb-reset` walk is `(DTR,RTS) = (0,0) → (1,0) → (0,1) → (0,0)`. The firmware mirrors it onto ROM strapping semantics:

| DTR | RTS | Action |
| ----- | ----- | -------- |
| 1 | 0 | set `RTC_CNTL_OPTION1.FORCE_DOWNLOAD_BOOT` (simulates IO0 low) |
| 0 | 1 | plain `software_reset()` — boot target decided by the flag (simulates EN low) |
| 0 | 0 | clear the flag |
| 1 | 1 | no-op |

The ROM bootloader does **not** clear `FORCE_DOWNLOAD_BOOT`; with
`--after hard-reset` esptool's S2-specific reset then leaves the chip in the
bootloader (USB PID 0x0002) and the next flash dies with
`OSError: [Errno 71] Protocol error` on the idle ROM's first control request.
`inv flash` (`tasks.py`) therefore runs **two** esptool invocations (the CLI
accepts exactly one operation per invocation): run 1 flashes with
`--after no-reset-stub`, run 2 reconnects (`--before no-reset`), clears bit 0
via `write-mem 0x3F408128 0 0x1`, and resets with `--after watchdog-reset`.
Both set `ESPTOOL_OPEN_PORT_ATTEMPTS=5` (Errno 71 retries). The port is found
by scanning `/sys/bus/usb/devices` for VID `303a` (ROM `0002` + app `3001`);
exactly one device required, `--port` overrides, none → fail-fast with
first-flash BOOT+RESET hint, several → list + require `--port`. The port may
re-enumerate between the two runs (reset while the stub was resident): run 2
waits for the node and is skipped if the firmware (PID `3001`) came back.

`inv monitor` resolves the port the same way (waits up to 8 s for
re-enumeration after a flash) and streams via pyserial with reconnects. Do
**not** point espflash's `monitor --before no-reset-no-sync` at the running
app: its connect path queries chip registers over SLIP and dies with an I/O
error after ~19 s (the flags only skip the reset, not the queries).

### Debug/panic output: log over USB-CDC, not UART

The Lolin S2 Mini has **no USB-UART bridge** on GPIO43/44 — raw `esp-println`
(kept at `no-op` feature only to satisfy esp-backtrace's `build.rs`) produces
no output. All diagnostics go through `src/logging.rs`: the `log` backend
writes `[<ms>:>7ms][<L>] <msg>` lines into a static 4 KiB ring (drop-oldest);
`control_and_log` in `src/hardware/usb.rs` drains it to CDC — visible in
`inv monitor`.

**Panics**: `src/panic_report.rs` owns the `#[panic_handler]`. It logs the
report into the ring, persists it twice (RTC slow section + NVS `diag/last_panic`),
clears the RTC boot-loop counter (`mark_stable()` arms a 30 s task in
`main.rs`), resets via `esp_hal::system::software_reset()`, and halts after
**3 panics without a stable 30 s run**. The next boot replays the report into
the ring (`=== LAST PANIC (previous run) ===`); symbolize `0x…` frames with
`addr2line -e target/xtensa-esp32s2-none-elf/debug/dmx-interface`. Snapshots
must be pushed verbatim (`push_raw`) and only cover bytes after
`mark_snapshot_floor()` — re-splitting stored bytes or missing the floor loops
stored reports into themselves. Note: `inv flash` writes the full 4 MB merged
image, so NVS (and the stored panic report) is wiped on every flash; the RTC
fallback did not survive the flash reset chain either (observed).

- **esp-hal API spellings**: `esp_hal::rtc_cntl::reset_reason(esp_hal::system::Cpu::ProCpu)`
  → `Option<impl Debug>`; `esp_hal::system::software_reset() -> !`.

### WiFi/net stack specifics

- **`WifiController` must stay alive**: dropping it de-initializes the radio.
  `net::task`'s event loop (`controller.subscribe()` → `next_event().await`) is
  the keep-alive — don't restructure it into something that drops the
  controller, and don't let the task return.
- **`net::task` must never return** (see above); `Wifi init failed` is handled
  with an eternal `core::future::pending()` instead of a panic.
- **AP mode**: static `192.168.4.1/24` (`net::AP_IP`/`AP_PREFIX`
  consts; the DHCP server replies from `AP_IP` and derives its pool log
  from it — the `/24` pool layout itself is documented in `dhcp.rs`), no
  gateway, no DNS servers. SSID: configured `ap_config.ssid` or `efuse::default_ap_ssid()`
  (`ChaosDMX-` + last two MAC bytes as uppercase hex). WPA2 only when the
  password is ≥ 8 chars; otherwise open + warning (RFC minimum).
- **Station mode** (`connection == WifiSta` + non-empty `station_config.ssid`): the
  radio starts with `WifiConfig::Station` as `initial_config`, then
  `controller.connect_async()` is raced against `STA_CONNECT_TIMEOUT`
  (15 s) via `embassy_futures::select`. Success → embassy-net DHCP client
  on `interfaces.station` (`start_station` polls `stack.config_v4()`,
  warns once after 10 s, gives up after `STA_DHCP_TIMEOUT` (15 s)).
  Association error/timeout **or lease timeout** → AP fallback:
  `controller.set_config(&WifiConfig::AccessPoint(...))` — esp-radio stops
  and restarts the WiFi mode internally on a mode change. The abandoned
  station stack idles safely (`tx_token()` requires link up, and
  `esp_wifi_send_data` additionally guards on the mode) and uses its own
  `STACK_STA` cell, so `start_ap` can init `STACK_AP` beside it. An empty
  SSID degrades straight to the AP with a warning (never a radio-less boot).
  After a successful boot the keep-alive loop reacts to
  `EventInfo::StationDisconnected` with `try_station()` retries every
  `STA_RECONNECT_DELAY` (5 s), forever — esp-radio never reconnects on its
  own (`connect_async` covers a single attempt). No runtime AP fallback:
  the HTTP slots already live on `STACK_STA`, so a vanished AP leaves the
  device unreachable until reboot.
- **Subscribe after the mutable controller work**: `controller.subscribe()`
  borrows the controller, so `connect_async()`/`set_config()` (both `&mut`)
  must run first — subscription happens right before the keep-alive loop.
- **DHCP server** (`src/net/dhcp.rs`): hand-rolled RFC 2131 subset over an
  `embassy_net::udp::UdpSocket` bound to port 67; replies broadcast to
  `255.255.255.255:68` until the client has an address (sender `0.0.0.0` or
  broadcast flag), unicast for renewals. 8 fixed leases starting at `.10`,
  MAC-keyed, 1 h. UDP sockets need their own `PacketMetadata` arrays + ring
  buffers (`UdpSocket::new` takes four slices); the two modes keep
  separate 8-slot `StackResources` pools (`STACK_STA`/`STACK_AP` — within
  one stack the DHCP server XOR the DHCP client occupies a slot;
  embassy-net's DHCP client takes one pool slot as well).
- **DHCP options are TLV** (type, length, value): option 53/54/50 are _not_
  bare values — reading the length byte as data derails the parser (fixed in
  `dhcp::parse_client`).
- **`embassy_net::Ipv4Address` is `core::net::Ipv4Addr`** (smoltcp 0.13
  re-export): use `octets()`, `is_unspecified()`, `Ipv4Address::UNSPECIFIED`/
  `BROADCAST` — no smoltcp-specific address API. `smoltcp::wire::dhcpv4` is
  _not_ reachable through embassy-net (would need smoltcp as a direct dep);
  `src/net/dhcp.rs` hand-parses the wire format with bounds-checked
  `Reader`/`Writer` cursors instead.
- **`embassy_time::Instant` has no `ZERO`** — use `Instant::from_nanos(0)` in
  const contexts, `Instant::now()` + `saturating_add(Duration)` for deadlines.
- **Task spawn pattern** (embassy-executor 0.10): the task fn call yields
  `Result<SpawnToken, SpawnError>` — `match` + `log::error!` (no `.expect` in
  public fns, `missing_panics_doc` is pedantic), then `spawner.spawn(token)`.

### HTTP/WebSocket server (`src/net/http.rs`, `src/net/ws.rs`)

- **Fixed connection slots, no `TcpSocket::new` churn**: creating a fresh
  socket per connection rebooted the board _without a panic log_ in an
  earlier iteration. Each of the 4 slot tasks owns its socket for life:
  `accept` → handle → `close` → wait for `State::Closed` (3 s deadline,
  else `abort()`) → `accept` again. Calling `accept()` while the socket is
  still `CloseWait`/`LastAck` returns `AcceptError::InvalidState` — that is
  what `recycle()` waits out.
- **smoltcp allows several `listen()` sockets on the same port**: incoming
  segments go to the first socket whose `accepts()` matches (pool order), so
  N slot tasks = N parallel connections; SYNs beyond that get an RST and the
  browser retries (fine for the single-file UI: page + font + config + WS
  ≤ 4 concurrent).
- **WebSocket sessions leave the request budget**: `route()` only sniffs the
  upgrade (key → owned `String`), `serve()` runs `ws::serve` with the socket
  timeout cleared and a per-frame 120 s idle timeout, then resets the
  timeout before recycling.
- **serde internally tagged enums don't compile in no_std**: `#[serde(tag = "type")]` on a `Deserialize` needs `TaggedContentVisitor`, which is
  `cfg(any(feature = "std", feature = "alloc"))`-gated. `ws.rs` therefore
  parses a `MessageHead` first and a typed payload second (same slice); the
  `Reply` is a flat struct with `skip_serializing_if`.
- **`serde_json_core::from_slice` returns `(value, bytes_consumed)`** — a
  tuple, not the value; ignore the count with `(patch, _used)`.
- **`TcpSocket::accept` takes a `u16` port** (`IpListenEndpoint: From<u16>` — `usize` does not implement it).
- **Embedded UI assets**: `include_bytes!(concat!(env!("CARGO_MANIFEST_DIR"), "/web/dist/index.html"))` tracks rebuilds automatically, but a clean tree
  needs `inv web:build` first or compilation fails.
- **Spawn tokens are distinct opaque types**: `slot0(stack)` … `slot3(stack)`
  return different `impl` types — don't collect them in one array; spawn via
  the `spawn_slot!` macro inside `http::spawn_all`.
