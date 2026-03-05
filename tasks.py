from invoke import task
import os
import shutil
import sys


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
    """Format all source files with clang-format, black, prettier, nixfmt, and svgo"""
    missing_tools = []

    print("Formatting C files...")
    result = c.run(
        "find main components -name '*.c' -o -name '*.h' | xargs clang-format -i",
        warn=True,
    )
    if result and not result.ok:
        missing_tools.append("clang-format")

    print("Formatting Python files...")
    result = c.run("black tasks.py", warn=True)
    if result and not result.ok:
        missing_tools.append("black")

    print("Formatting Nix files...")
    result = c.run("nixfmt flake.nix", warn=True)
    if result and not result.ok:
        missing_tools.append("nixfmt")

    print("Formatting SVG files...")
    result = c.run(
        "find . -name '*.svg' -not -path './build/*' -not -path './managed_components/*' | xargs svgo",
        warn=True,
    )
    if result and not result.ok:
        missing_tools.append("svgo")

    print("Formatting other files...")
    result = c.run("prettier --write '**/*.{js,json,yaml,yml,md,html,css}'", warn=True)
    if result and not result.ok:
        missing_tools.append("prettier")

    if missing_tools:
        print(f"\n❌ ERROR: Missing formatting tools: {', '.join(missing_tools)}")
        print("Please install them or reload the nix-shell.")
        sys.exit(1)


@task
def format_check(c):
    """Check if all files are formatted correctly"""
    missing_tools = []
    format_errors = []

    print("Checking C file formatting...")
    result = c.run(
        "find main components -name '*.c' -o -name '*.h' | xargs clang-format --dry-run --Werror",
        warn=True,
    )
    if result:
        if result.return_code == 127:  # Command not found
            missing_tools.append("clang-format")
        elif not result.ok:
            format_errors.append("C files")

    print("Checking Python file formatting...")
    result = c.run("black --check tasks.py", warn=True)
    if result:
        if result.return_code == 127:
            missing_tools.append("black")
        elif not result.ok:
            format_errors.append("Python files")

    print("Checking Nix file formatting...")
    result = c.run("nixfmt --check flake.nix", warn=True)
    if result:
        if result.return_code == 127:
            missing_tools.append("nixfmt")
        elif not result.ok:
            format_errors.append("Nix files")

    print("Checking other file formatting...")
    result = c.run("prettier --check '**/*.{js,json,yaml,yml,md,html,css}'", warn=True)
    if result:
        if result.return_code == 127:
            missing_tools.append("prettier")
        elif not result.ok:
            format_errors.append("JS/JSON/YAML/HTML/CSS files")

    if missing_tools:
        print(f"\n❌ ERROR: Missing formatting tools: {', '.join(missing_tools)}")
        print("Please install them or reload the nix-shell.")
        sys.exit(1)

    if format_errors:
        print(f"\n❌ ERROR: Formatting issues in: {', '.join(format_errors)}")
        print("Run 'invoke format' to fix them.")
        sys.exit(1)

    print("\n✅ All files are correctly formatted!")
