@echo off
call "%~dp0_ncs_env.bat"
if errorlevel 1 exit /b 1

if "%~1"=="" (
    echo Uso interno: build_experiment.bat preexperiment^|postexperiment
    echo Para compilar, use build_preexperiment.bat o build_postexperiment.bat.
    exit /b 1
)

set "PROFILE=%~1"
set "PROFILE_CONF=%APP_DIR%\experiments\%PROFILE%.conf"
set "BUILD_DIR=%APP_DIR%\build\%PROFILE%"

if not exist "%PROFILE_CONF%" (
    echo [ERROR] Perfil desconocido: %PROFILE%
    exit /b 1
)

echo [INFO] Profile:   %PROFILE%
echo [INFO] Board:     %BOARD%
echo [INFO] Build dir: %BUILD_DIR%

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

set "APP_DIR_CMAKE=%APP_DIR:\=/%"
set "BUILD_DIR_CMAKE=%BUILD_DIR:\=/%"
set "PROFILE_CONF_CMAKE=%PROFILE_CONF:\=/%"
set "ZEPHYR_BASE_CMAKE=%ZEPHYR_BASE:\=/%"
set "NCS_PYTHON_CMAKE=%NCS_PYTHON:\=/%"
set "NINJA_EXE_CMAKE=%NINJA_EXE:\=/%"
set "ZEPHYR_SDK_INSTALL_DIR_CMAKE=%ZEPHYR_SDK_INSTALL_DIR:\=/%"

"%CMAKE_EXE%" -GNinja ^
  -B "%BUILD_DIR_CMAKE%" ^
  -S "%APP_DIR_CMAKE%" ^
  -DBOARD=%BOARD% ^
  -DZEPHYR_BASE="%ZEPHYR_BASE_CMAKE%" ^
  -DCMAKE_MAKE_PROGRAM="%NINJA_EXE_CMAKE%" ^
  -DWEST_PYTHON="%NCS_PYTHON_CMAKE%" ^
  -DZEPHYR_TOOLCHAIN_VARIANT=%ZEPHYR_TOOLCHAIN_VARIANT% ^
  -DZEPHYR_SDK_INSTALL_DIR="%ZEPHYR_SDK_INSTALL_DIR_CMAKE%" ^
  -DEXTRA_CONF_FILE="%PROFILE_CONF_CMAKE%"
set "ERR=%ERRORLEVEL%"
if errorlevel 1 exit /b %ERR%

"%CMAKE_EXE%" --build "%BUILD_DIR_CMAKE%"
set "ERR=%ERRORLEVEL%"

exit /b %ERR%
