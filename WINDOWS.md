# Windows development

Everything runs inside a [development container](https://containers.dev/) — no
Nix, no toolchain and no container GUI on Windows itself. The engine stack is
the [Podman](https://podman.io/) CLI (Apache-2.0, ~30 MB) with its rootful
machine on [WSL 2](https://learn.microsoft.com/en-us/windows/wsl/), plus
[usbipd-win](https://github.com/dorssel/usbipd-win) for USB pass-through into
the machine. This keeps the host stack small and fully open-source (no Docker
Desktop), and it is the
[Espressif-documented workflow](https://docs.espressif.com/projects/vscode-esp-idf-extension/en/latest/additionalfeatures/docker-container.html)
for flashing over USB-CDC from a dev container.

macOS is unsupported: USB devices cannot reach the container VM there at all,
and the usbipd/WSL mechanism has no macOS equivalent.

## One-time setup

1. Install the prerequisites (all free, no account): enable CPU
   virtualization (AMD-V/SVM or VT-x in BIOS/UEFI), then in an
   **administrator** PowerShell run `wsl --install --no-distribution` (WSL 2
   platform only — no user distro needed; a reboot may be requested), install
   the [Podman CLI](https://podman.io/) and
   [usbipd-win](https://github.com/dorssel/usbipd-win) (e.g. `winget install RedHat.Podman` and `winget install dorssel.usbipd-win`), then create and
   start the machine — **rootful**, which is what makes the USB device nodes
   readable from the container later:

   ```powershell
   podman machine init
   podman machine set --rootful
   podman machine start
   ```

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

3. Share the board with usbipd once (elevated — UAC prompt), then attach it
   from your normal (non-elevated) shell with the board plugged in (see
   [Flashing (USB pass-through)](#flashing-usb-pass-through)):

   ```powershell
   # once per board, administrator:
   usbipd bind --busid <BUSID>
   # every session, non-elevated (BUSID from `usbipd list`):
   usbipd attach --auto-attach --wsl --busid <BUSID>
   ```

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
rootless engines. `tasks.py` prefers the live nodes under `/host-dev` (they
track hotplug re-enumeration on their own) over `/dev/ttyACM*`, which are
Podman's per-start device binds: after a flash reset those are stale inodes
and opens fail with `ENODEV`/`EACCES`.

USB/IP's VHCI driver lives in the WSL 2 kernel that the Podman machine runs
on, so once a device is attached it shows up in the machine's `/dev` — and
therefore on the container's `/host-dev`.

The board plumbing is two `usbipd` commands (see setup step 3):
`usbipd bind --busid <BUSID>` once per board (administrator, UAC), then
`usbipd attach --auto-attach --wsl --busid <BUSID>` per session (non-elevated
shell; keep that window open). The machine is rootful, so the container's
`root` really is uid 0 and opens the kernel's `0660 root:dialout` ttyACM nodes
directly via DAC override — no udev rule, no permission patching, no
in-machine daemon. `--auto-attach` re-attaches the board after every
flash-reset re-enumeration (the App↔ROM switch drops the attachment);
`inv flash` retries esptool's port open (~18 s) to bridge the gap. After a
WSL restart (or when the auto-attach window was closed) just run the attach
line again.

A plain [WSL](https://learn.microsoft.com/en-us/windows/wsl/) dev shell
remains a working alternative for flashing.

### Handing the device back to Windows

The device is owned by WSL while attached and invisible to Windows
applications. To release it, stop the auto-attach window and run
`usbipd detach --busid <BUSID>`.

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
  device cgroup may deny `open()` on the `/host-dev` device nodes (visible
  as `EPERM`) — which is why `devcontainer.json` ships
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
- **Node readability comes from the rootful machine, nothing else**: the
  machine's ttyACM nodes keep the kernel default `0660 root:dialout`, and the
  container runs as true uid 0, which opens them via DAC override. There is no
  udev rule, no poke daemon and no permission patching anywhere — the earlier
  rootless design (udev rule + synthetic `uevent` pokes + a Windows watcher)
  existed only because rootless Podman maps container root to uid 1000, and
  was removed together with the scripts that drove it. Hotplug re-enumeration
  in this WSL kernel still delivers no real uevents to userland, which is now
  irrelevant: the node appears with its final mode immediately.
- **The auto-attach window is the only "daemon"**: `usbipd attach --auto-attach --wsl --busid <BUSID>` is a foreground usbipd feature (keep
  the window open) that re-attaches the board after every re-enumeration and
  after WSL restarts. It replaces the old hidden Startup-folder watcher; it
  must run non-elevated as the user that owns the WSL distros, and it does
  not survive logoff — re-run the attach line per session.
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

Hard-won while driving `wsl.exe` from automation — Windows PowerShell 5.1
is the only PowerShell present on stock Windows:

- An embedded `"` inside a wsl command string makes the invocation silently
  produce no output (PS 5.1 does not escape it for native exes) — build
  quote-free `sh -c` strings instead.
- `-replace 'pat', { scriptblock }` stringifies the block into garbage
  (5.1 has no scriptblock replacements) — use `Substring`.
- Any needle you filter `Get-CimInstance` cmdlines on appears in your own
  command line; a naive self-match kills your own shell (exclude your own
  command as marker).
