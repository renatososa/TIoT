# Análisis del preexperimento 01

Archivo analizado: `preexperimento_01.txt`.

## Validez

La ejecución es completa y válida:

```text
generados                   30869
transmitidos                30869
paquetes completos recibidos 30869
descartes                       0
errores de envío                 0
paquetes incompletos             0
fragmentos inválidos             0
fragmentos duplicados            0
desbordamiento del central       0
```

Las veinte marcas digitales fueron recibidas y se obtuvo el registro de
calibración `C`. La diferencia de escala entre relojes fue aproximadamente
`-7,58 ppm`.

Los parámetros BLE permanecieron registrados antes de medir:

```text
intervalo de conexión  50000 us
latencia periférica        0
PHY TX/RX                  2M
Data Length TX/RX         251 bytes
ATT MTU                   247 bytes
notificaciones en vuelo     1
```

## Capacidad por payload

### Significado de “envío por saturación”

En este informe, saturación describe una secuencia sin temporizador de
adquisición: cada vez que Zephyr ejecuta el callback de la notificación, el XIAO
prepara inmediatamente el paquete siguiente.

```text
encolar notificación → esperar callback ATT → encolar notificación siguiente
```

Así se mide la máxima cantidad de paquetes completos por segundo que esta
implementación puede servir para un payload determinado. La generación queda
condicionada por BLE y la capacidad obtenida actúa como techo para evaluar la
etapa periódica.

En la configuración compilada, `CONFIG_BT_ATT_SENT_CB_AFTER_TX` está
deshabilitada. El callback registra la entrega de los datos desde ATT al
controlador. La latencia extremo a extremo se obtiene de la recepción registrada
por el central. El intervalo entre callbacks caracteriza la alimentación del
controlador desde el host.

| Payload | Fragmentos | Paquetes recibidos | Capacidad | Throughput útil | Latencia transporte P95 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 15 B | 1 | 602 | 60,10 pkt/s | 7,21 kbps | 49,50 ms |
| 30 B | 1 | 804 | 80,29 pkt/s | 19,27 kbps | 48,79 ms |
| 60 B | 1 | 11864 | 1191,79 pkt/s | 572,06 kbps | 5,33 ms |
| 120 B | 1 | 9035 | 907,28 pkt/s | 870,99 kbps | 6,23 ms |
| 240 B | 2 | 4509 | 452,69 pkt/s | 869,16 kbps | 6,83 ms |
| 480 B | 3 | 2635 | 264,44 pkt/s | 1015,46 kbps | 8,79 ms |
| 960 B | 5 | 1420 | 142,43 pkt/s | 1093,83 kbps | 12,20 ms |

La capacidad no es monótona para los paquetes pequeños:

- con 15 bytes se observan aproximadamente 3 paquetes cada 50 ms;
- con 30 bytes se observan aproximadamente 4 paquetes cada 50 ms;
- con 60 bytes, después del primer evento, los callbacks y recepciones
  continúan aproximadamente cada 0,8 ms.

### Explicación del salto de 30 a 60 bytes

La inspección de la configuración y del controlador usados en esta compilación
aporta una explicación mucho más concreta:

```text
CONFIG_BT_CTLR_SDC_TX_PACKET_COUNT=3
CONFIG_BT_CTLR_SDC_MAX_CONN_EVENT_LEN_DEFAULT=7500
CONFIG_BT_CTLR_SDC_CONN_EVENT_EXTEND_DEFAULT=y
# CONFIG_BT_ATT_SENT_CB_AFTER_TX is not set
```

`BT_CTLR_SDC_TX_PACKET_COUNT=3` reserva exactamente tres buffers TX de Link
Layer por conexión. Nordic documenta que la aplicación debe reponerlos durante
el propio evento de conexión. Además, la extensión del evento permanece activa
solo mientras alguno de los pares indique que todavía tiene datos y no existan
conflictos de planificación.

Esto coincide con la forma de los datos:

1. Con 15 B el host entrega inicialmente tres notificaciones al controlador.
   Esos tres buffers se consumen y luego aparece un hueco cercano al intervalo
   de conexión de 50 ms.
2. Con 30 B suele alcanzarse a reponer un cuarto buffer, pero no se sostiene el
   flujo dentro del evento.
3. Con 60 B la reposición host-controlador llega a tiempo de mantener datos
   pendientes. El enlace puede prolongar el evento y encadenar paquetes, por
   eso los callbacks y recepciones pasan a separarse aproximadamente 0,8 ms.

El tamaño de 60 B no tiene una ventaja física: tarda más, no menos, en
transmitirse. El cambio de régimen aparece porque el tiempo de transmisión y
procesamiento modifica el instante en que el host puede reponer los tres
buffers respecto de la decisión del controlador de cerrar o continuar el
evento. Bluetooth usa el bit `MD` (*More Data*) de los paquetes Link Layer para
esa decisión. Como el ESP32-S3 desempeña el rol central, la especificación le
permite decidir si continúa el evento cuando ve `MD=1`; por eso el umbral final
es una interacción entre ambos controladores, no una decisión unilateral del
XIAO.

La relación entre los tres buffers configurados y las ráfagas de tres paquetes
está demostrada por la configuración y los timestamps. La temporización interna
exacta que produce el umbral entre 30 y 60 B sigue siendo una inferencia,
porque los planificadores internos de SoftDevice Controller y del controlador
Espressif no tienen su implementación completa en el driver HCI de Zephyr. En
el central, Zephyr inicializa el controlador Espressif con su configuración
predeterminada y se comunica con él mediante VHCI.

La prueba causal recomendada es repetir una barrida de payloads alrededor del
umbral —por ejemplo 30, 36, 42, 48, 54 y 60 B— y luego repetirla con
`CONFIG_BT_CTLR_SDC_TX_PACKET_COUNT=10`. Si el salto se desplaza o desaparece,
quedará confirmado que el límite proviene de la alimentación de buffers y no
de la radio. Una captura Link Layer con un sniffer BLE permitiría además
observar directamente el bit `MD` y el cierre de cada evento.

Fuentes consultadas:

- [Configuración del controlador Nordic: buffers TX y extensión del evento](https://github.com/nrfconnect/sdk-nrf/blob/1d7a0b0e49b8f8bc86d0f9dfc0753f6b681e11fc/subsys/bluetooth/controller/Kconfig)
- [Semántica del callback ATT en el fork Zephyr de NCS](https://github.com/nrfconnect/sdk-zephyr/blob/37e6c28576ee230ef7f8a84daa38ea3dcc9fa85c/subsys/bluetooth/host/Kconfig.gatt)
- [Driver NCS que configura SoftDevice Controller](https://github.com/nrfconnect/sdk-nrf/blob/1d7a0b0e49b8f8bc86d0f9dfc0753f6b681e11fc/subsys/bluetooth/controller/hci_driver.c)
- [Interfaz oficial de SoftDevice Controller: tres buffers TX predeterminados](https://github.com/nrfconnect/sdk-nrfxlib/blob/632dc0afa66add5d1755058da47437515a0b2f55/softdevice_controller/include/sdc.h)
- [Driver HCI de Zephyr para el controlador Bluetooth del ESP32-S3](https://github.com/nrfconnect/sdk-zephyr/blob/37e6c28576ee230ef7f8a84daa38ea3dcc9fa85c/drivers/bluetooth/hci/hci_esp32.c)
- [Bluetooth Core, cierre de eventos de conexión y bit MD](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/low-energy-controller/link-layer-specification.html#UUID-b16c62e0-c4ac-b2f7-6edf-545d43f06c37)

## Evaluación de las ventanas EMG

La utilización estimada se calculó como:

```text
utilización = tasa de paquetes requerida / capacidad medida para ese payload
```

| Perfil | Payload | Tasa requerida | Capacidad | Utilización | Evaluación |
| --- | ---: | ---: | ---: | ---: | --- |
| S1-E5 | 15 B | 200 pkt/s | 60,10 pkt/s | 332,8 % | Sobrecargada |
| S1-E10 | 30 B | 100 pkt/s | 80,29 pkt/s | 124,5 % | Sobrecargada |
| S1-E20 | 60 B | 50 pkt/s | 1191,79 pkt/s | 4,2 % | Candidata |
| S2-E5 | 60 B | 200 pkt/s | 1191,79 pkt/s | 16,8 % | Candidata |
| S2-E10 | 120 B | 100 pkt/s | 907,28 pkt/s | 11,0 % | Candidata |
| S2-E20 | 240 B | 50 pkt/s | 452,69 pkt/s | 11,0 % | Candidata |
| S3-E5 | 240 B | 200 pkt/s | 452,69 pkt/s | 44,2 % | Candidata |
| S3-E10 | 480 B | 100 pkt/s | 264,44 pkt/s | 37,8 % | Candidata |
| S3-E20 | 960 B | 50 pkt/s | 142,43 pkt/s | 35,1 % | Candidata |

Con el criterio inicial de utilización máxima de 80 %, solo S1-E5 y S1-E10
quedan descartadas como configuraciones sostenibles. El resto conserva margen
según la prueba por saturación.

## Interpretación

- **S1:** únicamente E20 resulta viable con el empaquetado actual. El resultado
  se debe al comportamiento desfavorable observado con payloads de 15 y
  30 bytes, no al caudal útil de S1.
- **S2:** las tres ventanas son viables; E5 es la primera candidata si se
  prioriza latencia.
- **S3:** las tres ventanas son viables. E5 presenta la mayor utilización
  (`44,2 %`), pero todavía conserva margen.

Las latencias medidas aquí corresponden a envío por saturación. No incluyen la
acumulación de muestras ni reproducen la fase relativa entre un temporizador
periódico y los eventos de conexión. Por eso la ventana definitiva debe
validarse con adquisición periódica y cola finita.

## Siguiente etapa

Ejecutar la matriz periódica S1/S2/S3 por E5/E10/E20 manteniendo exactamente
los parámetros BLE registrados. Se espera que S1-E5 y S1-E10 hagan crecer la
cola; conviene ejecutarlas igualmente para comprobar la predicción. La
selección definitiva se hará con pérdida, ocupación de cola, latencia extremo
a extremo y consumo energético.

## Archivos generados

- `preexperimento_01_corregido.csv`: timestamps corregidos.
- `preexperimento_01_capacidad.csv`: métricas por payload.
- `preexperimento_01_ventanas.csv`: utilización de las nueve ventanas.
