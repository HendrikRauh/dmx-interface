from invoke import task

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
