# cspell:ignore: JTAG UART FTDI Espressif CDC

import os
import re
import shutil
import sys
import time
import webbrowser

from invoke import task
from invoke.exceptions import Exit, UnexpectedExit

_ELF = "dmx-interface"

# Shown when the device is unreachable (no port / connect failure) — see flash().
_FIRST_FLASH_HINT = "ℹ️  First flash? Hold BOOT + press RESET, then wait ~5s."


def _target_triple(chip: str) -> str:
    """Derive xtensa target triple from chip name (e.g. 'esp32s2' → 'xtensa-esp32s2-none-elf')."""
    return f"xtensa-{chip}-none-elf"


# Board definitions: name → chip (used as target triple, espflash --chip, and cargo feature)
TARGET_BOARDS = {
    "lolinS2mini": "esp32s2",
}

# Default board for dev tasks (build, flash, check, etc.)
_DEFAULT_BOARD = "lolinS2mini"


def _board_args(board):
    """Return (target, chip, features) for a board name."""
    if board not in TARGET_BOARDS:
        print(f"❌ Unknown board '{board}'. Available: {', '.join(TARGET_BOARDS)}")
        raise Exit(code=1)
    chip = TARGET_BOARDS[board]
    return _target_triple(chip), chip, chip


_ESPRESSIF_VID = "303a"
_APP_PID = "3001"  # running firmware; 0002 = ROM bootloader


def _sys_attr(path: str, name: str) -> str | None:
    """Read a single-line USB device attribute."""
    try:
        with open(os.path.join(path, name)) as f:
            return f.read().strip()
    except OSError:
        return None


def _espressif_ports() -> list[tuple[str, str, str]]:
    """List connected Espressif USB devices (VID 303a) as (tty, pid, description).

    Covers both the ROM bootloader (303a:0002) and the running CDC firmware
    (303a:3001); interfaces without a tty are ignored.
    """
    found = []
    usb_root = "/sys/bus/usb/devices"
    for entry in sorted(os.listdir(usb_root)):
        base = os.path.join(usb_root, entry)
        if _sys_attr(base, "idVendor") != _ESPRESSIF_VID:
            continue
        tty = None
        for iface in sorted(os.listdir(base)):
            tty_dir = os.path.join(base, iface, "tty")
            if not os.path.isdir(tty_dir):
                continue
            names = [n for n in os.listdir(tty_dir) if n.startswith("tty")]
            if names:
                tty = f"/dev/{names[0]}"
                break
        if not tty:
            continue
        pid = _sys_attr(base, "idProduct") or "????"
        product = _sys_attr(base, "product") or "ESP32"
        found.append((tty, pid, f"{_ESPRESSIF_VID}:{pid} {product}"))
    return found


def _usb_pid(tty: str) -> str | None:
    """Return the USB product id of the Espressif device owning tty, if any."""
    for port_tty, pid, _ in _espressif_ports():
        if port_tty == tty:
            return pid
    return None


def _wait_for_port(tty: str, timeout: float = 3.0) -> None:
    """Wait until the tty node exists again (re-enumeration after run 1)."""
    deadline = time.monotonic() + timeout
    while not os.path.exists(tty) and time.monotonic() < deadline:
        time.sleep(0.1)


def _resolve_port(port: str | None, hint: str | None = None) -> str:
    """Resolve a serial port: explicit --port wins, else the sole Espressif device."""
    if port:
        if not os.path.exists(port):
            print(f"❌ {port} not found — board not connected or not enumerated.")
            raise Exit(code=1)
        return port

    devices = _espressif_ports()
    if len(devices) == 1:
        tty, _, desc = devices[0]
        print(f"-> {tty} ({desc})")
        return tty
    if devices:
        print("❌ Multiple Espressif devices found — pass --port:")
        for tty, _, desc in devices:
            print(f"   {tty} ({desc})")
        raise Exit(code=1)
    print(f"❌ No Espressif device (VID {_ESPRESSIF_VID}) found on USB.")
    if hint:
        print(hint)
    raise Exit(code=1)


@task
def build(c, board=_DEFAULT_BOARD, release=False):
    """Build the no_std firmware for a specific board."""
    print("-> Building web frontend")
    c.run(f"cd {_WEB_DIR} && npm run build", pty=True)

    target, _, features = _board_args(board)
    profile = "release" if release else "debug"
    args = "--release" if release else ""
    print(f"-> Building for {board} ({target}) [{profile}]")
    c.run(f"cargo build --features {features} {args}", pty=True)

    elf_path = f"target/{target}/{profile}/{_ELF}"
    if os.path.exists(elf_path):
        size = os.path.getsize(elf_path)
        print(f"✓ {_ELF} ({size / 1024:.1f} KiB)")
    else:
        print(f"❌ Expected ELF not found at {elf_path}")
        raise Exit(code=1)


@task
def release(c):
    """Build release binaries for all boards → dist/*.bin"""
    version = os.environ.get("GITHUB_REF_NAME", "dev")

    dist_dir = "dist"
    if os.path.exists(dist_dir):
        shutil.rmtree(dist_dir)
    os.makedirs(dist_dir)

    for board, chip in TARGET_BOARDS.items():
        target = _target_triple(chip)
        print("\n=========================================")
        print(f"🚀 Building Target: {board} ({target})")
        print("=========================================")

        build(c, board=board, release=True)

        elf_path = f"target/{target}/release/{_ELF}"

        bin_name = f"dmx-interface-{board}-{version}.bin"
        bin_path = os.path.join(dist_dir, bin_name)

        print(f"-> Converting ELF to flashable .bin ({bin_name})")
        c.run(
            f"espflash save-image --chip {chip} --flash-size 4mb --merge {elf_path} {bin_path}",
            pty=True,
        )

        if os.path.exists(bin_path):
            size = os.path.getsize(bin_path)
            print(f"✓ {bin_path} ({size / 1024:.1f} KiB)")
        else:
            print(f"❌ Expected bin not found at {bin_path}")
            raise Exit(code=1)

        elf_out = os.path.join(dist_dir, f"dmx-interface-{board}-{version}.elf")
        shutil.copy2(elf_path, elf_out)
        print(f"✓ {elf_out}")

    print(f"\n✓ All targets built → {dist_dir}/")


@task
def flash(c, board=_DEFAULT_BOARD, port=None, release=False):
    """
    Flash firmware via USB CDC using esptool.py.

    The firmware exposes USB-CDC with software DTR/RTS auto-reset, so
    ESPTOOL_BEFORE=usb-reset enters the ROM bootloader by itself.
    Only the very first flash (no firmware yet) needs manual entry:
    hold BOOT + press RESET, then esptool.py will connect within ~5s.
    """
    target, chip, _ = _board_args(board)
    # Fail fast when the board is absent or ambiguous — no point in building
    # or invoking esptool then. A blank chip does not enumerate before the
    # manual bootloader entry, so it shows up as "no device" below.
    port_path = _resolve_port(port, hint=_FIRST_FLASH_HINT)

    profile = "release" if release else "debug"
    elf_path = f"target/{target}/{profile}/{_ELF}"

    if not os.path.exists(elf_path):
        build(c, board=board, release=release)

    bin_path = f"target/{target}/{profile}/{_ELF}.bin"
    port_arg = f"--port {port_path}"

    print(f"-> Converting ELF to bin for {chip}")
    c.run(
        f"espflash save-image --chip {chip} --flash-size 4mb --merge {elf_path} {bin_path}",
        pty=True,
    )

    # Merged binary contains bootloader + partition table + app at correct
    # internal offsets — flash the whole image starting at 0x0000.
    # If no firmware is running yet (first flash), the auto-reset below has
    # nothing to reset — enter the ROM bootloader manually first.
    #
    # The ROM bootloader does NOT clear RTC_CNTL_OPTION1.FORCE_DOWNLOAD_BOOT
    # (bit 0) after a USB-CDC download session. Without clearing it, the
    # post-flash reset lands in the ROM bootloader again — the device stays
    # stuck (next flash then hits `OSError: [Errno 71] Protocol error`).
    #
    # esptool's CLI runs exactly one operation per invocation (argparse in
    # v4, click in v5) — the old one-liner `write-flash … write-mem …`
    # never parsed. Two runs instead:
    #   1. DTR/RTS walk enters the bootloader, flash, stay in the stub
    #      (--after no-reset-stub keeps the stub resident for run 2).
    #   2. Reconnect without a reset walk (a walk would re-arm
    #      FORCE_DOWNLOAD_BOOT), clear bit 0 through the stub, then reset via
    #      the RTC watchdog, which boots the application.
    # Run 2 is silent (banner/chip block are noise), run 1 keeps the progress
    # bar. ESPTOOL_OPEN_PORT_ATTEMPTS retries the port open (Errno 71 above).
    #
    # Between the runs the chip can re-enumerate (a reset while the stub was
    # resident: port-close line glitch or a button press). Wait for the node
    # and look at what came back:
    #   - firmware (3001) → it booted the app, so the flag is clear — done
    #   - ROM bootloader / stub (0002) → run 2 as planned
    try:
        c.run(
            f"ESPTOOL_BEFORE=usb-reset ESPTOOL_OPEN_PORT_ATTEMPTS=5 "
            f"esptool --chip {chip} {port_arg} --baud 74880 "
            f"--after no-reset-stub write-flash 0x0000 {bin_path}",
            pty=True,
        )
    except UnexpectedExit as exc:
        out = getattr(getattr(exc, "result", None), "stdout", "") or ""
        if "Could not connect" in out or "Failed to connect" in out:
            print(_FIRST_FLASH_HINT)
        raise

    _wait_for_port(port_path)
    if _usb_pid(port_path) == _APP_PID:
        print(
            "✓ App came back after run 1 — FORCE_DOWNLOAD_BOOT already clear, cleanup skipped"
        )
        return

    try:
        c.run(
            f"ESPTOOL_OPEN_PORT_ATTEMPTS=5 esptool --silent --chip {chip} {port_arg} --baud 74880 "
            f"--before no-reset --after watchdog-reset write-mem 0x3F408128 0 0x1",
            pty=True,
        )
    except UnexpectedExit:
        print(
            "⚠ Firmware was flashed, but the cleanup reset failed — the device may "
            "stay in the bootloader. Power-cycle it or run `inv flash` again."
        )
        raise
    print("✓ FORCE_DOWNLOAD_BOOT cleared → watchdog reset, booting app")


# ANSI colors for the firmware's log line prefix `[<ms>ms][<L>] <msg>`
# (level markers from `logging.rs::level_marker`, `!` from the panic
# handler). Applied host-side by `monitor` — the ring and any persisted
# report stay plain bytes.
_LOG_RE = re.compile(r"^(\[\s*\d+ms\])\[(.)\] ")
# Byte twin of `_LOG_RE` for the raw stream the monitor's line sync sees.
_LOG_RE_B = re.compile(_LOG_RE.pattern.encode())
_LOG_COLORS = {
    "T": "\x1b[90m",  # trace — gray
    "D": "\x1b[34m",  # debug — blue
    "I": "\x1b[32m",  # info — green
    "W": "\x1b[33m",  # warn — yellow
    "E": "\x1b[31m",  # error — red
    "!": "\x1b[1;31m",  # panic — bold red
}
_ANSI_DIM = "\x1b[2m"
_ANSI_RESET = "\x1b[0m"


def _format_log_line(raw: bytes, color: bool) -> str:
    """Decode one log line (without newline) and tint it when it matches
    the firmware's log prefix. Non-matching lines pass through unchanged."""
    text = raw.decode("utf-8", "replace")
    match = _LOG_RE.match(text) if color else None
    tint = _LOG_COLORS.get(match.group(2)) if match else None
    if tint is None:
        return text
    # Timestamp dim, marker + message in the level color.
    return (
        f"{_ANSI_DIM}{match.group(1)}{_ANSI_RESET}"
        f"{tint}{text[match.end(1) :]}{_ANSI_RESET}"
    )


@task
def monitor(c, port=None):
    """
    Stream the device's serial output (raw byte passthrough, reconnecting).

    Shows the firmware log ring (`src/logging.rs`) incl. the last-panic
    replay after a reboot. espflash's monitor cannot be used here: its
    connect path queries chip registers over SLIP, which the running app
    never answers, so it dies with an I/O error after ~19 s. The port is
    opened without touching DTR/RTS beyond pyserial's asserted (1,1)
    default, which the firmware's bootloader state machine treats as a
    no-op; a vanished device (panic reset, flashing) is waited for. The
    first (possibly partial) line of a connection is dropped when it does
    not look like a log line — the ring may be joined where drop-oldest
    cut a line in half.

    On a TTY, levels are colorized (trace gray, debug blue, info green,
    warn yellow, error red, panic bold red); set `NO_COLOR` to disable.
    """
    del c  # no shell needed — the loop runs in-process
    import serial  # pyserial from the flake devShell; lazy import keeps other tasks independent

    color = sys.stdout.isatty() and not os.environ.get("NO_COLOR")
    tint_notes = sys.stderr.isatty() and not os.environ.get("NO_COLOR")
    sys.stdout.reconfigure(errors="replace")

    def note(msg: str, tint: str) -> None:
        """Status line on stderr, dimmed/colored when stderr is a TTY."""
        if tint_notes:
            msg = f"{tint}{msg}{_ANSI_RESET}"
        print(msg, file=sys.stderr)

    # After `inv ... flash` the board is gone for a moment (watchdog reset
    # + USB re-enumeration, ~1-3 s) — without this wait the chained
    # `inv build flash monitor` would die on "No Espressif device" here.
    deadline = time.monotonic() + 8.0
    waiting = False
    while True:
        ready = os.path.exists(port) if port else bool(_espressif_ports())
        if ready:
            break
        if not waiting:
            note("-> waiting for device ...", _ANSI_DIM)
            waiting = True
        if time.monotonic() >= deadline:
            break
        time.sleep(0.5)
    preferred = _resolve_port(port)

    def wait_for_tty() -> str:
        """Wait until a tty exists again, following re-enumeration."""
        while True:
            if os.path.exists(preferred):
                return preferred
            devices = _espressif_ports()
            if len(devices) == 1:
                return devices[0][0]
            time.sleep(0.5)

    def emit(raw: bytes) -> None:
        sys.stdout.write(_format_log_line(raw, color) + "\n")
        sys.stdout.flush()

    pending = b""  # bytes of the line still being received
    sync = True  # leading bytes may be a ring fragment — drop to the next line
    try:
        while True:
            node = wait_for_tty()
            note(f"-> monitoring {node}", _ANSI_DIM)
            try:
                with serial.Serial(node, 115_200, timeout=0.2) as link:
                    while True:
                        data = link.read(4096)
                        if not data:
                            continue
                        pending += data
                        if sync:
                            # The ring can be joined mid-line (drop-oldest
                            # cut a wrapped buffer somewhere inside a line).
                            # Every log line starts with `[<ms>ms][<L>] ` —
                            # wait until we can tell, then drop the fragment
                            # up to the first newline.
                            if _LOG_RE_B.match(pending):
                                sync = False
                            else:
                                idx = pending.find(b"\n")
                                if idx < 0:
                                    continue
                                pending = pending[idx + 1 :]
                                sync = False
                        while b"\n" in pending:
                            line, pending = pending.split(b"\n", 1)
                            emit(line)
            except BrokenPipeError:
                raise  # stdout gone — handled by the outer handler
            except (OSError, serial.SerialException) as exc:
                if pending and not sync:
                    # Device died mid-line: flush what arrived so far.
                    emit(pending)
                pending = b""
                sync = True
                note(
                    f"! device lost ({exc.__class__.__name__}), waiting ...",
                    _LOG_COLORS["W"],
                )
                time.sleep(0.5)  # let the node disappear fully before re-opening
    except BrokenPipeError:
        # Downstream closed the pipe (`inv monitor | head`) — point stdout's
        # fd at /dev/null so the interpreter-shutdown flush stays quiet.
        with open(os.devnull, "w", encoding="utf-8") as devnull:
            os.dup2(devnull.fileno(), sys.stdout.fileno())
        raise Exit(code=0) from None
    except KeyboardInterrupt:
        if pending and not sync:
            emit(pending)
        print(file=sys.stderr)
        raise Exit(code=0) from None


@task
def clean(c, full=False):
    """Clean build artifacts. Use --full to also remove node_modules."""
    c.run("cargo clean", pty=True)
    for d in ["web/dist", "dist"]:
        if os.path.isdir(d):
            shutil.rmtree(d)
            print(f"✓ Removed {d}/")
    if full and os.path.isdir(f"{_WEB_DIR}/node_modules"):
        shutil.rmtree(f"{_WEB_DIR}/node_modules")
        print(f"✓ Removed {_WEB_DIR}/node_modules/")


@task
def format(c):
    """Run all pre-commit hooks (lint, format, spell-check)."""
    c.run("pre-commit run --all-files", pty=True)


# Clippy flags: everything is an error. Groups: default lints (via -D
# warnings) + pedantic. Cherry-picked restriction/nursery lints that matter
# for no_std firmware: panic-prone indexing/slicing, silent `as` conversions,
# `unwrap` without context, needless `&mut`, std-instead-of-core drift,
# forgotten guards (`mem_forget`), string slicing, wildcard enum matches,
# `todo!`/`dbg!`, stale SAFETY comments, and multiple unsafe ops per block.
# The pre-commit hook runs the same thing via `invoke check` (see flake.nix).
_CLIPPY_ARGS = (
    "-D warnings "
    "-D clippy::pedantic "
    "-D clippy::arithmetic_side_effects "
    "-D clippy::undocumented_unsafe_blocks "
    "-D clippy::as_conversions "
    "-D clippy::indexing_slicing "
    "-D clippy::unwrap_used "
    "-D clippy::needless_pass_by_ref_mut "
    "-D clippy::std_instead_of_core "
    "-D clippy::std_instead_of_alloc "
    "-D clippy::alloc_instead_of_core "
    "-D clippy::mem_forget "
    "-D clippy::string_slice "
    "-D clippy::wildcard_enum_match_arm "
    "-D clippy::todo "
    "-D clippy::dbg_macro "
    "-D clippy::unnecessary_safety_comment "
    "-D clippy::multiple_unsafe_ops_per_block"
)


@task
def check(c, board=_DEFAULT_BOARD):
    """Type-check and lint a specific board (clippy subsumes cargo check)."""
    _, _, features = _board_args(board)
    c.run(f"cargo clippy --features {features} -- {_CLIPPY_ARGS}", pty=True)


@task(help={"o": "Open documentation in the default browser after generation."})
def docs(c, o=False):
    """Generate rustdoc documentation and copy redirect index."""
    target, _, features = _board_args(_DEFAULT_BOARD)
    doc_dir = f"target/{target}/doc"
    print("-> Generating documentation")
    c.run(
        f"cargo doc --features {features} --no-deps --document-private-items",
        pty=True,
        env={"RUSTDOCFLAGS": "-D warnings"},
    )

    redirect_src = "assets/docs/redirect.html"
    redirect_dst = os.path.join(doc_dir, "index.html")
    if os.path.exists(redirect_src):
        shutil.copy2(redirect_src, redirect_dst)
        print(f"✓ Copied redirect → {redirect_dst}")

    index_path = os.path.join(doc_dir, "dmx_interface/index.html")
    if os.path.exists(index_path):
        print(f"\n✓ Documentation generated in {doc_dir}")
        if o:
            webbrowser.open(f"file://{os.path.abspath(index_path)}")
        return
    raise Exit(code=1)


@task
def docspath(c):
    """Print the documentation output path (for CI)."""
    target, _, _ = _board_args(_DEFAULT_BOARD)
    print(f"target/{target}/doc")


# ──────────────────────────────────────────────────────────────────────────────
# Web frontend
# ──────────────────────────────────────────────────────────────────────────────

_WEB_DIR = "web"


@task(name="web:build")
def web_build(c):
    """Build the web frontend (Vite → web/dist/)."""
    print("-> Building web frontend")
    c.run(f"cd {_WEB_DIR} && npm run build", pty=True)


@task(name="web:dev")
def web_dev(c, port=5173):
    """Start the Vite dev server with mock API."""
    print(f"-> Starting Vite dev server on http://localhost:{port}")
    c.run(f"cd {_WEB_DIR} && npx vite --port {port}", pty=True)
