from invoke import task
from invoke.exceptions import Exit
import os
import shutil
import subprocess
import sys
import webbrowser


@task
def cleanbuild(c):
    """Clean build: fullclean and build the project"""
    c.run("idf.py fullclean")
    c.run("idf.py build")


@task
def build(c):
    """Build the project"""
    c.run("idf.py build")


@task
def flash(c):
    """Flash the project to device"""
    c.run("idf.py flash")


@task
def monitor(c, port="/dev/ttyUSB0"):
    """Monitor serial output from device"""
    c.run(f"idf.py monitor -p {port}")


@task
def run(c, port="/dev/ttyUSB0"):
    """Build, flash, and monitor in sequence"""
    build(c)
    flash(c)
    monitor(c, port)


@task
def clean(c):
    """Clean build artifacts"""
    c.run("idf.py fullclean")


@task
def config(c):
    """Open menuconfig to edit project settings"""
    is_windows = os.name == "nt"
    if is_windows:
        # Windows doesn't provide a POSIX pty, not sure how to open menuconfig interactive
        print(
            "Please run 'idf.py menuconfig' directly in the terminal to edit project settings. This option is not supported in the invoke task on Windows."
        )
    else:
        c.run("idf.py menuconfig --color-scheme=monochrome", pty=True)


@task
def saveconfig(c):
    """Save current config as sdkconfig.defaults"""
    c.run("idf.py save-defconfig")


@task
def update(c):
    """Update project dependencies"""
    c.run("idf.py update-dependencies")


@task
def reset(c):
    """Reset project to clean state: remove build, config, and managed components"""
    if os.path.exists("sdkconfig"):
        os.remove("sdkconfig")
    if os.path.exists("sdkconfig.old"):
        os.remove("sdkconfig.old")
    if os.path.exists("build"):
        shutil.rmtree("build")
    if os.path.exists("managed_components"):
        shutil.rmtree("managed_components")


@task
def format(c):
    """Format all source files using pre-commit hooks"""

    is_windows = os.name == "nt"
    if is_windows:
        # Windows doesn't provide a POSIX pty
        c.run("pre-commit run --all-files")
    else:
        c.run("pre-commit run --all-files", pty=True)


@task(help={"o": "Open documentation in the default browser after generation."})
def docs(c, o=False):
    """Generate Doxygen documentation."""
    proc = subprocess.run("doxygen Doxyfile", shell=True)
    if proc.returncode == 0:
        path = "docs/doxygen/html/index.html"
        print(f"\n✓ Documentation generated in {path}")
        if o:
            webbrowser.open(f"file://{os.path.abspath(path)}")
        return
    raise Exit(code=1)
