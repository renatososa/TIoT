@echo off
call "%~dp0_zephyr_env.bat"
if errorlevel 1 exit /b 1

if "%~1"=="" (
    echo Uso: monitor.bat COMx
    echo Ejemplo: monitor.bat COM5
    exit /b 1
)

set "ESP_PORT=%~1"

echo [INFO] Opening serial monitor on %ESP_PORT%
echo.

pushd "%ZEPHYR_WORKSPACE%"
west espressif monitor -d "%BUILD_DIR%" --esp-device "%ESP_PORT%"
set "ERR=%ERRORLEVEL%"
popd

exit /b %ERR%
