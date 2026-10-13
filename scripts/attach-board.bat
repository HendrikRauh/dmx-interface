@echo off
setlocal EnableExtensions
rem One-time board setup for the dmx-interface devcontainer - no helper
rem windows anymore:
rem   1. usbipd bind (one-time, UAC) so the board may be shared with WSL.
rem   2. udev rule + udevd in the podman machine: the hidden watcher pokes
rem      the rule every ~5 s while a board is attached (WSL delivers no
rem      real hotplug uevents to udevd - only the synthetic poke works).
rem   3. Startup-folder launcher (dmx-board-attach.vbs) that starts the
rem      hidden attach watcher board-autostart.ps1 at every logon - usbipd
rem      attach must run non-admin (usbipd issue #1008), so no elevated
rem      scheduled task and no console window. Re-run this script freely;
rem      everything is idempotent. Plug the board in, forget about it.

echo === dmx-interface board attach ===

rem --- Locate the Espressif USB device (VID 303a) ---
set "BUSID="
for /f "tokens=1" %%i in ('usbipd list 2^>nul ^| findstr /c:"303a"') do set "BUSID=%%i"
if not defined BUSID (
  echo [ERROR] No Espressif device ^(VID 303a^) found. Plug in the board and retry.
  pause
  exit /b 1
)
echo Found board on BUSID %BUSID%.

rem --- Share once if needed (bind requires elevation) ---
usbipd list | findstr /c:"%BUSID%" | findstr /c:"Not shared" >nul
if not errorlevel 1 (
  echo Binding %BUSID% once - accepting the UAC prompt...
  powershell -NoProfile -Command "Start-Process usbipd -ArgumentList 'bind','--busid','%BUSID%' -Verb RunAs -Wait"
  usbipd list | findstr /c:"%BUSID%" | findstr /c:"Not shared" >nul
  if not errorlevel 1 (
    echo [ERROR] Bind did not stick - re-run this script and accept UAC.
    pause
    exit /b 1
  )
)

rem --- udev rule + udevd in the machine: 0666 via uevent poke ---
echo Installing udev rule in the podman machine...
wsl -d podman-machine-default -u root -- bash < "%~dp0machine-udev.sh"
if errorlevel 1 (
  echo [WARN] udev setup did not run - the hidden watcher will retry it.
)

rem --- Hidden autostart: Startup-folder VBS (no admin, no window) ---
set "STARTUP=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup"
echo Writing autostart launcher %STARTUP%\dmx-board-attach.vbs ...
(
  echo ' Written by attach-board.bat - starts the hidden board attach watcher
  echo ' board-autostart.ps1 at logon. Delete this file to remove autostart;
  echo ' pause just the attaching via %LOCALAPPDATA%\dmx-board-attach.disabled.
  echo Set shell = CreateObject^("WScript.Shell"^)
  echo shell.Run "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""%~dp0board-autostart.ps1""", 0, False
) > "%STARTUP%\dmx-board-attach.vbs"
if not exist "%STARTUP%\dmx-board-attach.vbs" (
  echo [ERROR] Could not write the Startup launcher.
  pause
  exit /b 1
)
rem Start it right away (next logons are covered by the Startup entry).
wscript.exe "%STARTUP%\dmx-board-attach.vbs"

timeout /t 6 /nobreak >nul
echo.
echo Device state:
usbipd list | findstr /c:"%BUSID%"
echo Node inside the podman machine:
wsl -d podman-machine-default -- sh -c "ls -l /dev/ttyACM* 2>/dev/null || echo '(no ttyACM node visible yet)'"
echo.
echo Done - the attach watcher runs hidden and restarts at every logon.
echo To hand the board back to Windows: create the file
echo   %LOCALAPPDATA%\dmx-board-attach.disabled
echo ^(delete it to re-attach^) and run: usbipd detach --busid %BUSID%
pause
