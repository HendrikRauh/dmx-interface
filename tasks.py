from invoke import task
import os
import shutil

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
    
