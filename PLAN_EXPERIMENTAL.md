# Plan experimental maestro

## 1. Pregunta del proyecto

¿Cómo afectan el caudal y la duración de la ventana al transporte BLE continuo
de datos tipo EMG y al consumo del periférico?

La producción de muestras es periódica e independiente de BLE. Si el enlace se
retrasa, las ventanas esperan en una cola finita; no se ralentiza el sensor.

## 2. Etapas

### 2.0 Perfil asociado del entorno radioeléctrico

En una sesión contigua al experimento se graba en el ESP32-S3 el firmware
independiente de sondeo. Desde la misma posición, orientación, alimentación y
entorno del central se capturan:

- 60 s de escaneo BLE activo;
- al menos 60 s de escaneos Wi-Fi activos repetidos en 2,4 GHz.

BLE y Wi-Fi se miden secuencialmente y el central no establece conexiones. El
log conserva identidades, canales Wi-Fi, número de observaciones y RSSI
mínimo/medio/máximo. Entre sondeo y experimento se cambia únicamente el
firmware; el perfil de redes y el log del experimento se guardan juntos.

Este sondeo caracteriza emisores visibles, no ocupación de canal ni
interferencia. Se utiliza como descriptor del entorno y no como explicación
causal automática de pérdidas.

### 2.1 Preexperimento de capacidad

El preexperimento transmite tan rápido como la pila BLE lo permite y caracteriza
payloads de 15, 30, 60, 120, 240, 480 y 960 bytes. Sirve para estimar el techo de
servicio, no representa una adquisición EMG periódica.

Este firmware es independiente y **no incorpora instrumentación de consumo**.

### 2.2 Experimento periódico unificado

Una sola conexión ejecuta, en este orden, once perfiles de 20 segundos,
separados por pausas de un segundo:

| Orden | Perfil | Payload | Período | Fragmentos |
| ---: | --- | ---: | ---: | ---: |
| 1 | S1-E20 | 60 B | 20 ms | 1 |
| 2 | S2-E5 | 60 B | 5 ms | 1 |
| 3 | S2-E10 | 120 B | 10 ms | 1 |
| 4 | S2-E20 | 240 B | 20 ms | 2 |
| 5 | S3-E5 | 240 B | 5 ms | 2 |
| 6 | S3-E10 | 480 B | 10 ms | 3 |
| 7 | S3-E20 | 960 B | 20 ms | 5 |
| 8 | STRESS-960-E10 | 960 B | 10 ms | 5 |
| 9 | STRESS-960-E8 | 960 B | 8 ms | 5 |
| 10 | STRESS-960-E7 | 960 B | 7 ms | 5 |
| 11 | STRESS-960-E6 | 960 B | 6 ms | 5 |

Se omiten S1-E5 y S1-E10 porque sus payloads de 15 y 30 bytes resultaron
ineficientes en el preexperimento. Los cuatro perfiles de estrés buscan la
transición entre carga sostenible, crecimiento de cola y pérdida.

La secuencia de datos dura 230 segundos:

```text
11 perfiles × 20 s + 10 pausas × 1 s = 230 s
```

El central agrega un segundo de guarda antes de detener la sesión. El ensayo
completo incluye además la conexión, la sincronización y el volcado posterior.

## 3. Variables

Se mantienen fijos:

- XIAO nRF52840 como periférico y ESP32-S3 N16R8 como central;
- ATT MTU 247, notificación máxima 244 B y fragmentos útiles de hasta 212 B;
- PHY 2M, Data Length 251 B y parámetros de conexión;
- cola de ocho paquetes;
- distancia, orientación, tensión y cableado;
- temporización independiente de BLE.

También se documentan como condiciones del entorno:

- fecha, hora y ubicación;
- inventario BLE y Wi-Fi previo;
- posición y orientación del central;
- ubicación de los demás equipos y fuentes de alimentación.

Se registran por perfil:

- ventanas nominales, generadas, encoladas, transmitidas y recibidas;
- descartes por cola llena, al detener y por error de envío;
- ocupación máxima de la cola;
- throughput útil;
- latencia desde adquisición y latencia de transporte;
- intervalo de recepción y jitter;
- fragmentos por paquete.

## 4. Sincronización temporal

El ESP32-S3 genera diez pulsos antes de la secuencia y diez después:

```text
ESP32-S3 GPIO4 --- resistencia 1 kΩ --- XIAO D2 / P0.28
ESP32-S3 GND -------------------------- XIAO GND
```

Las marcas de ambos relojes permiten estimar offset y deriva mediante una
transformación lineal. La corrección se hace fuera de línea.

## 5. Medición de consumo

La instrumentación energética existe solamente en el firmware periódico
unificado. El PPK2 alimenta y mide el XIAO; sus entradas digitales reciben:

| Señal | Pin XIAO | Interpretación |
| --- | --- | --- |
| `ACQUISITION_ACTIVE` | D0 / P0.02 | Generación y empaquetado de una ventana |
| `TRANSMITTER_ACTIVE` | D1 / P0.03 | Entrega de un paquete completo a BLE |
| `SYNC_INPUT` | D2 / P0.28 | Entrada de sincronización desde el central |
| `ADVERTISING_ACTIVE` | D3 / P0.29 | Advertising activo |
| `CONNECTED_ACTIVE` | D4 / P0.04 | Conexión BLE establecida |
| `PROFILE_ACTIVE` | D5 / P0.05 | Uno de los once perfiles está activo |

Todos los equipos deben compartir GND. D4 y D5 son los pines normalmente
asociados a I²C en el conector; por eso el firmware deshabilita `i2c1`.

El orden fijo permite identificar cada pulso largo de `PROFILE_ACTIVE`. Para
cada perfil se calcularán corriente media y máxima, energía total, energía por
paquete recibido y energía por byte útil. Advertising y establecimiento de la
conexión se caracterizan una vez por sesión y se informan aparte: son comunes a
los perfiles y no se atribuyen al streaming.

`TRANSMITTER_ACTIVE` delimita el trabajo del hilo de aplicación hasta entregar
todos los fragmentos a la pila. No es una medición directa de la emisión de
radio; esa actividad se observa en la corriente del PPK2.

## 6. Ejecución y comparación

Se realiza una ejecución completa de la matriz periódica. Los once perfiles
comparten la misma conexión, montaje y secuencia.

La recomendación final se basará conjuntamente en estabilidad de cola, pérdida,
latencia, jitter y energía. Un perfil no se considera adecuado si consume poco
pero no sostiene la tasa de generación.
