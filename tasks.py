# cspell:ignore: JTAG UART FTDI Espressif CDC

import os
import shutil
import webbrowser

from invoke import task
from invoke.exceptions import Exit

_ELF = "dmx-interface"


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
    profile = "release" if release else "debug"
    elf_path = f"target/{target}/{profile}/{_ELF}"

    if not os.path.exists(elf_path):
        build(c, board=board, release=release)

    bin_path = f"target/{target}/{profile}/{_ELF}.bin"
    port_arg = f"--port {port}" if port else "--port /dev/ttyACM0"

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
    # So we clear bit 0 via write-mem through the stub, then reset via the
    # RTC watchdog, which boots the application.
    print("ℹ️  First flash? Hold BOOT + press RESET, then wait ~5s.")
    c.run(
        f"ESPTOOL_BEFORE=usb-reset esptool --chip {chip} {port_arg} --baud 74880 "
        f"--after watchdog-reset write-flash 0x0000 {bin_path} "
        f"write-mem 0x3F408128 0 0x1",
        pty=True,
    )


@task
def monitor(c, port=None):
    """Monitor serial output from device."""
    port_arg = f"--port {port}" if port else ""
    c.run(f"espflash monitor {port_arg}", pty=True)


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


@task
def check(c, board=_DEFAULT_BOARD):
    """Run cargo check for a specific board."""
    _, _, features = _board_args(board)
    c.run(f"cargo check --features {features}", pty=True)


@task(help={"o": "Open documentation in the default browser after generation."})
def docs(c, o=False):
    """Generate rustdoc documentation and copy redirect index."""
    target, _, features = _board_args(_DEFAULT_BOARD)
    doc_dir = f"target/{target}/doc"
    print("-> Generating documentation")
    c.run(
        f"cargo doc --features {features} --no-deps --document-private-items",
        pty=True,
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
