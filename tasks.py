# cspell:ignore: JTAG UART FTDI Espressif

from invoke import task
from invoke.exceptions import Exit
import os
import serial.tools.list_ports
import shutil
import webbrowser

## @brief Dictionary mapping supported target boards to their respective sdkconfig.defaults files.
TARGET_BOARDS = {
    "lolinS2mini": "sdkconfig.defaults",
    "lolinS3mini": "sdkconfig.defaults.lolinS3mini",
}


def _find_esp_port():
    """Finds the first matching USB port based on known ESP VIDs/PIDs."""
    # Known IDs for ESP boards and common USB-UART bridges
    ESP_IDENTIFIERS = [
        (0x303A, None),  # Espressif (e.g., ESP32-S3/C3 native USB-JTAG-Serial)
        (0x10C4, 0xEA60),  # Silicon Labs CP210x (very common on ESP32)
        (0x1A86, 0x7523),  # WCH CH340 (common on budget ESP boards)
        (0x0403, 0x6001),  # FTDI FT232R
    ]

    ports = serial.tools.list_ports.comports()

    for port in ports:
        # Check if VID/PID match
        for vid, pid in ESP_IDENTIFIERS:
            if port.vid == vid and (pid is None or port.pid == pid):
                return port.device

    # Fallback to default if no matching device is found
    return "/dev/ttyUSB0"


@task
def build(c, board="lolinS2mini"):
    """Build the project for a specific board (default: lolinS2mini)"""
    if board not in TARGET_BOARDS:
        print(f"❌ Error: Board '{board}' is not defined in TARGET_BOARDS.")
        print(f"Available boards: {', '.join(TARGET_BOARDS.keys())}")
        raise Exit(code=1)

    c.run("npm run build")

    defaults_file = TARGET_BOARDS[board]
    print(f"-> Building for board: {board} using {defaults_file}")
    c.run(f"idf.py -D SDKCONFIG_DEFAULTS={defaults_file} build", pty=True)


@task
def flash(c, port=None):
    """Flash the project to device"""
    target_port = port if port else _find_esp_port()
    print(f"-> Using serial port for flashing: {target_port}")
    c.run(f"idf.py -p {target_port} flash", pty=True)


@task
def monitor(c, port=None):
    """Monitor serial output from device"""
    target_port = port if port else _find_esp_port()
    print(f"-> Using serial port: {target_port}")
    c.run(f"idf.py monitor -p {target_port}", pty=True)


@task
def web_dev(c):
    """Start web development server with hot reloading"""
    c.run("npm run dev", pty=True)


def release(c):
    """Build single binaries for release across all targets"""
    version = os.environ.get("GITHUB_REF_NAME", "dev")

    dist_dir = "dist"
    if os.path.exists(dist_dir):
        shutil.rmtree(dist_dir)
    os.makedirs(dist_dir)

    for board, defaults_file in TARGET_BOARDS.items():
        print("\n=========================================")
        if not os.path.exists(defaults_file):
            print(f"⚠️ Warning: {defaults_file} not found. Skipping {board}...")
            continue

        print(f"🚀 Building Target: {board} ({defaults_file})")
        print("=========================================")

        c.run("idf.py fullclean", pty=True)
        if os.path.exists("sdkconfig"):
            os.remove("sdkconfig")

        build_cmd = f"idf.py -D SDKCONFIG_DEFAULTS={defaults_file} -D CMAKE_BUILD_TYPE=Release build"
        c.run(build_cmd, pty=True)

        c.run("idf.py merge-bin", pty=True)

        source_bin = "build/merged-binary.bin"
        target_bin = os.path.join(dist_dir, f"ChaosDMX-{board}-{version}.bin")

        if os.path.exists(source_bin):
            shutil.copy(source_bin, target_bin)
            print(f"➔ Asset successfully created: {target_bin}")
        else:
            print(f"❌ Error: {source_bin} was not created.")
            raise Exit(code=1)


@task
def clean(c):
    """Clean build artifacts"""
    c.run("idf.py fullclean", pty=True)

    path = "web/dist"
    if os.path.exists(path):
        shutil.rmtree(path)


@task
def config(c):
    """Open menuconfig to edit project settings"""
    c.run("idf.py menuconfig", pty=True)


@task
def saveconfig(c):
    """Save current config as sdkconfig.defaults"""
    c.run("idf.py save-defconfig", pty=True)


@task
def update(c):
    """Update project dependencies"""
    c.run("idf.py update-dependencies", pty=True)
    c.run("npm update", pty=True)
    c.run("nix flake update", pty=True)


@task
def reset(c):
    """Reset project to clean state: remove build, config, and managed components"""
    files_to_remove = [
        ".cspellcache",
        ".pre-commit-config.yaml",
        "bootloader.bin",
        "sdkconfig",
        "sdkconfig.old",
    ]
    dirs_to_remove = [
        ".ruff_cache",
        "build",
        "docs/doxygen",
        "managed_components",
        "web/dist",
        "web/node_modules",
    ]

    for f in files_to_remove:
        if os.path.exists(f):
            os.remove(f)

    for d in dirs_to_remove:
        if os.path.exists(d):
            shutil.rmtree(d)


@task
def format(c):
    """Format all source files using pre-commit hooks"""
    c.run("pre-commit run --all-files", pty=True)


@task(help={"o": "Open documentation in the default browser after generation."})
def docs(c, o=False):
    """Generate Doxygen documentation."""
    result = c.run("doxygen Doxyfile", warn=True)
    if result.ok:
        path = "docs/doxygen/html/index.html"
        print(f"\n✓ Documentation generated in {path}")
        if o:
            webbrowser.open(f"file://{os.path.abspath(path)}")
        return
    raise Exit(code=1)


@task
def docs_coverage(c):
    """List doxygen coverage of documentation."""
    c.run("python tools/doxy-coverage.py docs/doxygen/xml --no-error", pty=True)
