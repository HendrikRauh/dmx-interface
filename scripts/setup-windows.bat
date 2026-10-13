@echo off
setlocal EnableExtensions
rem One-shot Windows host setup for the dmx-interface devcontainer:
rem   WSL 2 platform (no user distro) + Podman CLI (rootless machine) + usbipd-win.
rem Idempotent - safe to re-run after a reboot or on an already set-up machine.
rem Board plumbing (bind, udev rule, hidden autostart) is attach-board.bat's job.
rem See WINDOWS.md for the full picture.

echo === dmx-interface Windows setup ===
echo.

rem --- Elevated (UAC): WSL features, winget installs and usbipd need admin ---
rem fltmc: reliable admin probe (net session fails when the Server service is off)
fltmc >nul 2>&1
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
echo [1/4] Installing WSL platform (no distribution)...
wsl --install --no-distribution
if errorlevel 1 (
  echo [ERROR] "wsl --install --no-distribution" failed.
  echo         If Windows asked for a reboot, reboot and re-run this script.
  pause
  exit /b 1
)

rem --- 2. Podman CLI (no Desktop GUI) ---
echo.
echo [2/4] Installing Podman CLI...
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
echo [3/4] Creating podman machine (rootless)...
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
if errorlevel 1 (
  echo [ERROR] podman machine start failed - re-run this script.
  pause
  exit /b 1
)

rem --- 4. usbipd-win (USB pass-through into the WSL 2 kernel) ---
echo.
echo [4/4] Installing usbipd-win...
winget install --id dorssel.usbipd-win --silent --accept-source-agreements --accept-package-agreements
if errorlevel 1 (
  echo [ERROR] winget could not install dorssel.usbipd-win.
  pause
  exit /b 1
)

echo.
echo === Done ===
echo Next steps:
echo   1. If Windows requested a reboot during the WSL step, reboot first.
echo   2. Plug in the board and run scripts\attach-board.bat once (UAC for the
echo      one-time usbipd bind) - it installs the udev rule and the hidden
echo      autostart; attach itself runs windowless from then on.
echo   3. VS Code: install the Dev Containers extension, set the user setting
echo      "dev.containers.dockerPath" to "podman", open this repository and run
echo      "Dev Containers: Reopen in Container".
pause
