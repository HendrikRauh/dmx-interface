#!/bin/sh
# Install the udev rule that keeps the podman machine's ttyACM nodes
# world-writable across re-enumerations. Rootless Podman maps the container's
# root to the Windows user (uid 1000), which cannot open the stock 660
# root:dialout node the kernel creates after every flash reset; a chmod loop
# used to chase the mode - the udev rule creates the node correctly instead.
# Fed to `wsl -d podman-machine-default -u root -- bash` on stdin by
# attach-board.bat / setup-windows.bat: cmd cannot quote the rule's quotes.
set -eu
printf '%s\n' 'KERNEL=="ttyACM*", MODE="0666"' \
	>/etc/udev/rules.d/99-dmx-ttyacm.rules
# A running udevd picks new rules up from disk (and reloads on the next
# event); the explicit reload is best-effort only - the podman machine's
# udevadm control fails with a varlink "Connection reset" on this image.
udevadm control --reload-rules 2>/dev/null || true
udevadm trigger --subsystem-match=tty 2>/dev/null || true
ls -l /dev/ttyACM* 2>/dev/null || true
