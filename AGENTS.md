# dmx-interface — AGENTS.md

<!--
Contract — this file is a budgeted ROUTER, not a manual.
  - Audience: AI agents first (AGENTS.md standard), humans via docs/.
  - Budget: ≤160 lines, enforced by scripts/check-agents-md.sh (hook "agents-md").
    Every line must earn its place. Canonical content lives in docs/*.md;
    AGENTS.md routes to it and keeps only terminal invariants.
  - Lifecycle comments on terminal rules:
      <!-- source: <where this was hard-won> | applies_to: <scope> | retire_when: <condition> -->

```text
Delete a rule the moment its retire_when fires — stale rules rot
faster than no rules.
```

- To add: delete something first. Pointers must resolve: every
  docs/<page>.md referenced here must exist (checked by the hook).
  -->

Rust firmware for ESP32-S2 (Lolin S2 Mini): DMX512 over WiFi (AP or station),
JSON config in NVS, on-device HTTP/WebSocket server + embedded web UI.
Pure Rust `no_std`, esp-hal + embassy. DMX UART output has not started yet;
status: [docs/roadmap.md](docs/roadmap.md).

## Dev shell

```sh
nix develop                    # all inv commands need this shell
nix develop -c inv build       # one-shot build (bundles web UI itself)
```

| Task | What |
| --- | --- |
| `inv build [--release]` | Firmware → `target/xtensa-esp32s2-none-elf/…/dmx-interface` |
| `inv flash` / `inv monitor` | Flash via esptool (auto-reset) / serial monitor |
| `inv check` | Strict clippy for xtensa — **the test gate; there is no `cargo test`** |
| `inv docs` | Full docs build: rustdoc (`/api/`) + site; `inv docs:rustdoc` alone |
| `inv web:build` / `inv web:dev` | Embedded UI build / dev server + mock API |
| `inv format` | Run all pre-commit hooks |

## Hard constraints

- Toolchain: nightly esp-rs fork via `RUSTUP_TOOLCHAIN` (from the flake);
  target `xtensa-esp32s2-none-elf`, edition 2024, `build-std` for
  `alloc`/`core` via `.cargo/config.toml` — never pass `-Zbuild-std` CLI.
- Clippy is strict (`-D warnings` + pedantic + cherry-picked
  restriction/nursery). Never `--all-targets`: the `test` crate does not
  exist for this target.
- `esp_bootloader_esp_idf::esp_app_desc!()` must stay in `main.rs`.
- Profiles: dev `opt-level = 1`; release LTO + `opt-level = "s"`.
  `package.esp-radio` stays pinned to `opt-level = 3` (WiFi timing breaks
  otherwise). `esp-alloc` keeps default features (malloc shims for esp-radio).
- Hooks are configured in `flake.nix` (git-hooks.nix) — never edit
  `.pre-commit-config.yaml`. New jargon → `.cspell.json`.
- Wire contract: `src/config.rs` JSON ⇄ `web/src/types/config.d.ts` — change
  both in the same commit.

<!-- source: repo conventions | applies_to: all edits | retire_when: never -->

## Doc router — read the page, not this file

| You are… | Read |
| --- | --- |
| building/flashing a device | [docs/getting-started.md](docs/getting-started.md), [docs/flashing.md](docs/flashing.md) |
| touching pins, wiring, LED/button | [docs/hardware.md](docs/hardware.md) |
| changing the config model or defaults | [docs/config.md](docs/config.md) |
| touching WiFi/DHCP/stack lifetime | [docs/network.md](docs/network.md) |
| touching HTTP/WS/API shape | [docs/http-ws.md](docs/http-ws.md) |
| working on the embedded UI | [docs/web.md](docs/web.md) (stack, wire contract) |
| setting up the environment / tasks | [docs/development.md](docs/development.md) |
| changing structure, tasks, data flow | [docs/architecture.md](docs/architecture.md) |
| planning work / checking status | [docs/roadmap.md](docs/roadmap.md) |

Module map and task topology: [docs/architecture.md](docs/architecture.md).
Crate versions and gotchas: `Cargo.toml` (source of truth).

## Scope

Firmware, the embedded UI (`web/`) and these docs (`site/`) live in this
repository; the UI is developed here alongside the server code —
[docs/web.md](docs/web.md) states the contract.

<!-- source: repo scope decision | applies_to: API/UI changes | retire_when: never -->

## Terminal invariants

Rules whose violation is silent or fatal.

1. `//!` only — never add `///` on `mod x;` declarations (breaks every
   intra-doc link in rustdoc; `inv docs` denies warnings).
   <!-- source: rustdoc scope bug, reproduced 1.98 | applies_to: src/**/mod decls | retire_when: never -->
2. Large statics go to `.dram2_uninit` — the main DRAM segment overflows
   (`stack.x:11 cannot move location counter backwards`).
   <!-- source: WiFi statics linker failure | applies_to: new static buffers | retire_when: linker layout changes -->
3. Flash with `--merge` (done for you by `inv flash`): a bare app image has
   no bootloader/partition table and never boots — the failure is silent.
   <!-- source: silent no-boot debugging | applies_to: any espflash save-image use | retire_when: never -->
4. GPIO7 is the driven status LED; onboard GPIO15 is never driven. Debug
   output is USB-CDC (`inv monitor`), not UART — the board has no UART bridge.
   <!-- source: dark-LED dead ends | applies_to: hardware/led, boards | retire_when: board redesign -->
5. `WifiController` must stay alive; `net::task` never returns — its event
   loop is the radio keep-alive. Subscribe only after `&mut` controller work.
   <!-- source: dropped-controller radio deaths | applies_to: src/net/mod.rs | retire_when: esp-radio API change -->
6. HTTP: fixed socket slots, never `TcpSocket::new` per connection (silent
   reboots). Spawn tokens are `Result` — `match`, no `.expect()` in public fns.
   <!-- source: reboot-without-panic session | applies_to: src/net/http.rs | retire_when: never -->
7. `#[esp_rtos::main]` does not call `esp_rtos::start()` — call it manually.
   <!-- source: executor never started | applies_to: src/main.rs | retire_when: macro starts calling it -->

<!-- source: hard-won debug sessions (see each page's intro) | applies_to: firmware | retire_when: individually noted -->

## Pre-commit / CI

Hooks (rustfmt, `invoke check`, rustdoc, doc coverage, lint/format suite,
agents-md budget) run from the dev shell via `inv format`; CI mirrors them in
`check.yml`, and `deploy-docs.yml` builds the docs and deploys them per
branch. Details: [docs/development.md](docs/development.md).
