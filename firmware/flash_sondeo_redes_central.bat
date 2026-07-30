@echo off
setlocal

if "%~1"=="" (
    echo Uso: flash_sondeo_redes_central.bat COM11
    exit /b 1
)

if not defined ZEPHYR_WORKSPACE set "ZEPHYR_WORKSPACE=%USERPROFILE%\zephyrproject"
set "PYTHON=%ZEPHYR_WORKSPACE%\.venv\Scripts\python.exe"
set "IMAGE=%~dp0central\sondeo_redes_esp32s3_n16r8.bin"

if not exist "%PYTHON%" (
    echo [ERROR] No se encontro Python del workspace Zephyr:
    echo         %PYTHON%
    exit /b 1
)

if not exist "%IMAGE%" (
    echo [ERROR] No se encontro el firmware:
    echo         %IMAGE%
    exit /b 1
)

"%PYTHON%" -m esptool --chip esp32s3 --port "%~1" --baud 921600 ^
  write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB ^
  0x0 "%IMAGE%"

exit /b %ERRORLEVEL%
