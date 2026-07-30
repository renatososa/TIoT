# Sondeo reproducible del entorno BLE y Wi-Fi

Este firmware utiliza solamente el ESP32-S3 central y genera un perfil de las
redes visibles desde la misma posición donde se ejecutará el experimento. No se
conecta a ningún dispositivo ni punto de acceso.

## Qué mide

Después de una espera inicial de 5 s realiza dos etapas secuenciales:

1. escaneo BLE activo durante 60 s;
2. escaneos Wi-Fi activos repetidos durante al menos 60 s.

Las etapas no se superponen para evitar que el sondeo simultáneo de las dos
radios del ESP32-S3 modifique directamente sus resultados.

Para BLE se guardan dirección, tipo de dirección, nombre anunciado, cantidad de
reportes, RSSI mínimo/medio/máximo y tiempos de primera y última observación.
Para Wi-Fi se guardan SSID, BSSID, banda, canal, seguridad, cantidad de
apariciones y RSSI mínimo/medio/máximo. Las filas se imprimen de mayor a menor
RSSI máximo.

## Compilar y grabar

Desde la raíz del repositorio:

```bat
compilar_sondeo_redes.bat
firmware\flash_sondeo_redes_central.bat COM11
```

El binario queda en:

```text
firmware\central\sondeo_redes_esp32s3_n16r8.bin
```

## Capturar

Abrir la captura y reiniciar el ESP32-S3:

```bat
capturar_sondeo_redes.bat COM11
```

De forma predeterminada se guarda:

```text
resultados\sondeo_redes.txt
```

También puede indicarse otro archivo:

```bat
capturar_sondeo_redes.bat COM11 resultados\sondeo_antes_experimento.txt
```

La captura termina al recibir `Session complete`. Conviene hacer el sondeo con
el central en la misma posición, orientación y alimentación del experimento y
sin mover los demás dispositivos. El archivo debe conservarse junto al log de
datos indicando fecha, hora y ubicación.

La salida contiene dos tablas fáciles de importar:

```text
CSV_BLE,event,address,address_type,name,observations,rssi_min_dbm,...
BLE,AA:BB:CC:DD:EE:FF,random,Xiao_GATT_Min,152,-57,-51.23,-46,...

CSV_WIFI,event,bssid,ssid,band,channel,security,observations,...
WIFI,11:22:33:44:55:66,red_laboratorio,2.4GHz,6,WPA2-PSK,8,...
```

Los registros `SETTING` conservan la duración, modo, intervalo y ventana del
sondeo. Los registros `SUMMARY_BLE` y `SUMMARY_WIFI` permiten verificar cuántas
identidades se almacenaron y si se superó la capacidad fija.

## Interpretación y limitaciones

- RSSI describe potencia recibida, no ocupación temporal ni interferencia.
- Un dispositivo BLE puede usar direcciones privadas rotatorias y aparecer como
  más de una identidad.
- Los reportes BLE de advertising y scan response cuentan como observaciones;
  no equivalen necesariamente a eventos físicos independientes.
- El escaneo BLE del controlador recorre los canales primarios de advertising,
  pero esta interfaz no informa en qué canal se recibió cada reporte.
- Una red Wi-Fi con varios puntos de acceso aparece una vez por BSSID, aunque
  todos compartan el mismo SSID.
- El ESP32-S3 sondea Wi-Fi en 2,4 GHz; las redes de 5 o 6 GHz no son visibles
  para este hardware.
- Un SSID oculto se registra como `<hidden>`.
- El sondeo Wi-Fi registra puntos de acceso y canal, pero no mide utilización de
  canal, retransmisiones ni tráfico de estaciones.

Por esas razones, el archivo caracteriza el entorno radioeléctrico visible y
ayuda a reproducirlo, pero no constituye un analizador de espectro.
