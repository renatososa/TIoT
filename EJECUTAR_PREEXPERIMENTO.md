# Ejecutar el preexperimento BLE

El preexperimento utiliza un único firmware para cada placa. No es posible
usar literalmente el mismo archivo en ambas porque el XIAO nRF52840 y el
ESP32-S3 tienen arquitecturas diferentes.

## Firmwares

```text
firmware/periferico/preexperimento_xiao_nrf52840.uf2
firmware/central/preexperimento_esp32s3_n16r8.bin
```

No mezclar estos archivos con los firmwares de los experimentos EMG.

## Secuencia automática

Con una sola notificación en vuelo, el XIAO envía tan rápido como BLE se lo
permite. Ejecuta durante 10 segundos cada payload:

| `config_id` | Payload | Fragmentos |
| --- | ---: | ---: |
| `0xA1` | 15 B | 1 |
| `0xA2` | 30 B | 1 |
| `0xA3` | 60 B | 1 |
| `0xA4` | 120 B | 1 |
| `0xA5` | 240 B | 2 |
| `0xA6` | 480 B | 3 |
| `0xA7` | 960 B | 5 |

Hay una pausa de 500 ms entre etapas. Después de conectarse, el central espera
35 segundos para que se estabilicen PHY, Data Length e intervalo de conexión.
Luego mantiene la medición durante 75 segundos y realiza una única
sincronización temporal al inicio y al final.

Antes de `Session preparing` deben aparecer registros `L,...` con el intervalo,
PHY y Data Length efectivos. El central conserva hasta 50.000 paquetes en
PSRAM y realiza un único volcado después de la sincronización final.

## Preparación

Conectar:

```text
ESP32-S3 GPIO4 --- resistencia 1 kOhm --- XIAO D2 / P0.28
ESP32-S3 GND --------------------------- XIAO GND
```

1. Pulsar dos veces Reset en el XIAO.
2. Copiar `preexperimento_xiao_nrf52840.uf2` a la unidad USB del bootloader.
3. Grabar el central, reemplazando `COM11` por su puerto:

```bat
cd firmware
flash_preexperimento_central.bat COM11
```

4. Cerrar el monitor serie del Arduino IDE y capturar desde una terminal:

```bat
cd C:\ruta\al\proyecto
capturar_preexperimento.bat COM11
```

Opcionalmente se puede indicar el nombre del archivo:

```bat
capturar_preexperimento.bat COM11 resultados\mi_preexperimento.txt
```

La terminal muestra los datos y los escribe inmediatamente en el archivo, sin
mantener todo el resultado en un búfer de pantalla. La captura termina
automáticamente después de `Session complete`; también puede detenerse con
Ctrl+C.

5. Reiniciar ambas placas si no se conectan automáticamente.

No cerrar el monitor al terminar los 75 segundos de medición. El central
imprime los registros después de recibir la sincronización final. Guardar desde
`Session preparing` hasta `Session complete`.

## Volver a compilar

Desde la raíz del repositorio:

```bat
compilar_preexperimento.bat
```

El script compila ambos dispositivos y actualiza los dos archivos del
directorio `firmware`.
