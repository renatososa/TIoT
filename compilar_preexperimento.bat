@echo off
setlocal
set "ROOT=%~dp0"

cmd /d /c call "%ROOT%periferico\peripherial_min\build_preexperiment.bat"
if errorlevel 1 exit /b 1

cmd /d /c call "%ROOT%central\central_min\build_preexperiment.bat"
if errorlevel 1 exit /b 1

if not exist "%ROOT%firmware\periferico" mkdir "%ROOT%firmware\periferico"
if not exist "%ROOT%firmware\central" mkdir "%ROOT%firmware\central"

copy /y "%ROOT%periferico\peripherial_min\build\preexperiment\zephyr\zephyr.uf2" ^
  "%ROOT%firmware\periferico\preexperimento_xiao_nrf52840.uf2" >nul
if errorlevel 1 exit /b 1

copy /y "%ROOT%central\central_min\build\preexperiment\zephyr\zephyr.bin" ^
  "%ROOT%firmware\central\preexperimento_esp32s3_n16r8.bin" >nul
if errorlevel 1 exit /b 1

echo.
echo [OK] Firmware generado:
echo   firmware\periferico\preexperimento_xiao_nrf52840.uf2
echo   firmware\central\preexperimento_esp32s3_n16r8.bin
exit /b 0
