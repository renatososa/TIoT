# Análisis del sondeo BLE y Wi-Fi

## Validez de la captura

Archivo analizado: `sondeo_redes.txt`.

La captura contiene:

- configuración completa del sondeo;
- 60 s de escaneo BLE activo;
- 18 escaneos Wi-Fi completados durante al menos 60 s;
- cinco identidades BLE y un BSSID Wi-Fi;
- cero reportes fuera de la capacidad de almacenamiento;
- cierre `Session complete`.

La capacidad configurada era de 128 identidades BLE y 64 BSSID Wi-Fi, por lo
que el inventario no fue truncado.

## Resultados BLE

Se recibieron 1.703 reportes correspondientes a cinco direcciones. Cuatro
direcciones eran aleatorias y una pública. Dos dispositivos anunciaron nombre;
los otros tres quedaron sin identificar.

Por privacidad, las direcciones BLE, el BSSID, el SSID y los nombres de
dispositivos ajenos al proyecto fueron sustituidos por seudónimos consistentes.
Las métricas radioeléctricas y temporales no fueron alteradas.

| Dirección | Tipo | Nombre | Observaciones | RSSI mín./medio/máx. | Primera/última detección |
| --- | --- | --- | ---: | ---: | ---: |
| `02:00:00:00:00:01` | aleatoria | `Xiao_GATT_Min` | 525 | −81 / −39,78 / −37 dBm | 69 / 59.989 ms |
| `02:00:00:00:00:02` | aleatoria | sin nombre | 263 | −82 / −50,56 / −44 dBm | 376 / 60.000 ms |
| `02:00:00:00:00:03` | aleatoria | sin nombre | 543 | −82 / −54,06 / −46 dBm | 74 / 59.871 ms |
| `02:00:00:00:00:04` | pública | `dispositivo_vecino` | 361 | −81 / −58,03 / −52 dBm | 8 / 59.822 ms |
| `02:00:00:00:00:05` | aleatoria | sin nombre | 11 | −86 / −71,90 / −67 dBm | 3.705 / 53.826 ms |

El XIAO fue el anunciante BLE de mayor potencia: su RSSI medio fue 10,78 dB
superior al segundo dispositivo más fuerte. Se observó desde 69 ms después del
inicio hasta prácticamente el final, por lo que estuvo disponible durante todo
el sondeo. Su mínimo aislado de −81 dBm muestra que el RSSI instantáneo puede
variar mucho; para describir proximidad resulta más representativo combinar
media, máximo y persistencia.

Los valores `advertising_type_mask` del log indican que el XIAO, el dispositivo
`02:00:00:00:00:03` y `dispositivo_vecino` produjeron advertising conectable y
respuestas al escaneo activo. Las direcciones `02:00:00:00:00:02` y
`02:00:00:00:00:05` se observaron como advertising
no conectable. Una dirección aleatoria puede ser privada y cambiar entre
sondeos, por lo que no garantiza una identidad estable.

## Resultados Wi-Fi

Los 18 barridos detectaron un único BSSID:

| BSSID | SSID | Banda | Canal | Seguridad reportada | Observaciones | RSSI mín./medio/máx. |
| --- | --- | --- | ---: | --- | ---: | ---: |
| `02:00:00:00:01:01` | `red_entorno_2.4` | 2,4 GHz | 4 | `UNKNOWN` | 16 | −41 / −33,31 / −28 dBm |

El punto de acceso presentó señal fuerte en el central y apareció en 16 de los
18 escaneos completados. `UNKNOWN` significa que el controlador no clasificó la
seguridad en este resultado; no permite concluir que la red sea abierta.

## Interpretación

Durante este sondeo el entorno visible fue acotado: cinco direcciones BLE y un
único BSSID Wi-Fi de 2,4 GHz. El XIAO tuvo la señal BLE media más alta y el
punto de acceso Wi-Fi del canal 4 tuvo una señal fuerte.

Esto no cuantifica interferencia. El inventario no mide ocupación de canal,
volumen de tráfico, colisiones ni retransmisiones, y el reporte BLE no expone el
canal primario en que fue recibido. Por tanto, no corresponde atribuir las
pérdidas de los perfiles periódicos al punto de acceso o a otro anunciante BLE.

El sondeo se ejecutó en una sesión separada porque requiere otro firmware del
central, pero se mantuvieron el mismo entorno, posición, orientación,
alimentación y montaje del experimento periódico. En consecuencia, se adopta
como el perfil radioeléctrico representativo de esa ejecución y se conserva
junto al log experimental.

Esta equivalencia de condiciones permite contextualizar el experimento, pero no
elimina la limitación de la medición: visibilidad y RSSI no cuantifican la
actividad ni la interferencia producida por cada emisor.
