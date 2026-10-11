#!/usr/bin/env bash
# Expose the ESP32 at its usual paths so `inv flash` finds /dev/ttyACM0.
#
# The host's /dev is bind-mounted at /host-dev (see devcontainer.json), so
# this loop (re)creates the paths whenever the CDC device (re)enumerates.
# Preferred: a private mknod node inside the container — openable via its
# own mode, the host's permissions stay untouched. Fallback: a symlink to
# /host-dev (rootless Podman refuses mknod in a user namespace). The node is
# rebuilt whenever its major:minor drifts from the host's (re-enumeration).
#
# Rootless Podman also drops the host's supplementary groups, so the symlink
# is visible but not readable unless the host grants group access — the
# one-line containers.conf annotation from README -> Development container ->
# Flashing does that. If the device is present but cannot be opened, print
# that hint once instead of failing silently.
set -u

hinted=""
while :; do
	if [ -e /host-dev/ttyACM0 ]; then
		want=$(stat -c '%t:%T' /host-dev/ttyACM0 2>/dev/null || true)
		have=$(stat -L -c '%t:%T' /dev/ttyACM0 2>/dev/null || true)
		kind=$(stat -c '%F' /host-dev/ttyACM0 2>/dev/null || true)
		if [ -z "$want" ] || [ "$want" != "$have" ]; then
			# Missing, broken, or stale (a different device behind the name).
			sudo -n rm -f /dev/ttyACM0 2>/dev/null || rm -f /dev/ttyACM0 2>/dev/null || true
			maj=${want%%:*}
			min=${want##*:}
			if [ "$kind" = "character special file" ] && [ -n "$maj" ] &&
				[ -n "$min" ] &&
				sudo -n mknod -m 666 /dev/ttyACM0 c "$((16#$maj))" "$((16#$min))" 2>/dev/null; then
				:
			else
				sudo -n ln -sfn /host-dev/ttyACM0 /dev/ttyACM0 2>/dev/null ||
					ln -sfn /host-dev/ttyACM0 /dev/ttyACM0 2>/dev/null ||
					true
			fi
		fi
		# Probe exactly once whether the node can be opened: the probe opens
		# the CDC node, which toggles DTR/RTS — repeating that would fight
		# esptool's reset walk. timeout guards against open() waiting for
		# carrier on the serial line.
		if [ -z "$hinted" ] && [ -e /dev/ttyACM0 ]; then
			hinted=1
			if ! timeout 1 bash -c ': </dev/ttyACM0' 2>/dev/null; then
				echo "ensure-tty: /dev/ttyACM0 is present but not openable."
				echo "Rootless Podman drops host group access (e.g. dialout); Docker's"
				echo "device cgroup may deny char devices outright."
				echo "See README -> Development container -> Flashing for the per-engine"
				echo "fix (containers.conf annotation / --privileged runArg)."
			fi
		fi
	fi
	if [ -d /host-dev/serial ] && [ ! -e /dev/serial ]; then
		sudo -n ln -sfn /host-dev/serial /dev/serial 2>/dev/null ||
			ln -sfn /host-dev/serial /dev/serial 2>/dev/null ||
			true
	fi
	sleep 2
done
