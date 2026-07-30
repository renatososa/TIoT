@echo off

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%.") do set "APP_DIR=%%~fI"

if not defined NCS_ROOT set "NCS_ROOT=C:\ncs\v3.3.1"
if not defined NCS_TOOLCHAIN set "NCS_TOOLCHAIN=C:\ncs\toolchains\936afb6332"
if not defined BOARD set "BOARD=xiao_ble/nrf52840"

set "NCS_PYTHON=%NCS_TOOLCHAIN%\opt\bin\python.exe"
set "CMAKE_EXE=%NCS_TOOLCHAIN%\opt\bin\cmake.exe"
set "NINJA_EXE=%NCS_TOOLCHAIN%\opt\bin\ninja.exe"
set "ZEPHYR_BASE=%NCS_ROOT%\zephyr"
set "ZEPHYR_SDK_INSTALL_DIR=%NCS_TOOLCHAIN%\opt\zephyr-sdk"
set "ZEPHYR_TOOLCHAIN_VARIANT=zephyr"
set "PATH=%NCS_TOOLCHAIN%\opt\bin;%PATH%"

if not exist "%NCS_ROOT%\.west\config" (
    echo [ERROR] No se encontro el workspace NCS en %NCS_ROOT%
    exit /b 1
)

if not exist "%NCS_PYTHON%" (
    echo [ERROR] No se encontro Python del toolchain NCS en %NCS_PYTHON%
    exit /b 1
)

if not exist "%CMAKE_EXE%" (
    echo [ERROR] No se encontro CMake en %CMAKE_EXE%
    exit /b 1
)

exit /b 0
