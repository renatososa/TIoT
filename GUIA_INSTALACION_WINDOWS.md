# Guía de instalación y compilación en Windows

El proyecto usa dos entornos Zephyr:

| Destino | Entorno verificado |
| --- | --- |
| XIAO nRF52840 | nRF Connect SDK v3.3.1, toolchain `936afb6332` |
| ESP32-S3 N16R8 | Zephyr `a3e41914...`, Zephyr SDK 1.0.1 |

## 1. Herramientas base

En una terminal con `winget`:

```bat
winget install Kitware.CMake
winget install Ninja-build.Ninja
winget install Python.Python.3.12
winget install Git.Git
winget install oss-winget.dtc
winget install 7zip.7zip
```

Cerrar y abrir la terminal, y verificar:

```bat
git --version
python --version
cmake --version
ninja --version
dtc --version
```

## 2. XIAO nRF52840

Instalar nRF Connect SDK v3.3.1 y su toolchain desde **nRF Connect for VS Code →
Manage SDKs**. Con las rutas recomendadas deben existir:

```text
C:\ncs\v3.3.1\.west\config
C:\ncs\toolchains\936afb6332\opt\bin\python.exe
```

Si se instaló en otro lugar:

```bat
set NCS_ROOT=D:\ruta\v3.3.1
set NCS_TOOLCHAIN=D:\ruta\toolchains\936afb6332
```

## 3. ESP32-S3

Crear el workspace:

```bat
cd /d %USERPROFILE%
py -3.12 -m venv zephyrproject\.venv
call zephyrproject\.venv\Scripts\activate.bat
python -m pip install --upgrade pip
python -m pip install west==1.5.0
west init -m https://github.com/zephyrproject-rtos/zephyr ^
  --mr a3e41914e33e9bf0169307c8b3696bb98bc7f926 ^
  %USERPROFILE%\zephyrproject
cd /d %USERPROFILE%\zephyrproject
west update
west blobs fetch hal_espressif
cmd /c zephyr\scripts\utils\west-packages-pip-install.cmd
west zephyr-export
```

Descargar Zephyr SDK 1.0.1 para Windows, extraerlo en
`%USERPROFILE%\zephyr-sdk-1.0.1` y ejecutar:

```bat
cd /d %USERPROFILE%\zephyr-sdk-1.0.1
setup.cmd /t xtensa-espressif_esp32s3_zephyr-elf /h /c
```

Si el workspace o SDK están en otra ruta:

```bat
set ZEPHYR_WORKSPACE=D:\zephyrproject
set ZEPHYR_SDK_INSTALL_DIR=D:\zephyr-sdk-1.0.1
```

## 4. Verificar

Desde la raíz del repositorio:

```bat
verificar_entorno.bat
```

Para verificar además las dos parejas de firmware:

```bat
verificar_entorno.bat compilar
```

## 5. Compilar

Preexperimento:

```bat
compilar_preexperimento.bat
```

Experimento periódico unificado:

```bat
compilar_experimentos.bat
```

Sondeo BLE y Wi-Fi del entorno:

```bat
compilar_sondeo_redes.bat
```

Los resultados finales se copian a `firmware\central` y
`firmware\periferico`. También quedan los resultados intermedios en:

```text
central\central_min\build\preexperiment\zephyr\zephyr.bin
central\central_min\build\postexperiment\zephyr\zephyr.bin
central\central_min\build\network_scan\zephyr\zephyr.bin
periferico\peripherial_min\build\preexperiment\zephyr\zephyr.uf2
periferico\peripherial_min\build\postexperiment\zephyr\zephyr.uf2
```

## 6. Verificación de la configuración

La compilación correcta del experimento unificado debe mostrar:

- XIAO: `CONFIG_EMG_POSTEXPERIMENT=y`;
- central: flash 16 MB y PSRAM 8 MB;
- ausencia de errores de Devicetree para D4/D5;
- artefactos `zephyr.uf2` y `zephyr.bin`.

En el XIAO, `i2c1` está desactivado en ambos builds porque D4 y D5 se reservan
para los marcadores. El preexperimento no compila el módulo de marcadores ni
realiza medición energética.

## 7. Grabación

- XIAO: pulsar dos veces Reset y copiar el UF2 a la unidad USB.
- ESP32-S3: usar el script correspondiente en `firmware`, indicando el COM.

Ejemplo:

```bat
firmware\flash_experimentos_central.bat COM11
```

Consultar [EJECUTAR_PREEXPERIMENTO.md](EJECUTAR_PREEXPERIMENTO.md) o
[EJECUTAR_EXPERIMENTOS.md](EJECUTAR_EXPERIMENTOS.md) según la etapa. Para
caracterizar el entorno consultar
[EJECUTAR_SONDEO_REDES.md](EJECUTAR_SONDEO_REDES.md).

## 8. Fallos frecuentes

- `west` no existe: activar `%USERPROFILE%\zephyrproject\.venv`.
- falta `hal_espressif`: ejecutar `west update` y
  `west blobs fetch hal_espressif`.
- no se encuentra NCS: definir `NCS_ROOT` y `NCS_TOOLCHAIN`.
- una compilación interrumpida dejó procesos: esperar a que terminen `cmake` y
  `ninja` antes de repetir el script limpio.

Referencias oficiales: [Zephyr](https://docs.zephyrproject.org/latest/develop/getting_started/index.html),
[Zephyr SDK](https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html)
y [nRF Connect SDK](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/installation.html).
