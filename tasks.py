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
    """Format all source files using pre-commit hooks and optimize SVGs"""
    print("\nOptimizing SVG files...")
    c.run(
        "find . -name '*.svg' -not -path './build/*' -not -path './managed_components/*' | xargs -r svgo --quiet --final-newline",
        warn=True,
    )
    is_windows = os.name == "nt"
    if is_windows:
        # Windows doesn't provide a POSIX pty
        c.run("pre-commit run --all-files")
    else:
        c.run("pre-commit run --all-files", pty=True)


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


@task(help={"o": "Open documentation in the default browser after generation."})
def docs(c, o=False):
    """Generate Doxygen documentation."""
    task_begin("docs", "Docs")
    proc = subprocess.run("doxygen Doxyfile", shell=True)
    if proc.returncode == 0:
        task_end("docs", "done", "Docs generated")
        path = "docs/doxygen/html/index.html"
        console.print(
            f"\n[bold green]✓ Documentation generated in {path}[/bold green]"
        )
        if o:
            import webbrowser
            webbrowser.open(f"file://{os.path.abspath(path)}")
        return
    task_end("docs", "failed", "Doxygen failed")
    raise Exit(code=1)
