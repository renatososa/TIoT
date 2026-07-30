@echo off

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%.") do set "APP_DIR=%%~fI"

if not defined ZEPHYR_WORKSPACE set "ZEPHYR_WORKSPACE=%USERPROFILE%\zephyrproject"
if not defined BOARD set "BOARD=esp32s3_devkitc/esp32s3/procpu"
if not defined SNIPPETS set "SNIPPETS=espressif-flash-16M espressif-psram-8M"
if not defined BUILD_DIR set "BUILD_DIR=%APP_DIR%\build"
if not defined ZEPHYR_BASE set "ZEPHYR_BASE=%ZEPHYR_WORKSPACE%\zephyr"
if not defined CMAKE_EXE set "CMAKE_EXE=cmake"
if not defined NINJA_EXE set "NINJA_EXE=ninja"
if not defined WEST_PYTHON set "WEST_PYTHON=%ZEPHYR_WORKSPACE%\.venv\Scripts\python.exe"
if not defined ZEPHYR_TOOLCHAIN_VARIANT set "ZEPHYR_TOOLCHAIN_VARIANT=zephyr"
if not defined ZEPHYR_SDK_INSTALL_DIR (
    if exist "%USERPROFILE%\zephyr-sdk-1.0.1\cmake\Zephyr-sdkConfig.cmake" (
        set "ZEPHYR_SDK_INSTALL_DIR=%USERPROFILE%\zephyr-sdk-1.0.1"
    ) else if exist "%USERPROFILE%\zephyr-sdk-1.0.0\cmake\Zephyr-sdkConfig.cmake" (
        set "ZEPHYR_SDK_INSTALL_DIR=%USERPROFILE%\zephyr-sdk-1.0.0"
    )
)

if not exist "%ZEPHYR_WORKSPACE%\.west\config" (
    echo [ERROR] No se encontro un workspace Zephyr en:
    echo         %ZEPHYR_WORKSPACE%
    echo.
    echo Defini la variable ZEPHYR_WORKSPACE o edita este archivo.
    exit /b 1
)

if not exist "%ZEPHYR_BASE%\share\zephyr-package\cmake\ZephyrConfig.cmake" (
    echo [ERROR] No se encontro ZEPHYR_BASE valido en:
    echo         %ZEPHYR_BASE%
    exit /b 1
)

if not defined ZEPHYR_SDK_INSTALL_DIR (
    echo [ERROR] No se encontro ZEPHYR_SDK_INSTALL_DIR.
    echo         Define la variable al path del SDK de Zephyr 1.0.x.
    echo         Ejemplo:
    echo         set ZEPHYR_SDK_INSTALL_DIR=%USERPROFILE%\zephyr-sdk-1.0.1
    exit /b 1
)

if not exist "%ZEPHYR_SDK_INSTALL_DIR%\cmake\Zephyr-sdkConfig.cmake" (
    echo [ERROR] No se encontro un SDK Zephyr valido en:
    echo         %ZEPHYR_SDK_INSTALL_DIR%
    exit /b 1
)

if /I "%BOARD%"=="esp32s3_devkitc/esp32s3/procpu" if not exist "%ZEPHYR_SDK_INSTALL_DIR%\gnu\xtensa-espressif_esp32s3_zephyr-elf\bin\xtensa-espressif_esp32s3_zephyr-elf-gcc.exe" (
    echo [ERROR] El SDK no tiene instalado el toolchain Xtensa para ESP32-S3.
    echo         Falta: xtensa-espressif_esp32s3_zephyr-elf
    echo         Ejecuta setup.cmd del Zephyr SDK con:
    echo         setup.cmd /t xtensa-espressif_esp32s3_zephyr-elf
    exit /b 1
)

if not exist "%ZEPHYR_WORKSPACE%\modules\hal\espressif" (
    echo [ERROR] Falta el modulo hal_espressif en el workspace:
    echo         %ZEPHYR_WORKSPACE%\modules\hal\espressif
    echo.
    echo Ejecuta en el workspace:
    echo         west blobs fetch hal_espressif
    exit /b 1
)

if exist "%ZEPHYR_WORKSPACE%\.venv\Scripts\activate.bat" (
    call "%ZEPHYR_WORKSPACE%\.venv\Scripts\activate.bat"
)

where west >nul 2>nul
if errorlevel 1 (
    echo [ERROR] El comando west no esta disponible en PATH.
    echo         Revisa si existe:
    echo         %ZEPHYR_WORKSPACE%\.venv\Scripts\west.exe
    echo.
    echo         Si no existe, activa la venv del workspace e instala west:
    echo         python -m pip install west
    exit /b 1
)

where cmake >nul 2>nul
if errorlevel 1 (
    if exist "C:\ncs\toolchains\936afb6332\opt\bin\cmake.exe" (
        set "CMAKE_EXE=C:\ncs\toolchains\936afb6332\opt\bin\cmake.exe"
    ) else if exist "C:\Program Files\CMake\bin\cmake.exe" (
        set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"
    ) else (
        echo [ERROR] El comando cmake no esta disponible en PATH.
        exit /b 1
    )
)

where ninja >nul 2>nul
if errorlevel 1 (
    if exist "C:\ncs\toolchains\936afb6332\opt\bin\ninja.exe" (
        set "NINJA_EXE=C:\ncs\toolchains\936afb6332\opt\bin\ninja.exe"
    ) else (
        echo [ERROR] El comando ninja no esta disponible en PATH.
        exit /b 1
    )
)

REM west invokes CMake and Ninja by name, so expose fallback executables in PATH.
if /I not "%CMAKE_EXE%"=="cmake" for %%I in ("%CMAKE_EXE%") do set "PATH=%%~dpI;%PATH%"
if /I not "%NINJA_EXE%"=="ninja" for %%I in ("%NINJA_EXE%") do set "PATH=%%~dpI;%PATH%"

exit /b 0
