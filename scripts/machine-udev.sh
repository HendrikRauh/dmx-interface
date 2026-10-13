#!/bin/sh
# Install the ttyACM udev rule and (re)start udevd in the podman machine.
# udevd re-applies MODE=0666 when this script pokes a device's uevent file.
#
# In this WSL kernel udevd never receives real hotplug uevents over netlink
# (only the boot-time coldplug replay applies rules); writing "add" to
# /sys/class/tty/ttyACM*/uevent reliably re-runs rule matching (~1 s).
# PID 1 in the machine is podman's own init, not systemd: udevd is started
# detached here and revived by this script if it ever dies.
# board-autostart.ps1 feeds this script every ~5 s while a board is
# attached (pidof check only; the uevent trigger runs only when udevd was
# just (re)started or a node still carries the wrong mode) - there is no
# in-machine daemon or chmod loop.
#
# udevadm control is deliberately avoided: its varlink socket never showed
# up reliably in this image. Rule application uses uevent writes instead.
RULE=/etc/udev/rules.d/99-ttyacm-mode.rules
echo 'KERNEL=="ttyACM*", MODE="0666"' >"$RULE"

NEEDS_APPLY=0
if ! pidof systemd-udevd >/dev/null 2>&1; then
	rm -f /tmp/udevd.log
	setsid /lib/systemd/systemd-udevd </dev/null >/tmp/udevd.log 2>&1 &
	sleep 1
	NEEDS_APPLY=1
fi

# Re-run rules for nodes that are missing 0666 (freshly started udevd did
# not see their add event, or the node was created while udevd was dead).
for node in /dev/ttyACM*; do
	[ -e "$node" ] || continue
	if [ "$(stat -c '%a' "$node" 2>/dev/null)" != 666 ]; then
		NEEDS_APPLY=1
		break
	fi
done
if [ "$NEEDS_APPLY" = 1 ]; then
	for uevent in /sys/class/tty/ttyACM*/uevent; do
		[ -e "$uevent" ] || continue
		echo add >"$uevent" 2>/dev/null || true
	done
fi
