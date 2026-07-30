@echo off
setlocal

if "%~1"=="" (
    echo Uso: capturar_preexperimento.bat COM11 [archivo.txt]
    exit /b 1
)

set "PYTHON=%USERPROFILE%\zephyrproject\.venv\Scripts\python.exe"
if not exist "%PYTHON%" set "PYTHON=python"

if "%~2"=="" (
    "%PYTHON%" "%~dp0tools\capturar_serial.py" "%~1" --stop-on-complete
) else (
    "%PYTHON%" "%~dp0tools\capturar_serial.py" "%~1" -o "%~2" --stop-on-complete
)

exit /b %ERRORLEVEL%
