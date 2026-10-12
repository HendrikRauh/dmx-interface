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
# While a board is present, the loop also (re)starts the in-machine ttyACM
# keeper (mode 0666; udev is unreliable in the machine image) every ~15 s -
# including after a machine restart. It must stay behind the device check:
# `wsl -d ...` auto-boots a stopped machine, so poking it with no board
# plugged in would keep the machine alive forever.
#
# To hand the board back to Windows: create the marker file
# %LOCALAPPDATA%\dmx-board-attach.disabled (delete it to re-enable), then
# run `usbipd detach --busid <BUSID>`.

$log = Join-Path $env:TEMP 'dmx-board-attach.log'
$disabled = Join-Path $env:LOCALAPPDATA 'dmx-board-attach.disabled'

# One watcher per session. WaitOne(0) would throw AbandonedMutexException
# (terminating in PS) after a crashed watcher, so the script dies instead
# of taking over - the ctor+try pattern handles that case by ownership.
$mutex = New-Object System.Threading.Mutex($false, 'Local\dmx-board-attach')
try {
    if (-not $mutex.WaitOne(0)) {
        exit 0
    }
} catch [System.Threading.AbandonedMutexException] {
    # previous watcher died without releasing - we own the mutex now
}

# /mnt/c path of the keeper spawner next to this script. Drive letter must
# be lower case: WSL mounts C: at /mnt/c and paths are case-sensitive.
# (No -replace scriptblock: Windows PowerShell 5.1 stringifies it.)
$p = ($PSScriptRoot -replace '\\', '/')
$keeper = '/mnt/' + $p.Substring(0, 1).ToLowerInvariant() + $p.Substring(2) + '/machine-tty-keeper.sh'
$nextKeeper = [DateTime]::MinValue

# Start with a fresh log each session - a failing attach appends every
# couple of seconds and would grow the file forever.
[IO.File]::WriteAllText($log, '')

function Log([string]$message) {
    "$(Get-Date -Format o) $message" | Add-Content -Path $log
}

Log 'autostart: watching for VID 303a'
while ($true) {
    if (Test-Path $disabled) {
        Start-Sleep -Seconds 10
        continue
    }
    $line = usbipd list 2>$null | Select-String '303a' | Select-Object -First 1
    if ($null -eq $line) {
        Start-Sleep -Seconds 5
        continue
    }
    if ((Get-Date) -gt $nextKeeper) {
        # Plain usbipd attach below, not --auto-attach: that blocks in a
        # forever session and would starve this keeper spawn; the loop's
        # own rescan is fast enough (<=5 s, and `inv flash` waits 150 open
        # attempts for the re-attach).
        $out = wsl -d podman-machine-default -u root -- bash $keeper 2>&1
        if ($LASTEXITCODE -ne 0 -or $out) {
            Log "keeper: rc=$LASTEXITCODE $out"
        }
        $nextKeeper = (Get-Date).AddSeconds(15)
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
