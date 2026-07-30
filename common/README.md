# Protocolo experimental EMG

`emg_protocol.h` define el formato binario común al XIAO y al ESP32-S3. La
versión actual es 2 y todos los enteros multibyte se transmiten little-endian.

## Servicio GATT propio

Se usa un servicio propio porque el proyecto no transmite una magnitud clínica
ya modelada por un perfil adoptado, sino bloques continuos de muestras EMG junto
con identificadores, dos marcas temporales, fragmentación y comandos de control
del ensayo. Forzar estos datos dentro de un servicio destinado a otra clase de
sensor no aportaría interoperabilidad y ocultaría información necesaria para
medir pérdida, latencia y reconstrucción.

La interfaz GATT implementada es:

| Elemento | UUID | Propiedades | Permisos | Uso |
| --- | --- | --- | --- | --- |
| Servicio primario | `12345678-1234-5678-1234-56789abcdef0` | — | — | Agrupa el protocolo experimental |
| Característica RX | `12345678-1234-5678-1234-56789abcdef1` | Write, Write Without Response | Write | Comandos de control de 4 B, enviados por el central |
| Característica TX | `12345678-1234-5678-1234-56789abcdef2` | Notify, Read | Read | Fragmentos, estadísticas y sincronización, enviados por el XIAO |
| Descriptor CCC de TX | UUID estándar `0x2902` | — | Read, Write | Habilita las notificaciones |

El central descubre el servicio y las dos características por UUID; no depende
de handles fijos. Después habilita el CCC de TX y recién entonces inicia la
sesión. El periférico acepta una sola conexión y se anuncia con el nombre
`Xiao_GATT_Min`, incluyendo el UUID del servicio en los datos de advertising.

Estos UUID de 128 bits pertenecen al protocolo local y no están asignados por
Bluetooth SIG. Son adecuados para el prototipo coordinado —ambos extremos son
parte de este repositorio—, pero antes de publicar o integrar el sistema con
terceros conviene reemplazar la familia de ejemplo `12345678...` por UUID
propios generados y mantenerlos estables.

## Transporte

- ATT MTU: 247 B.
- Valor máximo de una notificación: 244 B (`MTU - 3`).
- Encabezado propio: 32 B.
- Datos útiles por fragmento: hasta 212 B.
- Paquete de aplicación máximo: 960 B, hasta cinco fragmentos.
- Una notificación de aplicación en vuelo; espera máxima del callback: 2 s.
- Buffers ACL preparados para Data Length de 251 B; actualización automática
  habilitada.
- Una conexión BLE simultánea.

Aunque BLE permite configurar un ATT MTU de hasta 517 B, con la configuración
de radio utilizada ese tamaño se fragmentaría en capas inferiores. Se eligió
MTU 247 para que cada notificación quepa en una PDU de Link Layer con Data
Length 251, y la fragmentación se realiza explícitamente en la aplicación.

### De dónde sale el MTU 247

`247` no es el máximo impuesto por el XIAO ni el máximo general de ATT. Es una
decisión de diseño para evitar que L2CAP fragmente una PDU ATT:

```text
máximo payload de una PDU de datos Link Layer con DLE = 251 B
encabezado Basic L2CAP                              =   4 B
ATT MTU que cabe sin fragmentación inferior        = 247 B

valor útil de una notificación:
ATT MTU                                             = 247 B
opcode ATT + handle                                 =   3 B
valor de la característica                          = 244 B

datos de muestras por fragmento de la aplicación:
valor de la característica                          = 244 B
encabezado EMG propio                               =  32 B
datos útiles                                        = 212 B
```

ATT admite valores de atributo de hasta 512 B y por eso las implementaciones
suelen permitir un ATT MTU máximo de 517 B: algunos tipos de PDU necesitan
cinco bytes además del valor. Con Link Layer limitado a 251 B, una PDU ATT mayor
de 247 B requiere segmentación y reensamblado L2CAP.

La negociación usa el menor MTU anunciado por los dos extremos. Por tanto, 247
solo es efectivo si XIAO y ESP32-S3 lo soportan y completan el intercambio; el
central imprime `ATT MTU updated: tx=247 rx=247` para comprobarlo.

Fuentes:

- [Bluetooth Core Specification: ATT, MTU y valor máximo de atributo](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host/attribute-protocol--att-.html)
- [Bluetooth Core Specification: encabezado Basic L2CAP de 4 bytes](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/host/logical-link-control-and-adaptation-protocol-specification.html)
- [Bluetooth SIG: Data Length Extension y payload de 251 bytes](https://www.bluetooth.com/blog/exploring-bluetooth-5-how-fast-can-it-be/)
- [Zephyr: configuración necesaria para actualizar el ATT MTU](https://docs.zephyrproject.org/latest/samples/bluetooth/mtu_update/README.html)
- [Nordic: DLE de hasta 251 bytes](https://docs.nordicsemi.com/r/bundle/sds_s113/page/SDS/s1xx/ble_protocol_stack/features_ble_protocol_stack.html)

### Parámetros de conexión

Los parámetros GATT y los parámetros del enlace BLE pertenecen a capas
diferentes. El servicio anterior define la interfaz de aplicación; MTU, PHY,
Data Length e intervalo se negocian en ATT/L2CAP, Host o Link Layer.

| Parámetro | Configuración actual |
| --- | --- |
| Conexiones simultáneas | 1 |
| Intervalo solicitado/preferido | 30 a 50 ms |
| Latencia del periférico | 0 |
| Timeout de supervisión preferido | 420 ms |
| ATT MTU máximo local | 247 B |
| Tamaño de buffers ACL | 251 B |
| Data Length Update | Automático, hasta la capacidad negociada |
| PHY | Capacidad y reporte habilitados; 2M no se fuerza explícitamente |

Antes de cada sesión, el central espera la negociación e imprime registros `L`
con los valores efectivos de intervalo, latencia, timeout, PHY y Data Length.
Esos registros —no solamente la configuración solicitada— deben conservarse
con los resultados. Si el experimento requiere garantizar PHY 2M, el central
debe ejecutar explícitamente `bt_conn_le_phy_update()` y abortar o etiquetar la
sesión cuando el valor negociado sea diferente.

## Encabezado de fragmento

| Offset | Bytes | Campo |
| ---: | ---: | --- |
| 0 | 2 | `magic` (`0x454d`) |
| 2 | 1 | `protocol_version` |
| 3 | 1 | `config_id` |
| 4 | 4 | `packet_id` |
| 8 | 8 | `acquisition_time_us` |
| 16 | 8 | `tx_time_us` |
| 24 | 2 | `packet_payload_len` |
| 26 | 2 | `fragment_offset` |
| 28 | 1 | `fragment_index` |
| 29 | 1 | `fragment_count` |
| 30 | 2 | `fragment_payload_len` |

El tiempo de recepción de un paquete es el instante en que el central recibe su
último fragmento; para un paquete no fragmentado coincide con la recepción de
su única notificación.

## Control y sincronización

La característica RX recibe comandos `PREPARE`, `START_DATA`, `STOP_DATA` y
`REPORT_SYNC`. El central genera diez pulsos antes del tráfico y diez después.
El periférico devuelve sus marcas locales al finalizar, sin agregar tráfico BLE
durante la medición.

Los comandos usan Write Request en la implementación del central, por lo que
reciben una respuesta ATT. Los datos usan notificaciones: el callback confirma
que Zephyr/controlador aceptó el envío, no que el paquete completo haya sido
recibido por la aplicación central. La continuidad se verifica mediante
`packet_id` y los contadores finales.

Antes de las marcas, el periférico envía un registro de estadísticas por cada
perfil ejecutado: generado, encolado, transmitido, descartes, errores y máxima
ocupación de cola.

## Configuraciones unificadas

| Orden | Perfil | `config_id` | Payload | Período |
| ---: | --- | ---: | ---: | ---: |
| 1 | S1-E20 | `0x13` | 60 B | 20 ms |
| 2 | S2-E5 | `0x21` | 60 B | 5 ms |
| 3 | S2-E10 | `0x22` | 120 B | 10 ms |
| 4 | S2-E20 | `0x23` | 240 B | 20 ms |
| 5 | S3-E5 | `0x31` | 240 B | 5 ms |
| 6 | S3-E10 | `0x32` | 480 B | 10 ms |
| 7 | S3-E20 | `0x33` | 960 B | 20 ms |
| 8 | STRESS-960-E10 | `0xb1` | 960 B | 10 ms |
| 9 | STRESS-960-E8 | `0xb2` | 960 B | 8 ms |
| 10 | STRESS-960-E7 | `0xb3` | 960 B | 7 ms |
| 11 | STRESS-960-E6 | `0xb4` | 960 B | 6 ms |

El preexperimento conserva además identificadores propios para sus siete
tamaños de payload.
