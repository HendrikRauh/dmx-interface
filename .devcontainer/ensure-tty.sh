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
# per-engine fix is documented in WINDOWS.md. Runs as root (postStart with
# remoteUser root), so no sudo anywhere.
set -u

# A mountpoint cannot be rm'd (podman --privileged bind-mounts the device
# into /dev; rm would return EBUSY and leave the dead bind behind), so
# unmount first. Shared by the stale-replace and the orphan-cleanup paths.
drop_node() {
	if mountpoint -q "$1"; then
		umount "$1" 2>/dev/null || true
	fi
	rm -f "$1" 2>/dev/null || true
}

while :; do
	for want_path in /host-dev/ttyACM*; do
		[ -e "$want_path" ] || continue
		name=$(basename "$want_path")
		want=$(stat -c '%t:%T' "$want_path" 2>/dev/null || true)
		have=$(stat -L -c '%t:%T' "/dev/$name" 2>/dev/null || true)
		want_mode=$(stat -c '%a' "$want_path" 2>/dev/null || true)
		have_mode=$(stat -L -c '%a' "/dev/$name" 2>/dev/null || true)
		kind=$(stat -c '%F' "$want_path" 2>/dev/null || true)
		# podman --privileged bind-mounts the host's device nodes into
		# /dev (mode and inode taken from the host at container start).
		# After a hot-plug re-enumeration the host drops that inode —
		# major:minor may stay identical while the bind points at a dead
		# entry (nlink 0), so also compare the mode and the link count.
		stale=""
		[ -z "$want" ] && stale=1
		[ "$want" != "$have" ] && stale=1
		[ "$want_mode" != "$have_mode" ] && stale=1
		[ "$(stat -c '%h' "/dev/$name" 2>/dev/null)" = "0" ] && stale=1
		if [ -n "$stale" ]; then
			# Missing, broken, or stale (a different device behind the name).
			drop_node "/dev/$name"
			maj=${want%%:*}
			min=${want##*:}
			if [ "$kind" = "character special file" ] && [ -n "$maj" ] &&
				[ -n "$min" ] &&
				mknod -m 666 "/dev/$name" c "$((16#$maj))" "$((16#$min))" 2>/dev/null; then
				:
			else
				ln -sfn "$want_path" "/dev/$name" 2>/dev/null || true
			fi
		fi
		# Privileged engines (Docker) pre-create the node in the container's
		# own tmpfs with the host's root-only mode. chmod is safe there
		# (container-local node), but never through the symlink fallback —
		# that would re-own the host's node.
		if [ -e "/dev/$name" ] && [ ! -L "/dev/$name" ] && [ ! -r "/dev/$name" ]; then
			chmod 666 "/dev/$name" 2>/dev/null || true
		fi
	done

	# Drop container nodes whose host counterpart disappeared (re-enumeration
	# renamed the device): a stale node fails opens with ENXIO forever.
	for dev_path in /dev/ttyACM*; do
		[ -e "$dev_path" ] || [ -L "$dev_path" ] || continue
		name=$(basename "$dev_path")
		if [ ! -e "/host-dev/$name" ]; then
			drop_node "$dev_path"
		fi
	done
	sleep 2
done
