#!/bin/sh
# Keep the machine's /dev/ttyACM* nodes mode 0666. The udev rule from
# machine-udev.sh is the fast path, but udev is unreliable in the podman
# machine image (udevadm control talks varlink and rule application proved
# flaky), so a tiny in-machine loop is the fallback - kernel defaults would
# leave 660, which the rootless container (uid 1000) cannot open.
#
# This script only (re)starts that loop when it is missing: idempotent,
# no window, no systemd. Fed to `wsl -- bash` on stdin (attach-board.bat)
# or called via the /mnt/c path by board-autostart.ps1 every few seconds,
# which also re-spawns it after a machine restart.
if ! pgrep -f 'chmod 666 /dev/ttyACM' >/dev/null 2>&1; then
	if command -v setsid >/dev/null 2>&1; then
		setsid sh -c 'while :; do chmod 666 /dev/ttyACM* 2>/dev/null; sleep 2; done' \
			</dev/null >/dev/null 2>&1 &
	else
		nohup sh -c 'while :; do chmod 666 /dev/ttyACM* 2>/dev/null; sleep 2; done' \
			</dev/null >/dev/null 2>&1 &
	fi
fi
