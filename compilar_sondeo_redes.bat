@echo off
setlocal
set "ROOT=%~dp0"

cmd /d /c call "%ROOT%central\central_min\build_network_scan.bat"
if errorlevel 1 exit /b 1

if not exist "%ROOT%firmware\central" mkdir "%ROOT%firmware\central"

copy /y "%ROOT%central\central_min\build\network_scan\zephyr\zephyr.bin" ^
  "%ROOT%firmware\central\sondeo_redes_esp32s3_n16r8.bin" >nul
if errorlevel 1 exit /b 1

echo.
echo [OK] Firmware de sondeo BLE y Wi-Fi generado:
echo   firmware\central\sondeo_redes_esp32s3_n16r8.bin
exit /b 0
