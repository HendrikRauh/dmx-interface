#!/bin/sh
# Keep the machine's /dev/ttyACM* nodes mode 0666 - the kernel's default
# 660 root:dialout is unreadable for the rootless container user (uid 1000).
# udev in the podman machine image is unreliable (udevadm control talks
# varlink and rule application proved flaky), so this loop is the real
# mechanism, not a fallback.
#
# This script only (re)starts the loop when it is missing: idempotent, no
# window, no systemd. Fed to `wsl -- bash` on stdin (attach-board.bat) or
# called via the /mnt/c path by board-autostart.ps1 every ~15 s, which also
# re-spawns it after a machine restart. setsid detaches it from the WSL
# session - a plain nohup background job died with its session on occasion.
if ! pgrep -f 'chmod 666 /dev/ttyACM' >/dev/null 2>&1; then
	setsid sh -c 'while :; do chmod 666 /dev/ttyACM* 2>/dev/null; sleep 2; done' \
		</dev/null >/dev/null 2>&1 &
fi
