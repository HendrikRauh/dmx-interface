# cspell:ignore: JTAG UART FTDI Espressif

from invoke import task
from invoke.exceptions import Exit
import os
import serial.tools.list_ports
import shutil
import webbrowser


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
def build(c):
    """Build the project"""
    c.run("idf.py build", pty=True)


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
def clean(c):
    """Clean build artifacts"""
    c.run("idf.py fullclean", pty=True)


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
    c.run("cd web && npm update", pty=True)
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
        ".cspell",
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
