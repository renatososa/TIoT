# Central ESP32-S3 N16R8

Firmware Zephyr para ESP32-S3 DevKitC con módulo WROOM-1-N16R8. Escanea,
conecta, descubre el servicio, negocia el enlace, controla la sesión y
reconstruye los paquetes fragmentados.

Los metadatos se guardan en PSRAM durante el tráfico. La salida serie se realiza
al final para no bloquear la recepción BLE.

## Compilación

```bat
build_preexperiment.bat
build_postexperiment.bat
build_network_scan.bat
```

Resultados:

```text
build\preexperiment\zephyr\zephyr.bin
build\postexperiment\zephyr\zephyr.bin
build\network_scan\zephyr\zephyr.bin
```

El target usa flash de 16 MB, PSRAM de 8 MB, modo flash DIO y consola UART0.

## Sesión unificada

Después de conectar y estabilizar el enlace:

1. genera diez pulsos iniciales por GPIO4;
2. inicia once perfiles de 20 s, separados por pausas de 1 s;
3. deja un segundo de guarda;
4. detiene el tráfico y genera diez pulsos finales;
5. recibe once registros de estadísticas y veinte marcas del XIAO;
6. imprime calibración, datos y resumen.

El cableado es:

```text
ESP32-S3 GPIO4 --- resistencia 1 kΩ --- XIAO D2 / P0.28
ESP32-S3 GND -------------------------- XIAO GND
```

## Registros

```text
D,config_id,packet_id,acquisition_time_us,tx_time_us,rx_time_us,payload_bytes,fragments
P,config_id,generated,enqueued,transmitted,queue_dropped,stop_dropped,send_errors,queue_high_watermark
S,pulse_id,group,central_time_us,xiao_time_us
C,xiao_start,central_start,xiao_end,central_end,scale_q32
R,nombre,valor
E,nombre,...
```

No cerrar la captura hasta leer `Session complete`. La guía operativa completa
está en [EJECUTAR_EXPERIMENTOS.md](../../EJECUTAR_EXPERIMENTOS.md).

## Sondeo del entorno

La variante `network_scan` no se conecta al XIAO. Realiza un inventario BLE y
Wi-Fi secuencial, conserva estadísticas de RSSI y emite dos tablas CSV al
finalizar. El procedimiento está en
[EJECUTAR_SONDEO_REDES.md](../../EJECUTAR_SONDEO_REDES.md).
