# Attach the Espressif board (VID 303a) into WSL for the podman machine.
# Runs forever, hidden, at logon (Startup-folder launcher
# dmx-board-attach.vbs, written by scripts/attach-board.bat). Started by
# scripts/attach-board.bat right away; every logon is covered by the
# Startup entry. A board that is not plugged in is fine - it attaches on
# first sight; flash resets and replugs re-attach within seconds.
#
# Attach must run non-admin (usbipd issue #1008), so the launcher starts
# plain user-level powershell. Events go to %TEMP%\dmx-board-attach.log.
#
# The loop also (re)starts the in-machine ttyACM keeper (mode 0666; udev is
# unreliable in this image) every ~15 s - including after a machine restart.
#
# To hand the board back to Windows: create the marker file
# %LOCALAPPDATA%\dmx-board-attach.disabled (delete it to re-enable), then
# run `usbipd detach --busid <BUSID>`.

$log = Join-Path $env:TEMP 'dmx-board-attach.log'
$disabled = Join-Path $env:LOCALAPPDATA 'dmx-board-attach.disabled'

# One watcher per session: a second instance would only race the attach.
$mutex = New-Object System.Threading.Mutex($false, 'Local\dmx-board-attach')
if (-not $mutex.WaitOne(0)) {
    exit 0
}

# /mnt/c path of the keeper spawner next to this script. Drive letter must
# be lower case: WSL mounts C: at /mnt/c and paths are case-sensitive.
# (No -replace scriptblock: Windows PowerShell 5.1 stringifies it.)
$p = ($PSScriptRoot -replace '\\', '/')
$keeper = '/mnt/' + $p.Substring(0, 1).ToLower() + $p.Substring(2) + '/machine-tty-keeper.sh'
$nextKeeper = [DateTime]::MinValue

function Log([string]$message) {
    "$(Get-Date -Format o) $message" | Add-Content -Path $log
}

Log 'autostart: watching for VID 303a'
while ($true) {
    if (Test-Path $disabled) {
        Start-Sleep -Seconds 10
        continue
    }
    if ((Get-Date) -gt $nextKeeper) {
        # No --auto-attach here: the command must return so the keeper can
        # run on schedule; this loop's own rescan is fast enough (<=5 s,
        # and `inv flash` waits 150 open attempts for the re-attach).
        $out = wsl -d podman-machine-default -u root -- bash $keeper 2>&1
        if ($LASTEXITCODE -ne 0 -or $out) {
            Log "keeper: rc=$LASTEXITCODE $out"
        }
        $nextKeeper = (Get-Date).AddSeconds(15)
    }
    $line = usbipd list 2>$null | Select-String '303a' | Select-Object -First 1
    if ($null -eq $line) {
        Start-Sleep -Seconds 5
        continue
    }
    $busid = ($line.ToString() -split '\s+')[0]
    if ($line.ToString() -match 'Attached') {
        Start-Sleep -Seconds 3
        continue
    }
    Log "attach: busid $busid"
    usbipd attach --wsl --busid $busid 2>&1 | ForEach-Object { Log "usbipd: $_" }
    Start-Sleep -Seconds 2
}
