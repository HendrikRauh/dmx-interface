#!/usr/bin/env bash
# cspell:ignore: ENXIO tmpfs
# Expose the ESP32 at its usual paths so `inv flash` finds /dev/ttyACM0.
#
# The host's /dev is bind-mounted at /host-dev (see devcontainer.json), so
# this loop (re)creates the paths whenever the CDC device (re)enumerates.
# Preferred: a private mknod node inside the container — openable via its
# own mode, the host's permissions stay untouched. Fallback: a symlink to
# /host-dev (rootless Podman refuses mknod in a user namespace).
#
# Re-enumeration renames the device (ttyACM0 -> ttyACM1 once the kernel
# reuses a fresh minor while the old node lingers), so this mirrors *every*
# host ttyACM* node under its host name and drops container nodes whose
# host counterpart is gone — a stale node would otherwise keep answering
# os.path.exists() and fail every open with ENXIO ("No such device or
# address"), which is what `inv monitor`'s reconnect loop used to trip on.
#
# Rootless Podman also drops the host's supplementary groups, so the symlink
# is visible but not readable unless the host grants group access — the
# one-line containers.conf annotation from README -> Development container ->
# Flashing does that. If the device is present but cannot be opened, print
# that hint once instead of failing silently.
set -u

probed=""
while :; do
	for want_path in /host-dev/ttyACM*; do
		[ -e "$want_path" ] || continue
		name=$(basename "$want_path")
		want=$(stat -c '%t:%T' "$want_path" 2>/dev/null || true)
		have=$(stat -L -c '%t:%T' "/dev/$name" 2>/dev/null || true)
		kind=$(stat -c '%F' "$want_path" 2>/dev/null || true)
		if [ -z "$want" ] || [ "$want" != "$have" ]; then
			# Missing, broken, or stale (a different device behind the name).
			sudo -n rm -f "/dev/$name" 2>/dev/null || rm -f "/dev/$name" 2>/dev/null || true
			maj=${want%%:*}
			min=${want##*:}
			if [ "$kind" = "character special file" ] && [ -n "$maj" ] &&
				[ -n "$min" ] &&
				sudo -n mknod -m 666 "/dev/$name" c "$((16#$maj))" "$((16#$min))" 2>/dev/null; then
				:
			else
				sudo -n ln -sfn "$want_path" "/dev/$name" 2>/dev/null ||
					ln -sfn "$want_path" "/dev/$name" 2>/dev/null ||
					true
			fi
		fi
		# Privileged engines (Docker) pre-create the node in the container's
		# own tmpfs with the host's root-only mode. chmod is safe there
		# (container-local node), but never through the symlink fallback —
		# that would re-own the host's node.
		if [ -e "/dev/$name" ] && [ ! -L "/dev/$name" ] && [ ! -r "/dev/$name" ]; then
			sudo -n chmod 666 "/dev/$name" 2>/dev/null ||
				chmod 666 "/dev/$name" 2>/dev/null ||
				true
		fi
		# Probe exactly once per container lifetime whether a node can be
		# opened: the probe opens the CDC node (asserting DTR), which drains
		# the firmware's log ring into the probe shell — repeating that per
		# re-enumeration name would eat the boot logs `inv monitor` is meant
		# to show. timeout guards against open() waiting for carrier.
		if [ -z "$probed" ] && [ -e "/dev/$name" ]; then
			probed=1
			if timeout 1 bash -c ": </dev/$name" 2>/dev/null; then
				:
			else
				echo "ensure-tty: /dev/$name is present but not openable."
				echo "Rootless Podman drops host group access (e.g. dialout); Docker's"
				echo "device cgroup may deny char devices outright."
				echo "See README -> Development container -> Flashing for the per-engine"
				echo "fix (containers.conf annotation / --privileged runArg)."
			fi
		fi
	done

	# Drop container nodes whose host counterpart disappeared (re-enumeration
	# renamed the device): a stale node fails opens with ENXIO forever.
	for dev_path in /dev/ttyACM*; do
		[ -e "$dev_path" ] || [ -L "$dev_path" ] || continue
		name=$(basename "$dev_path")
		if [ ! -e "/host-dev/$name" ]; then
			sudo -n rm -f "$dev_path" 2>/dev/null || rm -f "$dev_path" 2>/dev/null || true
		fi
	done

	if [ -d /host-dev/serial ] && [ ! -e /dev/serial ]; then
		sudo -n ln -sfn /host-dev/serial /dev/serial 2>/dev/null ||
			ln -sfn /host-dev/serial /dev/serial 2>/dev/null ||
			true
	fi
	sleep 2
done
