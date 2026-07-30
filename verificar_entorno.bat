@echo off
setlocal EnableDelayedExpansion

set "ERRORS=0"
set "PROJECT_ROOT=%~dp0"

if not defined ZEPHYR_WORKSPACE set "ZEPHYR_WORKSPACE=%USERPROFILE%\zephyrproject"
if not defined ZEPHYR_SDK_INSTALL_DIR set "ZEPHYR_SDK_INSTALL_DIR=%USERPROFILE%\zephyr-sdk-1.0.1"
if not defined NCS_ROOT set "NCS_ROOT=C:\ncs\v3.3.1"
if not defined NCS_TOOLCHAIN set "NCS_TOOLCHAIN=C:\ncs\toolchains\936afb6332"

echo Verificacion del entorno TIoT
echo =============================
echo.

call :check_command git "Git"

call :check_file "%ZEPHYR_WORKSPACE%\.west\config" "Workspace West del central"
call :check_file "%ZEPHYR_WORKSPACE%\.venv\Scripts\python.exe" "Python virtual del central"
call :check_file "%ZEPHYR_WORKSPACE%\.venv\Scripts\west.exe" "West del central"
call :check_file "%ZEPHYR_WORKSPACE%\zephyr\VERSION" "Fuentes Zephyr del central"
call :check_file "%ZEPHYR_WORKSPACE%\modules\hal\espressif\zephyr\module.yml" "Modulo hal_espressif"
call :check_file "%ZEPHYR_SDK_INSTALL_DIR%\sdk_version" "Zephyr SDK"
call :check_file "%ZEPHYR_SDK_INSTALL_DIR%\gnu\xtensa-espressif_esp32s3_zephyr-elf\bin\xtensa-espressif_esp32s3_zephyr-elf-gcc.exe" "Compilador Xtensa ESP32-S3"

call :check_file "%NCS_ROOT%\.west\config" "Workspace nRF Connect SDK"
call :check_file "%NCS_TOOLCHAIN%\opt\bin\python.exe" "Python del toolchain NCS"
call :check_file "%NCS_TOOLCHAIN%\opt\bin\cmake.exe" "CMake del toolchain NCS"
call :check_file "%NCS_TOOLCHAIN%\opt\bin\ninja.exe" "Ninja del toolchain NCS"
call :check_file "%NCS_TOOLCHAIN%\opt\zephyr-sdk\arm-zephyr-eabi\bin\arm-zephyr-eabi-gcc.exe" "Compilador ARM del XIAO"

echo.
echo Versiones detectadas
echo --------------------
if exist "%ZEPHYR_WORKSPACE%\.venv\Scripts\python.exe" "%ZEPHYR_WORKSPACE%\.venv\Scripts\python.exe" --version
if exist "%ZEPHYR_WORKSPACE%\.venv\Scripts\west.exe" "%ZEPHYR_WORKSPACE%\.venv\Scripts\west.exe" --version
if exist "%NCS_TOOLCHAIN%\opt\bin\python.exe" "%NCS_TOOLCHAIN%\opt\bin\python.exe" --version
if exist "%NCS_TOOLCHAIN%\opt\bin\cmake.exe" "%NCS_TOOLCHAIN%\opt\bin\cmake.exe" --version | findstr /B /C:"cmake version"
if exist "%NCS_TOOLCHAIN%\opt\bin\ninja.exe" "%NCS_TOOLCHAIN%\opt\bin\ninja.exe" --version

if exist "%ZEPHYR_WORKSPACE%\zephyr\.git" (
    for /f %%R in ('git -C "%ZEPHYR_WORKSPACE%\zephyr" rev-parse HEAD 2^>nul') do set "ZEPHYR_REV=%%R"
    echo Zephyr central: !ZEPHYR_REV!
    if /I not "!ZEPHYR_REV!"=="a3e41914e33e9bf0169307c8b3696bb98bc7f926" (
        echo [ADVERTENCIA] La revision de Zephyr no coincide con la documentada.
    )
)

if exist "%NCS_ROOT%\nrf\.git" (
    for /f %%R in ('git -C "%NCS_ROOT%\nrf" describe --tags --always 2^>nul') do set "NCS_REV=%%R"
    echo nRF Connect SDK: !NCS_REV!
    if /I not "!NCS_REV!"=="v3.3.1" (
        echo [ADVERTENCIA] La version NCS no coincide con v3.3.1.
    )
)

echo.
if not "%ERRORS%"=="0" (
    echo [ERROR] Faltan %ERRORS% componentes. Consultar GUIA_INSTALACION_WINDOWS.md.
    exit /b 1
)

echo [OK] Todas las dependencias requeridas fueron encontradas.

if /I not "%~1"=="compilar" (
    echo Para verificar las compilaciones: verificar_entorno.bat compilar
    exit /b 0
)

echo.
echo Compilando la pareja del preexperimento...
call "%PROJECT_ROOT%compilar_preexperimento.bat"
if errorlevel 1 (
    echo [ERROR] Fallo la compilacion del preexperimento.
    exit /b 1
)

echo.
echo Compilando la pareja unificada de once perfiles...
call "%PROJECT_ROOT%compilar_experimentos.bat"
if errorlevel 1 (
    echo [ERROR] Fallo la compilacion del experimento unificado.
    exit /b 1
)

echo.
echo [OK] El entorno y las dos parejas de firmware fueron verificados.
exit /b 0

:check_command
where %~1 >nul 2>nul
if errorlevel 1 (
    echo [FALTA] %~2 no esta disponible en PATH.
    set /a ERRORS+=1
) else (
    echo [OK]    %~2
)
exit /b 0

:check_file
if exist "%~1" (
    echo [OK]    %~2
) else (
    echo [FALTA] %~2
    echo         %~1
    set /a ERRORS+=1
)
exit /b 0
