# Windows development

Everything runs inside a [development container](https://containers.dev/) — no
Nix, no toolchain and no container GUI on Windows itself. The engine stack is
the [Podman](https://podman.io/) CLI (Apache-2.0, ~30 MB) with its rootless
machine on [WSL 2](https://learn.microsoft.com/en-us/windows/wsl/), plus
[usbipd-win](https://github.com/dorssel/usbipd-win) for USB pass-through into
the machine. This keeps the host stack small and fully open-source (no Docker
Desktop), and it is the
[Espressif-documented workflow](https://docs.espressif.com/projects/vscode-esp-idf-extension/en/latest/additionalfeatures/docker-container.html)
for flashing over USB-CDC from a dev container.

macOS is unsupported: USB devices cannot reach the container VM there at all,
and the usbipd/WSL mechanism has no macOS equivalent.

## One-time setup

1. Run `scripts\setup-windows.bat` **as administrator** once. It checks CPU
   virtualization (enable AMD-V/SVM or VT-x in BIOS/UEFI if it complains),
   installs the WSL 2 platform (`wsl --install --no-distribution` — no user
   distro needed), the Podman CLI, a rootless `podman machine` and usbipd-win,
   then starts the machine. Re-running it is safe; a reboot may be requested
   after the WSL step.
2. Open the cloned repository in VS Code (with the
   [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers);
   [GitHub Desktop](https://desktop.github.com/) is fine for cloning), set the
   user setting `"dev.containers.dockerPath": "podman"` (Settings → search
   "docker path"; hosts where `docker` is Podman in disguise — NixOS symlinks,
   Podman-only hosts — need it too, Docker users skip it) and run
   **Dev Containers: Reopen in Container**. The first build downloads the
   Xtensa toolchain during `postCreateCommand` (5–15 minutes — `esp-rs-nix`
   ships pre-built artifacts but has no binary cache); every later start only
   needs `podman machine start` (the setup script leaves it running).
3. Run `scripts\attach-board.bat` once, **non-elevated**, with the board
   plugged in (see [Flashing (USB pass-through)](#flashing-usb-pass-through)): it shares the board with usbipd
   (one-time UAC for `usbipd bind`), installs the in-machine permission keeper
   and drops the hidden autostart that keeps the device attached.

## Daily use

Work as usual: `inv build`, `inv check`, `inv format`, `inv web:dev` (Vite dev
server — port 5173 is forwarded to your host automatically), `inv flash`,
`inv monitor`. Integrated terminals start in `nix develop` automatically. You
do not have to touch the terminal at all: the tasks in `.vscode/tasks.json`
wrap the `inv` commands under **Terminal → Run Task…**, with **Ctrl+Shift+B**
bound to the firmware build.

## Flashing (USB pass-through)

The container bind-mounts the machine's `/dev` at `/host-dev` — a direct
`/dev` bind would replace the container's own devpts and break terminals on
rootless engines. `.devcontainer/ensure-tty.sh` runs in a loop after every
start: it creates a private `mknod` node for the board (leaving the host's
permissions untouched) or falls back to symlinking `/dev/ttyACM0` to
`/host-dev`, so `inv flash` finds the device at its usual path.

USB/IP's VHCI driver lives in the WSL 2 kernel that the Podman machine runs
on, so once a device is attached it shows up in the machine's `/dev` — and
therefore on the container's `/host-dev`.

`scripts\attach-board.bat` automates the board plumbing:

1. Finds the board (VID `303a`) and runs `usbipd bind` once if needed — the
   only step that needs elevation (accept the UAC prompt).
2. Starts `scripts/machine-tty-keeper.sh` in the machine: an idempotent
   spawner for a 2-second `chmod 666 /dev/ttyACM*` loop. Rootless Podman maps
   the container's `root` to your Windows user, so the machine's
   `660 root:dialout` node would otherwise stay unreadable inside the
   container.
3. Writes `Startup\dmx-board-attach.vbs` and starts it right away — a hidden
   watcher (`scripts/board-autostart.ps1`) that keeps the board attached for
   as long as you are logged on.

After that: plug the board in and forget about it. Attach, node permissions
and the container's `/dev/ttyACM0` all heal on their own after every flash
reset — `inv flash` waits up to 150 open attempts (≈15 s) for the device to
re-enumerate.

A plain [WSL](https://learn.microsoft.com/en-us/windows/wsl/) dev shell
remains a working alternative for flashing.

### Handing the device back to Windows

The device is owned by WSL while attached and invisible to Windows
applications. To release it:

1. Create the marker file `%LOCALAPPDATA%\dmx-board-attach.disabled` (the
   watcher idles while it exists; delete it again to re-enable attaching).
2. Run `usbipd detach --busid <BUSID>` — detaching alone is futile, the
   watcher would re-attach it within seconds.

## Notes

- The container keeps its own Nix store: **Rebuild Container** re-downloads
  the toolchain, plain restarts keep everything.

- On hosts with Nix you can keep using the native dev shell — both are driven
  by the same `flake.nix`. Entering either environment regenerates
  `.pre-commit-config.yaml` (a store symlink) for its own Nix store, which
  breaks the pre-commit tools on the other side until their next
  `nix develop` — self-healing, nothing to do by hand.

- Alternatively to VS Code you can drive the same setup from a terminal with
  the [devcontainer CLI](https://github.com/devcontainers/cli) (the
  `ghcr.io/devcontainers/cli` image cannot be pulled anonymously anymore, so
  install the CLI itself):

  ```sh
  # inside the Podman machine (Windows: wsl -d podman-machine-default)
  curl -fsSL https://raw.githubusercontent.com/devcontainers/cli/main/scripts/install.sh | sh
  devcontainer up --workspace-folder /mnt/c/Users/<you>/<repo> --docker-path podman
  devcontainer exec --workspace-folder /mnt/c/Users/<you>/<repo> --docker-path podman \
    -- nix develop --command inv build
  ```

### Linux engines (other engines)

- **Rootful Docker**: plug in the board and run `inv flash`. Docker's default
  device cgroup may deny `open()` on the node `ensure-tty.sh` creates (visible
  as `EPERM` on `/dev/ttyACM0`) — which is why `devcontainer.json` ships
  `"runArgs": ["--privileged"]` on every engine; drop the flag if you want a
  tighter sandbox and flashing still works.

- **Rootless Podman**: Podman drops the host's supplementary groups, so
  `/dev/ttyACM0` (root:`dialout`) would be visible but unreadable. A one-time
  host setup documented in `containers.conf(5)` (requires the `crun` runtime,
  Podman's default) restores them for all your Podman containers — add to
  `~/.config/containers/containers.conf` (Docker ignores this file):

  ```toml
  [containers]
  annotations = ["run.oci.keep_original_groups=1"]
  ```

  then recreate the container once.

## How it works

Mechanics worth knowing when something misbehaves:

- **usbipd attaches at the WSL 2 platform level**: the VHCI driver lives in
  the one shared WSL2 kernel, so an attached device appears in _every_
  distro — including `podman-machine-default`. `usbipd attach --wsl` only
  needs _a_ running distro to poke; it must not target a utility distro
  (`--distribution docker-desktop` fails, usbipd issue #794). No separate
  Ubuntu is needed: `wsl --install --no-distribution` (platform only)
  suffices.
- **`usbipd attach` must run non-admin** as the same user that owns the WSL
  distros (usbipd issue #1008); only `usbipd bind` needs elevation (once,
  persistent).
- **The permission keeper replaces udev**: the podman machine image's udev is
  unreliable — `udevadm control --reload-rules` fails (varlink to systemd is
  broken) and workers hung intermittently, applying a rule to some
  re-enumerations and not others. The keeper loop is the real safety net: it
  is spawned with `setsid` (a plain `nohup` background job died with its WSL
  session on occasion) and revived by the watcher every ~15 s, which also
  covers machine restarts. The scripts are fed to `wsl -- bash` on stdin or
  called via the `/mnt/c` path — a PowerShell pipe would inject CRLF.
- **The autostart is windowless by construction**: Task Scheduler,
  `Start-Process` and even `schtasks /Create /SC ONLOGON` (access denied
  without admin) flash a console otherwise, so a Startup-folder VBS launches
  the watcher hidden. The watcher (re)finds the BUSID and runs plain
  `usbipd attach --wsl` — deliberately **not** `--auto-attach`, which blocks
  in a forever session and would starve the keeper spawn; the watcher's own
  rescan (≤5 s, plus a 2 s retry after a failed attempt) is fast enough. A
  board that is absent at logon just attaches on first sight.
- **The workspace runs as `root` deliberately**: on Windows the bind-mounted
  workspace is owned by container uid 0 on every engine (drvfs mounts `C:\`
  with forced `uid=1000;gid=1000`, which rootless Podman's default map
  renders as `0:0`; Docker Desktop mounts as root anyway), so root is the
  only owner that works engine-neutral. `chmod`/`chown` cannot heal it —
  drvfs records no ownership. Running as `vscode` would break `npm`
  (lifecycle `EPERM` on `chmod`), `vite`/`rolldown` (`EPERM copyfile`) and
  Nix's flake fetcher (`repository path … is not owned by current user`).
  `postCreateCommand` additionally registers a system-wide git
  `safe.directory` wildcard for engines where the mount stays user-owned
  (e.g. rootful Linux). If git or Nix still reports dubious ownership, the
  container predates that fix — run **Dev Containers: Rebuild Container**.
  Caveats of running as root: VS Code's server/extensions live in
  `/root/.vscode-server` (fresh download after the switch), and on rootful
  Linux engines build artifacts land root-owned on the host (Windows drvfs
  renders everything `uid=1000` regardless — a Linux-only annoyance).
- **`.devcontainer/Dockerfile`'s one-line Nix sandbox pre-seed is
  load-bearing**: the image exists only to write `/etc/nix/nix.conf`
  (`sandbox = false`) **before** the Nix feature runs — its installer
  bootstraps a build sandbox (`sethostname` needs `CAP_SYS_ADMIN`), which
  Podman build steps don't get (rootless _and_ rootful — verified). Without
  the pre-seed, `devcontainer up` fails with `error: cannot set host name: Operation not permitted`. Don't "simplify" back to `"image": …`.

### PowerShell 5.1 + `wsl.exe` argument pitfalls

Hard-won while building the scripts above — Windows PowerShell 5.1 is the
only PowerShell present on stock Windows:

- An embedded `"` inside a wsl command string makes the invocation silently
  produce no output (PS 5.1 does not escape it for native exes) — build
  quote-free `sh -c` strings instead.
- `-replace 'pat', { scriptblock }` stringifies the block into garbage
  (5.1 has no scriptblock replacements) — use `Substring`.
- Any needle you filter `Get-CimInstance` cmdlines on appears in your own
  command line; a naive self-match kills your own shell (exclude your own
  command as marker).
