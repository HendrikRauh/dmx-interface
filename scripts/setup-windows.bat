@echo off
setlocal EnableExtensions
rem One-shot Windows host setup for the dmx-interface devcontainer:
rem   WSL 2 platform (no user distro) + Podman CLI (rootless machine) + usbipd-win.
rem Idempotent - safe to re-run after a reboot or on an already set-up machine.
rem See README "Development container" for the full picture.

echo === dmx-interface Windows setup ===
echo.

rem --- Elevated (UAC): WSL features, winget installs and usbipd need admin ---
net session >nul 2>&1
if errorlevel 1 (
  echo Requesting administrator privileges...
  powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

rem --- Preflight: CPU virtualization must be on (BIOS/UEFI: AMD-V/SVM or VT-x) ---
for /f %%i in ('powershell -NoProfile -Command "(Get-CimInstance Win32_ComputerSystem).HypervisorPresent"') do set "HYPERV=%%i"
if /i not "%HYPERV%"=="True" (
  for /f %%i in ('powershell -NoProfile -Command "(Get-CimInstance Win32_Processor).VirtualizationFirmwareEnabled"') do set "VIRT=%%i"
  if /i not "%VIRT%"=="True" (
    echo [ERROR] Virtualization is disabled in firmware.
    echo         Reboot, enable AMD-V / SVM / Intel VT-x in BIOS/UEFI
    echo         see https://aka.ms/enablevirtualization, then re-run this script.
    pause
    exit /b 1
  )
  echo [info] Hypervisor not active yet - the WSL step below enables the
  echo        required Windows features and asks for a reboot.
)

rem --- 1. WSL platform only, no user distro (podman machine is the only distro) ---
echo.
echo [1/5] Installing WSL platform (no distribution)...
wsl --install --no-distribution
if errorlevel 1 (
  echo [ERROR] "wsl --install --no-distribution" failed.
  echo         If Windows asked for a reboot, reboot and re-run this script.
  pause
  exit /b 1
)

rem --- 2. Podman CLI (no Desktop GUI) ---
echo.
echo [2/5] Installing Podman CLI...
winget install --id RedHat.Podman --silent --accept-source-agreements --accept-package-agreements
if errorlevel 1 (
  echo [ERROR] winget could not install RedHat.Podman.
  pause
  exit /b 1
)
rem winget does not refresh PATH in this session - use the known install path.
set "PODMAN=%ProgramFiles%\RedHat\Podman\podman.exe"
if not exist "%PODMAN%" set "PODMAN=podman"

rem --- 3. Rootless podman machine (WSL 2 provider, created on first use) ---
echo.
echo [3/5] Creating podman machine (rootless)...
"%PODMAN%" machine list 2>nul | findstr /c:"podman-machine-default" >nul
if errorlevel 1 (
  "%PODMAN%" machine init
  if errorlevel 1 (
    echo [ERROR] podman machine init failed.
    pause
    exit /b 1
  )
) else (
  echo         machine already exists, skipping init.
)
"%PODMAN%" machine start

rem --- 4. usbipd-win (USB pass-through into the WSL 2 kernel) ---
echo.
echo [4/5] Installing usbipd-win...
winget install --id dorssel.usbipd-win --silent --accept-source-agreements --accept-package-agreements
if errorlevel 1 (
  echo [ERROR] winget could not install dorssel.usbipd-win.
  pause
  exit /b 1
)

rem --- 5. Board plumbing: udev rule + hidden Startup-folder autostart ---
rem The watcher waits for the board (VID 303a) and attaches it into WSL
rem forever; scripts/attach-board.bat additionally does the one-time usbipd
rem bind (UAC). No admin needed here - usbipd attach must run non-admin.
echo.
echo [5/5] Registering hidden board autostart...
wsl -d podman-machine-default -u root -- bash < "%~dp0machine-udev.sh"
if errorlevel 1 (
  echo [ERROR] Could not install the udev rule in the machine.
  pause
  exit /b 1
)
wsl -d podman-machine-default -u root -- bash < "%~dp0machine-tty-keeper.sh"
if errorlevel 1 (
  echo [WARN] Keeper did not start - the hidden watcher will retry it.
)
set "STARTUP=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup"
(
  echo ' Written by setup-windows.bat - starts the hidden board attach
  echo ' watcher board-autostart.ps1 at logon. Delete this file to remove
  echo ' autostart; pause just the attaching via the disabled marker file.
  echo Set shell = CreateObject^("WScript.Shell"^)
  echo shell.Run "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""%~dp0board-autostart.ps1""", 0, False
) > "%STARTUP%\dmx-board-attach.vbs"
if not exist "%STARTUP%\dmx-board-attach.vbs" (
  echo [ERROR] Could not write the Startup launcher.
  pause
  exit /b 1
)
wscript.exe "%STARTUP%\dmx-board-attach.vbs"

echo.
echo === Done ===
echo Next steps:
echo   1. If Windows requested a reboot during the WSL step, reboot first.
echo   2. Plug in the board and run scripts\attach-board.bat once (UAC for the
echo      one-time usbipd bind) - attach itself runs hidden from then on.
echo   3. VS Code: install the Dev Containers extension, set the user setting
echo      "dev.containers.dockerPath" to "podman", open this repository and run
echo      "Dev Containers: Reopen in Container".
pause
