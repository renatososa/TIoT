<div align="center">

<h1>Enlace BLE para señales tipo EMG</h1>

<p><strong>Caracterización experimental de empaquetado, latencia, pérdida,
cola y consumo para adquisición EMG periódica.</strong></p>

<p>
  <img alt="Zephyr 4.4" src="https://img.shields.io/badge/RTOS-Zephyr_4.4-6C2DC7?style=flat-square">
  <img alt="Bluetooth LE PHY 2M" src="https://img.shields.io/badge/Bluetooth_LE-PHY_2M-0082FC?style=flat-square">
  <img alt="XIAO nRF52840" src="https://img.shields.io/badge/Periferico-XIAO_nRF52840-00A98F?style=flat-square">
  <img alt="ESP32-S3 N16R8" src="https://img.shields.io/badge/Central-ESP32--S3_N16R8-E7352C?style=flat-square">
  <img alt="Medición energética pendiente" src="https://img.shields.io/badge/Estado-Energia_pendiente-F0A500?style=flat-square">
</p>

<p>
  <a href="GUIA_INSTALACION_WINDOWS.md">Instalación</a> ·
  <a href="#inicio-rápido">Inicio rápido</a> ·
  <a href="#documentación">Documentación</a> ·
  <a href="#resultados-principales">Resultados</a> ·
  <a href="#1-introducción">Informe técnico</a>
</p>

</div>

## Acerca del proyecto

Este repositorio contiene los firmwares, protocolo, herramientas de captura y
resultados necesarios para evaluar el transporte BLE de flujos periódicos con
caudales representativos de electromiografía de superficie.

### Características

- Adquisición periódica independiente de la disponibilidad de BLE.
- Escenarios de 1, 4 y 8 canales a 1–2 kHz con palabras de 24 bits.
- Cola finita, fragmentación explícita y reconstrucción de paquetes.
- Sincronización GPIO y corrección de offset y deriva entre relojes.
- Preexperimento de saturación y once perfiles periódicos unificados.
- Sondeo reproducible del entorno BLE y Wi-Fi.
- Procesamiento en Python de pérdida, throughput, latencia, jitter y cola.
- Marcadores digitales preparados para medición energética con PPK2.

## Inicio rápido

Los comandos se ejecutan desde la raíz del repositorio en Windows:

| Acción | Comando | Resultado |
| --- | --- | --- |
| Verificar herramientas | `verificar_entorno.bat` | Comprueba Zephyr, SDK, Python y toolchains |
| Compilar sondeo de redes | `compilar_sondeo_redes.bat` | Binario BLE/Wi-Fi para el ESP32-S3 |
| Compilar preexperimento | `compilar_preexperimento.bat` | Firmwares de caracterización por saturación |
| Compilar experimento | `compilar_experimentos.bat` | Firmwares de los once perfiles periódicos |

> Los binarios generados quedan en [`firmware/`](firmware/). La instalación
> inicial y la grabación de cada placa se explican en la
> [guía de instalación](GUIA_INSTALACION_WINDOWS.md).

## Documentación

| Documento | Propósito |
| --- | --- |
| [Guía de instalación](GUIA_INSTALACION_WINDOWS.md) | Preparar y verificar el entorno de compilación |
| [Plan experimental](PLAN_EXPERIMENTAL.md) | Definir etapas, variables y criterios |
| [Sondeo BLE y Wi-Fi](EJECUTAR_SONDEO_REDES.md) | Caracterizar el entorno radioeléctrico |
| [Ejecutar preexperimento](EJECUTAR_PREEXPERIMENTO.md) | Medir la capacidad por tamaño de payload |
| [Ejecutar experimento](EJECUTAR_EXPERIMENTOS.md) | Grabar, capturar y procesar los once perfiles |
| [Protocolo compartido](common/README.md) | Consultar GATT, MTU y formatos binarios |
| [Análisis periódico](resultados/EXPERIMENTOS_UNIFICADOS_ANALISIS.md) | Revisar las métricas completas de la ejecución |
| [Análisis del entorno](resultados/SONDEO_REDES_ANALISIS.md) | Revisar el inventario BLE y Wi-Fi |

## Resultados principales

El tamaño de ventana no puede elegirse únicamente a partir del throughput:
también determina cuántos paquetes compiten por cada evento de conexión, cuánto
crece la cola y qué latencia se acumula.

| Escenario | Caudal objetivo | Perfil sostenible evaluado | Resultado |
| --- | ---: | --- | --- |
| S1: 1 canal × 1 kHz × 24 bits | 24 kbps | S1-E20 | 60 B cada 20 ms, sin pérdida |
| S2: 4 canales × 1 kHz × 24 bits | 96 kbps | S2-E10 y S2-E20 | Ambos sin pérdida; falta comparar energía |
| S3: 8 canales × 2 kHz × 24 bits | 384 kbps | S3-E20 | 960 B cada 20 ms, sin pérdida |

> **Estado actual:** el transporte y el análisis temporal están completos. La
> selección final entre las ventanas sostenibles queda pendiente de la medición
> energética con PPK2.

<details>
<summary><strong>Contenido del informe técnico</strong></summary>

<br>

1. [Introducción](#1-introducción)
2. [Objetivo](#2-objetivo)
3. [Metodología](#3-metodología)
4. [Resultados del sondeo radioeléctrico](#4-resultados-del-sondeo-radioeléctrico)
5. [Resultados del preexperimento](#5-resultados-del-preexperimento)
6. [Resultados del experimento periódico](#6-resultados-del-experimento-periódico-unificado)
7. [Estado y conclusiones](#7-estado-y-conclusiones)
8. [Referencias](#8-referencias)

</details>

---

## 1. Introducción

Este proyecto estudia cómo empaquetar y transportar por Bluetooth Low Energy
(BLE) un flujo de muestras representativo de electromiografía de superficie
(sEMG). El periférico es un XIAO nRF52840 y el central un ESP32-S3 N16R8; ambos
ejecutan Zephyr.

El problema no consiste solamente en comprobar si un paquete BLE puede cruzar
el enlace. Un sensor EMG adquiere muestras a una frecuencia fija, aunque la
radio tarde en atenderlas. Por lo tanto, una arquitectura útil debe responder
cuánto tráfico produce la señal, qué cantidad de muestras conviene reunir por
paquete y qué ocurre cuando BLE sirve los paquetes más lentamente de lo que se
generan.

Para conservar esa independencia, un temporizador genera ventanas periódicas,
una cola finita absorbe variaciones temporales y otro hilo las fragmenta y
transmite. Así, una demora del enlace se observa como acumulación de latencia o
descarte, no como una reducción artificial de la frecuencia del sensor.

```mermaid
flowchart LR
    A["Muestreo periódico"] --> B["Ventana de muestras"]
    B --> C["Cola finita<br/>8 paquetes"]
    C --> D["Fragmentación<br/>hasta 212 B útiles"]
    D --> E["Notificaciones BLE"]
    E --> F["Reconstrucción<br/>ESP32-S3"]
    F --> G["Corrección temporal<br/>y métricas"]
```

El firmware no conecta todavía un front-end analógico ni adquiere una señal
fisiológica: genera bytes deterministas con el mismo caudal que producirían
varias configuraciones de EMG. Por eso los resultados caracterizan la
arquitectura de adquisición, empaquetado y comunicación, pero no la calidad de
una señal EMG, su relación señal-ruido ni su aptitud diagnóstica.

### Estructura del repositorio

| Ruta | Contenido |
| --- | --- |
| [`periferico/peripherial_min/`](periferico/peripherial_min/) | Firmware del XIAO nRF52840 |
| [`central/central_min/`](central/central_min/) | Firmware del ESP32-S3 central |
| [`common/`](common/) | Protocolo binario compartido |
| [`tools/`](tools/) | Captura serie, corrección temporal y análisis |
| [`firmware/`](firmware/) | Binarios compilados y scripts de grabación |
| [`resultados/`](resultados/) | Capturas, CSV e informes derivados |

---

## 2. Objetivo

Determinar qué ventanas de transporte permiten sostener, sin pérdida, distintos
caudales representativos de EMG sobre esta implementación BLE, y comparar el
compromiso entre latencia, frecuencia de paquetes, ocupación de cola,
fragmentación, throughput y consumo energético.

La pregunta de diseño es: dado un número de canales, una frecuencia de muestreo
y un formato de muestra, ¿cuántos milisegundos de señal deben agruparse antes de
enviar cada paquete?

---

## 3. Metodología

### De la señal EMG al caudal de datos

La sEMG posee contenido espectral hasta aproximadamente 450–500 Hz. Por el
criterio de Nyquist, 1 kHz queda en la frontera teórica para representar esa
banda y deja muy poco margen para el filtro antialias; las guías consultadas
indican que en la práctica se utilizan frecuencias superiores a 1,5 kHz y se
prefieren 2 kHz o 2048 Hz. En consecuencia, el proyecto usa 1 kHz como caso
base de menor caudal y 2 kHz como caso preferido de mayor fidelidad y demanda
([Merletti y Cerone, 2020](https://doi.org/10.1016/j.jelekin.2020.102440)).

La cantidad de canales depende de la aplicación. Un canal diferencial permite
seguir un músculo o una localización; varios canales permiten observar
simultáneamente distintos músculos o posiciones. Como referencia de escala, la
base NinaPro DB2 emplea 12 electrodos sEMG a 2 kHz para movimientos de mano y
CapgMyo utiliza una matriz de alta densidad de 128 canales a 1 kHz. El proyecto
no intenta reproducir por completo esos sistemas: selecciona 1, 4 y 8 canales
para recorrer desde un nodo mínimo hasta un dispositivo multicanal compacto.
Cuatro canales constituyen un punto intermedio de ingeniería, no un estándar
clínico ([Atzori et al., 2014](https://doi.org/10.1038/sdata.2014.53);
[Du et al., 2017](https://doi.org/10.3390/s17030458)).

Cada muestra se representa con una palabra de 24 bits, es decir, tres bytes.
Esta elección modela la salida sin comprimir de front-ends multicanal para
biopotenciales como la familia ADS1299, disponible con 4, 6 u 8 ADC simultáneos
de 24 bits y tasas desde 250 SPS hasta 16 kSPS. No significa que una EMG
necesite 24 bits efectivos: la bibliografía recomienda al menos 12 bits y
prefiere 16 bits para una cadena correctamente amplificada. Tampoco deben
confundirse los 24 bits nominales del convertidor con su número efectivo de bits,
que depende del ruido, la ganancia y el rango de entrada
([ADS1299](https://www.ti.com/product/ADS1299);
[Merletti y Cerone, 2020](https://doi.org/10.1016/j.jelekin.2020.102440)).

Con estos supuestos, el caudal útil antes de agregar encabezados se calcula como:

```text
caudal [B/s] = canales × frecuencia_muestreo [muestras/s] × 3 B/muestra
caudal [kbps] = caudal [B/s] × 8 / 1000
```

| Escenario | Interpretación | Canales | Muestreo | Palabra | Caudal útil |
| --- | --- | ---: | ---: | ---: | ---: |
| S1 | adquisición mínima de un punto muscular | 1 | 1 kHz | 24 bits | 3.000 B/s = 24 kbps |
| S2 | nodo multicanal compacto | 4 | 1 kHz | 24 bits | 12.000 B/s = 96 kbps |
| S3 | nodo multicanal de mayor demanda | 8 | 2 kHz | 24 bits | 48.000 B/s = 384 kbps |

Estos valores son caudales de muestras. El tráfico real sobre la radio es mayor
por el encabezado experimental y por los encabezados ATT, L2CAP y Link Layer.

### De la ventana temporal al tamaño de paquete

Una **ventana de transporte** reúne las muestras adquiridas durante un período
antes de entregarlas al transmisor. No es una ventana de análisis EMG para
extraer características o clasificar gestos: en este proyecto no se procesa la
señal y todos los valores crudos deben llegar al central.

Para una ventana de duración `W`:

```text
muestras por canal = frecuencia_muestreo × W / 1000
payload [B] = canales × frecuencia_muestreo × W / 1000 × 3
paquetes por segundo = 1000 / W
```

Se eligieron 5, 10 y 20 ms para comparar tres compromisos de transporte. Una
ventana corta hace disponibles las muestras antes, pero multiplica la cantidad
de paquetes y el costo de sus encabezados; una ventana larga amortiza ese costo,
pero debe completarse antes de poder enviarse y agrega latencia de formación.
Diez milisegundos funciona como punto intermedio. Los tamaños derivados, no
elegidos arbitrariamente, son:

| Escenario | 5 ms | 10 ms | 20 ms |
| --- | ---: | ---: | ---: |
| S1: 1 canal, 1 kHz | 15 B, 200 pkt/s | 30 B, 100 pkt/s | 60 B, 50 pkt/s |
| S2: 4 canales, 1 kHz | 60 B, 200 pkt/s | 120 B, 100 pkt/s | 240 B, 50 pkt/s |
| S3: 8 canales, 2 kHz | 240 B, 200 pkt/s | 480 B, 100 pkt/s | 960 B, 50 pkt/s |

### Etapa 1: caracterización BLE

Antes de simular una adquisición periódica se ejecuta un preexperimento de
saturación. Este envía paquetes tan rápido como puede la implementación y mide
la capacidad para los siete payloads únicos de la tabla anterior: 15, 30, 60,
120, 240, 480 y 960 bytes. Su propósito es separar el efecto del tamaño de
paquete de la frecuencia de adquisición, descubrir a partir de qué tamaño se
utiliza eficientemente el enlace y obtener un techo de referencia.

El preexperimento no representa un sensor: no impone una frecuencia de
adquisición, no utiliza la cola periódica y no incluye instrumentación de
consumo. Por eso su throughput máximo no se interpreta como caudal periódico
sostenible.

### Etapa 2: generación periódica

Con la caracterización anterior se descartan S1-E5 y S1-E10. Ambos serían
caudales EMG válidos, pero sus payloads de 15 y 30 B mostraron una utilización
anómala y muy baja en saturación; S1-E20 conserva el mismo caudal de 24 kbps con
un paquete más eficiente. Se mantienen las tres ventanas de S2 y S3 para
observar si una misma tasa de muestras puede ser sostenible o no según cómo se
empaquete.

El firmware unifica esos siete perfiles EMG y cuatro perfiles de estrés en una
sola sesión. Cada perfil dura 20 segundos y hay una pausa de un segundo entre
perfiles. La duración es un compromiso operativo que produce entre 1.000 y
4.000 ventanas nominales por perfil, según su período, sin volver
inmanejable la captura serie:

| Orden | Perfil | Relación con EMG | Payload | Período | Carga ofrecida |
| ---: | --- | --- | ---: | ---: | ---: |
| 1 | S1-E20 | 1 canal × 1 kHz × 24 bits | 60 B | 20 ms | 24 kbps |
| 2 | S2-E5 | 4 canales × 1 kHz × 24 bits | 60 B | 5 ms | 96 kbps |
| 3 | S2-E10 | 4 canales × 1 kHz × 24 bits | 120 B | 10 ms | 96 kbps |
| 4 | S2-E20 | 4 canales × 1 kHz × 24 bits | 240 B | 20 ms | 96 kbps |
| 5 | S3-E5 | 8 canales × 2 kHz × 24 bits | 240 B | 5 ms | 384 kbps |
| 6 | S3-E10 | 8 canales × 2 kHz × 24 bits | 480 B | 10 ms | 384 kbps |
| 7 | S3-E20 | 8 canales × 2 kHz × 24 bits | 960 B | 20 ms | 384 kbps |
| 8 | STRESS-960-E10 | estrés de transporte | 960 B | 10 ms | 768 kbps |
| 9 | STRESS-960-E8 | estrés de transporte | 960 B | 8 ms | 960 kbps |
| 10 | STRESS-960-E7 | estrés de transporte | 960 B | 7 ms | 1.097 kbps |
| 11 | STRESS-960-E6 | estrés de transporte | 960 B | 6 ms | 1.280 kbps |

Los primeros siete representan los perfiles EMG seleccionados. Los cuatro
últimos conservan el mayor payload, para amortizar encabezados, y aumentan su
frecuencia para localizar el límite sostenible. No representan nuevas
configuraciones de sensor: son cargas artificiales destinadas a separar el
límite de la comunicación de los tres escenarios EMG.

La producción y el envío se ejecutan en hilos separados. El temporizador no
espera a BLE: coloca cada ventana en una cola de ocho posiciones y, si está
llena, registra un descarte. La profundidad ocho es un parámetro de esta
implementación, no un límite de BLE. Cada posición reserva el payload máximo de
960 B más metadatos, por lo que la cola consume aproximadamente 7,8 kB. Se
mantiene fija para comparar todos los perfiles bajo la misma capacidad de
absorción; aumentarla podría reducir pérdidas breves a costa de RAM y de una
latencia acumulada mayor, no elevaría el caudal sostenible en régimen
permanente.

### Transporte y tiempo

Se definió un servicio GATT propio porque no se transporta una medida clínica
aislada, sino ventanas binarias de muestras con control experimental,
timestamps, identificadores y fragmentación. Los servicios estandarizados para
otras clases de sensores no representan esos campos ni harían interoperable
este protocolo. Como los dos extremos pertenecen al mismo sistema, una interfaz
propia permite preservar exactamente la información necesaria para reconstruir
un paquete y medirlo. Esto sacrifica interoperabilidad directa con aplicaciones
BLE genéricas: un receptor debe implementar este protocolo. La especificación
completa de UUID, características, propiedades, permisos y formato binario está
en [common/README.md](common/README.md).

Los datos se envían mediante notificaciones GATT. Una notificación evita la
confirmación ATT de una indicación y permite mayor caudal; en contrapartida, su
callback confirma que la pila local aceptó el dato, no que la aplicación central
lo recibió. La recepción se verifica mediante identificadores consecutivos,
reconstrucción y contadores al final de cada perfil.

Se usa ATT MTU 247 porque la extensión Data Length permite 251 B en una PDU de
datos de Link Layer y el encabezado Basic L2CAP ocupa 4 B. Así, una PDU ATT de
247 B cabe sin segmentación L2CAP. Cada notificación ofrece 244 B (`MTU - 3`),
de los cuales 32 corresponden al encabezado experimental; quedan 212 B de
muestras por fragmento:

```text
Data Length de Link Layer                   251 B
- encabezado Basic L2CAP                     4 B
= ATT MTU sin segmentación inferior         247 B

ATT MTU                                    247 B
- opcode y handle de notificación             3 B
- encabezado experimental                    32 B
= muestras por fragmento                    212 B
```

ATT puede configurarse hasta 517 B, pero ese es un máximo lógico y no significa
que el valor viaje en una sola PDU de radio. Con Data Length 251, un MTU mayor
introduciría segmentación L2CAP. El proyecto prefiere un MTU de 247 y realiza
una fragmentación de aplicación explícita, visible y medible. Por ello, los
payloads de 15, 30, 60 y 120 B usan un fragmento; 240 B usa dos; 480 B usa tres
y 960 B usa cinco.

El transmisor mantiene un paquete de aplicación en curso y espera el callback
de cada notificación antes de entregar el fragmento siguiente. Esta decisión
simplifica la reconstrucción, acota el estado y permite asociar el tiempo de
transporte a un paquete completo, pero también forma parte del sistema
caracterizado: los resultados no representan el máximo teórico de cualquier
implementación BLE que permita más operaciones simultáneas.

Para que la comparación responda principalmente al empaquetado, todos los
perfiles comparten la misma conexión. En la ejecución analizada se negociaron
PHY 2M, Data Length 251 B, ATT MTU 247, latencia de periférico cero e intervalo
de conexión de 50 ms. Este último establece cada cuánto se abren oportunidades
programadas de comunicación; no obliga a que cada paquete espere siempre 50 ms,
pero agrupa el servicio del enlace y puede producir ráfagas. En 50 ms se generan
10 ventanas de 5 ms, 5 de 10 ms o 2–3 de 20 ms, lo que ayuda a explicar por qué
dos perfiles con el mismo caudal útil no ejercen la misma presión sobre una cola
de ocho posiciones.

El tiempo de recepción de un paquete se define cuando llega su último fragmento;
si no hay fragmentación, es la recepción de su única notificación. Como cada
microcontrolador posee un reloj local independiente, comparar directamente sus
timestamps introduciría offset y deriva. El central genera diez pulsos GPIO
antes de la secuencia y diez después. El XIAO marca esos mismos flancos y los
devuelve fuera de la fase de datos, de modo que la sincronización no agrega
tráfico BLE durante los perfiles:

```text
ESP32-S3 GPIO4 --- resistencia 1 kΩ --- XIAO D2 / P0.28
ESP32-S3 GND -------------------------- XIAO GND
```

Dos grupos permiten ajustar una transformación lineal: el primero fija el
origen relativo y la separación entre los grupos estima la diferencia de
frecuencia. Diez pulsos por grupo reducen la influencia de la variación de una
única interrupción. El método alinea las escalas temporales para el análisis,
pero no modifica físicamente ninguno de los relojes.

### Métricas y criterios de cálculo

Ninguna métrica aislada decide la ventana. El throughput recibido puede ser alto
aunque se descarten muestras; una pérdida nula puede ocultar una cola casi llena;
y una media de latencia puede ocultar una cola larga. Por eso se combinan
contadores del productor, recepción extremo a extremo, ocupación de cola y
distribuciones temporales.

Cada paquete completo conserva tres instantes:

| Timestamp | Dispositivo | Definición |
| --- | --- | --- |
| `acquisition_time_us` | XIAO | Comienzo de la ventana de muestras |
| `tx_time_us` | XIAO | Momento en que el transmisor comienza a entregar el primer fragmento a GATT |
| `rx_time_us` | ESP32-S3 | Recepción del último fragmento y reconstrucción completa del paquete |

Los timestamps del XIAO se convierten al reloj del central mediante una
transformación lineal calculada con los promedios de los pulsos iniciales y
finales:

```text
t_xiao_corregido = central_inicio
                 + (t_xiao - xiao_inicio)
                 × (central_fin - central_inicio)
                 / (xiao_fin - xiao_inicio)
```

Esta corrección compensa tanto el offset inicial como la diferencia de
frecuencia entre los dos relojes. No se modifican los contadores físicos de los
microcontroladores.

Además de corregir cada paquete, se informan los parámetros de calibración:

```text
offset_inicial = central_inicio - xiao_inicio
offset_final   = central_fin - xiao_fin

escala = (central_fin - central_inicio)
         / (xiao_fin - xiao_inicio)

deriva_escala [ppm] = (escala - 1) × 10⁶
```

El offset depende principalmente del instante en que arrancó cada
microcontrolador; no representa por sí mismo un error del enlace. Su cambio
entre el inicio y el final evidencia la deriva acumulada. Una
`deriva_escala` negativa significa que, durante el ensayo, el contador del XIAO
avanzó ligeramente más rápido que el del central.

#### Producción, pérdida y throughput

El XIAO informa por perfil:

- `generated`: ventanas producidas por el temporizador;
- `enqueued`: ventanas incorporadas a la cola;
- `transmitted`: paquetes entregados completamente a la pila BLE;
- `queue_dropped`: ventanas descartadas porque la cola estaba llena;
- `stop_dropped`: ventanas pendientes descartadas al terminar el perfil;
- `send_errors`: errores al entregar un paquete a BLE;
- `queue_high_watermark`: máxima cantidad de posiciones ocupadas.

La pérdida extremo a extremo se calcula contra lo realmente generado:

```text
pérdida (%) = (paquetes_generados - paquetes_recibidos)
              / paquetes_generados × 100
```

De esta forma también se contabilizan las ventanas que se pierden antes de
llegar al enlace y que no pueden detectarse observando solamente saltos de
`packet_id`.

El throughput informado es útil: cuenta únicamente bytes de muestras recibidos,
sin encabezados ATT, L2CAP, Link Layer ni el encabezado experimental:

```text
throughput_útil (kbps) =
    bytes_útiles_recibidos × 8 / duración_del_perfil / 1000
```

Se utilizan kilobits decimales (`1 kbps = 1000 bit/s`) y una duración nominal de
20 s por perfil. Se compara con la **carga útil ofrecida**, calculada como
`payload × 8 / período`: si la pérdida es nula ambos valores deben coincidir,
salvo el pequeño efecto de la duración real de la etapa. Si existe pérdida, un
throughput recibido alto no demuestra sostenibilidad.

#### Cola y latencias

La ocupación máxima se expresa como posiciones utilizadas sobre las ocho
disponibles. Alcanzar `8/8` indica que la cola llegó a llenarse, aunque no
informa por sí solo durante cuánto tiempo permaneció llena.

Las latencias se calculan después de corregir los timestamps:

```text
latencia_pre_envío = tx_time_corregido - acquisition_time_corregido

latencia_de_transporte = rx_time - tx_time_corregido

latencia_extremo_a_extremo =
    rx_time - acquisition_time_corregido
  = latencia_pre_envío + latencia_de_transporte
```

La latencia pre-envío incluye la formación temporal de la ventana, la
generación y el empaquetado, la planificación de los hilos y la eventual espera
en cola. No es posible aislar la espera pura en cola con el formato actual
porque no se almacena un timestamp de encolado. La latencia de transporte
incluye la espera por una oportunidad BLE, el envío de todos los fragmentos y
la reconstrucción en el central. No representa una confirmación de recepción
proporcionada por la notificación GATT.

Para cada latencia se informa:

- **media:** promedio aritmético de todos los paquetes recibidos;
- **P95:** percentil por rango más próximo; al menos el 95 % de las
  observaciones es menor o igual a ese valor;
- **máximo:** conservado en el CSV de métricas para diagnosticar valores
  extremos.

Las tablas del informe muestran `media / P95`.

#### Intervalo de recepción y jitter

Para paquetes consecutivos recibidos:

```text
delta_rx = rx_time_actual - rx_time_anterior
```

Cuando existen descartes, dos paquetes recibidos pueden estar separados por más
de un período de generación. Para no confundir esa pérdida con jitter:

```text
jitter =
    |delta_rx - delta_packet_id × período_de_generación|
```

Se calculan media, desviación estándar y P95 del jitter absoluto. El intervalo
de conexión BLE condiciona estas métricas porque agrupa las recepciones
alrededor de sus eventos, pero no constituye una espera mínima obligatoria.

#### Criterio de sostenibilidad

Un perfil se considera sostenible en esta ejecución cuando:

- la pérdida extremo a extremo es 0 %;
- no hay errores de envío;
- todos los paquetes transmitidos son reconstruidos por el central.

La ocupación máxima y las latencias se utilizan para estimar el margen restante.
Por ejemplo, un perfil con pérdida nula pero `7/8` posiciones ocupadas tiene
menos margen ante una demora transitoria que otro que alcance solamente `3/8`.

#### Procesamiento

El flujo reproducible es:

```text
log serie del central
    → validación de P, S, C, D, R y E
    → corrección de timestamps del XIAO
    → agrupación por config_id
    → cálculo de pérdida, throughput, cola, latencias y jitter
    → CSV de métricas e informe
```

Se ejecuta con:

```bat
python tools\corregir_tiempos.py ^
  resultados\experimentos_unificados.txt ^
  -o resultados\experimentos_unificados_corregido.csv

python tools\analizar_experimentos.py ^
  resultados\experimentos_unificados_corregido.csv ^
  resultados\experimentos_unificados.txt ^
  -o resultados\experimentos_metricas.csv
```

### Consumo energético

La sostenibilidad temporal no basta para escoger una ventana: dos perfiles
pueden entregar todos sus datos y, sin embargo, activar el procesador y la radio
con frecuencias distintas. Por eso el consumo se mide solamente después del
preexperimento, sobre el firmware periódico unificado que representa la
arquitectura final.

La sesión se divide en advertising, conexión sin streaming y once perfiles
activos. Advertising y establecimiento de conexión son costos comunes para
alcanzar el estado conectado, no consecuencias de una ventana específica; se
miden una vez para describir el ciclo completo, pero la comparación entre
ventanas se hace durante cada pulso de perfil activo.

| Marcador | Pin XIAO |
| --- | --- |
| adquisición | D0 / P0.02 |
| transmisor de aplicación | D1 / P0.03 |
| sincronización | D2 / P0.28 |
| advertising | D3 / P0.29 |
| conexión | D4 / P0.04 |
| perfil activo | D5 / P0.05 |

I²C se deshabilita en los builds del XIAO para liberar D4 y D5. Los marcadores
no se compilan ni se usan en el preexperimento. Advertising y conexión se miden
una vez por sesión y no se atribuyen a un perfil particular.

Para cada perfil se obtendrán:

```text
corriente media
corriente máxima
energía total
energía por paquete completo recibido
energía por byte útil recibido
```

La energía se calcula integrando la potencia durante el pulso
`PROFILE_ACTIVE` correspondiente:

```text
E = ∫ V × I(t) dt
```

La corriente media describe el costo sostenido; el máximo ayuda a dimensionar
la alimentación; la energía total permite comparar los 20 s de cada perfil. La
energía por paquete no basta por sí sola porque las ventanas contienen
cantidades distintas de muestras, de modo que la comparación principal se
normaliza también por byte útil recibido. En perfiles con pérdida, el
denominador incluye únicamente información que llegó completa: la energía
gastada en ventanas descartadas queda penalizada.

`ACQUISITION_ACTIVE` y `TRANSMITTER_ACTIVE` permiten separar visualmente el
trabajo de generación y transmisión. Este último representa actividad del hilo
de aplicación, no el instante físico exacto durante el cual la radio está
emitiendo. Los GPIO sirven como referencias temporales para alinear una captura
externa de corriente; su nivel lógico no es una medición de potencia.

El diseño completo está en [PLAN_EXPERIMENTAL.md](PLAN_EXPERIMENTAL.md) y la
instalación reproducible en
[GUIA_INSTALACION_WINDOWS.md](GUIA_INSTALACION_WINDOWS.md).

### Reproducibilidad

La [guía de instalación y compilación](GUIA_INSTALACION_WINDOWS.md) forma parte
de la metodología: enumera las herramientas para Windows, instala los SDK de
Zephyr correspondientes al nRF52840 y al ESP32-S3, verifica el entorno, compila
ambos extremos y comprueba en los artefactos los parámetros BLE relevantes. Los
scripts de compilación generan por separado el firmware del preexperimento y el
del experimento periódico, de manera que repetir una etapa no requiere editar
manualmente el código.

La ejecución se documenta en
[EJECUTAR_PREEXPERIMENTO.md](EJECUTAR_PREEXPERIMENTO.md) y
[EJECUTAR_EXPERIMENTOS.md](EJECUTAR_EXPERIMENTOS.md). La segunda guía incluye
grabación, conexión del pulso de sincronización, captura del puerto serie y los
comandos de corrección y análisis. Los binarios ya construidos se conservan en
`firmware/`, mientras que los fuentes, configuraciones y scripts permiten
regenerarlos.

### Perfil del entorno radioeléctrico

El desempeño de un enlace inalámbrico también puede depender del entorno donde
se ejecuta. Para documentarlo se incorpora, como etapa asociada al experimento,
un firmware independiente del ESP32-S3 que realiza un inventario de los
emisores visibles desde la posición real del central. No se utiliza para
modificar automáticamente los parámetros BLE, sino para conservar una
descripción reproducible del contexto radioeléctrico junto con los resultados.

El procedimiento es:

```text
fijar posición, orientación, alimentación y entorno
    → ejecutar sondeo y experimento en sesiones contiguas
    → cambiar únicamente el firmware del central entre ambas
    → guardar juntos el perfil radioeléctrico y el log experimental
```

El sondeo comienza después de una guarda de 5 s y ejecuta las radios
secuencialmente:

1. escaneo BLE activo durante 60 s, sin filtro de duplicados ni conexiones;
2. escaneos Wi-Fi activos repetidos durante al menos 60 s, sin asociarse a
   ningún punto de acceso.

La separación evita que el uso simultáneo de BLE y Wi-Fi por el propio
ESP32-S3 introduzca una condición distinta durante la caracterización. Para BLE
se almacenan hasta 128 direcciones con tipo, nombre anunciado, cantidad de
observaciones, primera y última detección, y RSSI mínimo, medio y máximo. Para
Wi-Fi se almacenan hasta 64 BSSID con SSID, banda, canal, seguridad, cantidad de
observaciones y la misma distribución de RSSI. El ESP32-S3 detecta Wi-Fi
solamente en 2,4 GHz.

Para que el perfil sea comparable deben conservarse:

- posición y orientación del central;
- distancia y ubicación de XIAO, computadora, PPK2 y demás equipos;
- alimentación utilizada;
- fecha, hora y ubicación del ensayo;
- duración y configuración impresas en los registros `SETTING`;
- contadores `SUMMARY_BLE` y `SUMMARY_WIFI`, incluidos posibles desbordamientos.

El resultado permite describir cuántos emisores eran visibles, en qué canales
Wi-Fi operaban y qué potencia recibía el central. Es información contextual:
RSSI no mide ocupación temporal, tráfico ni interferencia, y una dirección BLE
privada puede cambiar. En consecuencia, el sondeo permite detectar diferencias
grandes entre ambientes, pero no demuestra que una red determinada haya causado
una pérdida o latencia. El procedimiento completo y el formato de salida están
en [EJECUTAR_SONDEO_REDES.md](EJECUTAR_SONDEO_REDES.md).

---

## 4. Resultados del sondeo radioeléctrico

La captura [sondeo_redes.txt](resultados/sondeo_redes.txt) finalizó
correctamente. El inventario no se truncó: se observaron cinco direcciones BLE
frente a una capacidad de 128 y un BSSID Wi-Fi frente a una capacidad de 64,
sin reportes fuera de almacenamiento.

### Entorno BLE

Durante 60 s se recibieron 1.703 reportes correspondientes a cinco direcciones:

> Por privacidad, las direcciones BLE, el BSSID, el SSID y los nombres de
> dispositivos ajenos al proyecto fueron sustituidos por seudónimos
> consistentes. Las métricas radioeléctricas y temporales no fueron alteradas.

| Dirección | Tipo | Nombre | Observaciones | RSSI mín./medio/máx. |
| --- | --- | --- | ---: | ---: |
| `02:00:00:00:00:01` | aleatoria | `Xiao_GATT_Min` | 525 | −81 / −39,78 / −37 dBm |
| `02:00:00:00:00:02` | aleatoria | sin nombre | 263 | −82 / −50,56 / −44 dBm |
| `02:00:00:00:00:03` | aleatoria | sin nombre | 543 | −82 / −54,06 / −46 dBm |
| `02:00:00:00:00:04` | pública | `dispositivo_vecino` | 361 | −81 / −58,03 / −52 dBm |
| `02:00:00:00:00:05` | aleatoria | sin nombre | 11 | −86 / −71,90 / −67 dBm |

El XIAO fue el anunciante BLE de mayor potencia media, 10,78 dB por encima del
segundo dispositivo, y permaneció visible desde 69 ms después de comenzar hasta
prácticamente el final del sondeo. La diferencia entre su mínimo de −81 dBm y su
media de −39,78 dBm confirma que un valor instantáneo aislado no describe por sí
solo las condiciones del enlace.

### Entorno Wi-Fi

Se completaron 18 barridos activos en 2,4 GHz. Todos los resultados
correspondieron a un único BSSID:

| BSSID | SSID | Canal | Seguridad reportada | Observaciones | RSSI mín./medio/máx. |
| --- | --- | ---: | --- | ---: | ---: |
| `02:00:00:00:01:01` | `red_entorno_2.4` | 4 | `UNKNOWN` | 16 | −41 / −33,31 / −28 dBm |

El punto de acceso presentó una señal fuerte en la ubicación del central.
`UNKNOWN` indica que el controlador no clasificó la seguridad; no significa que
la red sea abierta.

### Alcance de estos resultados

El sondeo describe un entorno visible acotado, con cinco direcciones BLE y un
punto de acceso Wi-Fi cercano. No mide ocupación temporal, tráfico,
retransmisiones ni colisiones; por eso no permite atribuir las pérdidas del
experimento periódico a uno de esos emisores.

Aunque el firmware de sondeo se ejecutó en una sesión separada, se mantuvieron
el mismo entorno y montaje del experimento periódico. Por ello esta captura se
adopta como el perfil radioeléctrico representativo de esa ejecución. La
separación sigue siendo necesaria porque el central no puede ejecutar
simultáneamente el firmware experimental y el de sondeo. El análisis completo
está en
[resultados/SONDEO_REDES_ANALISIS.md](resultados/SONDEO_REDES_ANALISIS.md).

---

## 5. Resultados del preexperimento

La caracterización válida utilizó PHY 2M, Data Length 251, MTU 247 e intervalo
de conexión de 50 ms. Recibió 30.869 paquetes completos sin descartes, errores,
paquetes incompletos ni desbordamiento:

| Payload | Fragmentos | Capacidad | Throughput útil | Latencia transporte P95 |
| ---: | ---: | ---: | ---: | ---: |
| 15 B | 1 | 60,10 pkt/s | 7,21 kbps | 49,50 ms |
| 30 B | 1 | 80,29 pkt/s | 19,27 kbps | 48,79 ms |
| 60 B | 1 | 1191,79 pkt/s | 572,06 kbps | 5,33 ms |
| 120 B | 1 | 907,28 pkt/s | 870,99 kbps | 6,23 ms |
| 240 B | 2 | 452,69 pkt/s | 869,16 kbps | 6,83 ms |
| 480 B | 3 | 264,44 pkt/s | 1015,46 kbps | 8,79 ms |
| 960 B | 5 | 142,43 pkt/s | 1093,83 kbps | 12,20 ms |

Los payloads de 15 y 30 bytes no mantuvieron el mismo régimen de saturación que
los tamaños mayores. Desde 60 bytes la implementación aprovecha mejor los
eventos de conexión: al duplicar de 30 a 60 B, la tasa observada pasó de
80,29 a 1191,79 paquetes/s y la latencia P95 descendió de 48,79 a 5,33 ms. Este
salto no se atribuye al tiempo de radio de 30 B adicionales, sino al
comportamiento de planificación y alimentación de buffers de esta pila bajo
saturación. El ensayo permite identificar el cambio empíricamente, pero no
aislar una única causa interna.

Entre 120 y 960 B, aumentar el paquete reduce la cantidad de paquetes por
segundo, pero el throughput crece porque se amortizan los encabezados y las
llamadas a la pila. La fragmentación tampoco invalida un tamaño: 960 B necesita
cinco fragmentos y aun así obtuvo el mayor throughput útil, 1,09 Mbps, con un
tiempo de transporte P95 de 12,20 ms. Para elegir una ventana interesa el tiempo
del paquete completo, medido al recibir el último fragmento, y no el tiempo
aislado de cada fragmento.

Estos resultados justificaron dos decisiones de la etapa periódica: excluir
S1-E5 y S1-E10, que usarían los payloads ineficientes de 15 y 30 B, y agregar
una barrida de estrés con 960 B para exigir al enlace con el tamaño que alcanzó
el mayor caudal.

---

## 6. Resultados del experimento periódico unificado

### Validez de la ejecución

La sesión analizada utilizó intervalo de conexión de 50 ms, latencia de
periférico 0, PHY 2M y Data Length de 251 B. Se recibieron los once registros
de estadísticas, las veinte marcas de sincronización y la calibración final,
sin errores de protocolo, fragmentos inválidos ni desbordamiento del central.

El log de trabajo contiene 21.377 paquetes válidos. Sus contadores cierran
exactamente:

```text
25.760 generados - 4.340 descartes por cola llena
                  -    43 descartes al finalizar
= 21.377 transmitidos y recibidos
```

### Offset y deriva de los relojes

Los promedios de los diez pulsos iniciales y finales produjeron:

| Parámetro | Valor medido |
| --- | ---: |
| Marca inicial XIAO | 92.135.778 µs |
| Marca inicial central | 8.829.900 µs |
| Offset inicial `central - XIAO` | −83.305.878 µs |
| Marca final XIAO | 324.787.459 µs |
| Marca final central | 241.480.000 µs |
| Offset final `central - XIAO` | −83.307.459 µs |
| Intervalo medido por el XIAO | 232.651.681 µs |
| Intervalo medido por el central | 232.650.100 µs |
| Escala `central / XIAO` | 0,999993204433 |
| Deriva de la escala | −6,7956 ppm |

El offset se volvió 1.581 µs más negativo durante 232,65 s. Expresado de forma
intuitiva, el reloj del XIAO avanzó aproximadamente 6,7956 ppm más rápido que el
del central, unos 6,8 µs por segundo. Aunque la diferencia es pequeña, acumularía
alrededor de 24,5 ms por hora si permaneciera constante. Esto justifica corregir
no solo el offset inicial sino también la pendiente temporal.

### Métricas por perfil

La latencia extremo a extremo se mide desde el comienzo de adquisición de la
ventana hasta la recepción de su último fragmento. Se divide en latencia
pre-envío y latencia de transporte desde que el transmisor toma el paquete. Las
columnas de latencia muestran `media / P95`.

| Perfil | Paquete | Ventana | Pérdida | Throughput útil | Cola máx. | Pre-envío media/P95 | Latencia E2E media/P95 | Transporte media/P95 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| S1-E20 | 60 B | 20 ms | 0,00 % | 24,02 kbps | 3/8 | 31,00 / 39,72 ms | 59,10 / 89,57 ms | 28,09 / 50,12 ms |
| S2-E5 | 60 B | 5 ms | 7,38 % | 89,11 kbps | 8/8 | 32,43 / 66,15 ms | 47,50 / 86,79 ms | 15,07 / 45,54 ms |
| S2-E10 | 120 B | 10 ms | 0,00 % | 96,19 kbps | 7/8 | 25,94 / 47,11 ms | 49,00 / 79,47 ms | 23,06 / 50,31 ms |
| S2-E20 | 240 B | 20 ms | 0,00 % | 96,19 kbps | 5/8 | 33,43 / 44,89 ms | 60,30 / 91,50 ms | 26,87 / 51,58 ms |
| S3-E5 | 240 B | 5 ms | 13,00 % | 335,23 kbps | 8/8 | 35,62 / 80,99 ms | 46,84 / 96,44 ms | 11,23 / 42,46 ms |
| S3-E10 | 480 B | 10 ms | 2,79 % | 374,40 kbps | 8/8 | 40,97 / 90,80 ms | 55,98 / 114,45 ms | 15,01 / 49,32 ms |
| S3-E20 | 960 B | 20 ms | 0,00 % | 384,77 kbps | 7/8 | 48,99 / 104,04 ms | 72,36 / 132,09 ms | 23,37 / 55,44 ms |
| STRESS-960-E6 | 960 B | 6 ms | 50,64 % | 633,98 kbps | 8/8 | 120,21 / 207,40 ms | 138,48 / 238,99 ms | 18,27 / 55,53 ms |
| STRESS-960-E7 | 960 B | 7 ms | 33,52 % | 731,14 kbps | 8/8 | 112,92 / 203,16 ms | 128,48 / 225,39 ms | 15,56 / 48,78 ms |
| STRESS-960-E8 | 960 B | 8 ms | 21,52 % | 754,94 kbps | 8/8 | 109,81 / 172,47 ms | 124,99 / 195,27 ms | 15,18 / 47,98 ms |
| STRESS-960-E10 | 960 B | 10 ms | 15,74 % | 649,73 kbps | 8/8 | 81,89 / 161,07 ms | 99,18 / 181,02 ms | 17,29 / 51,66 ms |

### Interpretación

Los perfiles sostenibles en esta ejecución fueron S1-E20, S2-E10, S2-E20 y
S3-E20. En todos ellos la cantidad recibida coincide con la generada y el
throughput medido coincide, dentro de la duración efectiva del perfil, con los
24, 96 y 384 kbps derivados del modelo de señal.

S2-E5, S3-E5 y S3-E10 llenaron la cola y perdieron ventanas. Una ventana menor
no resulta automáticamente mejor: genera más paquetes dentro de cada intervalo
de conexión de 50 ms y exige que una cola de ocho posiciones absorba las
ráfagas y variaciones del servicio BLE. Por tanto, el caudal de muestras no es
la única variable relevante. S2-E5, S2-E10 y S2-E20 ofrecen los mismos 96 kbps,
pero solo E5 produce 200 paquetes/s y pierde 7,38 %; E10 y E20 producen 100 y
50 paquetes/s y no pierden datos.

Para S2, E10 y E20 transportaron el caudal completo de aproximadamente
96,2 kbps. E10 obtuvo la menor latencia extremo a extremo P95, 79,47 ms, pero
alcanzó 7/8 posiciones de cola. E20 utilizó 5/8 y requiere la mitad de paquetes,
aunque cada uno se divide en dos fragmentos. La comparación energética permitirá
decidir si conviene priorizar latencia o margen de cola y menor frecuencia de
paquetes.

Para S3, E20 fue la única ventana sin pérdidas. E10 perdió 2,79 % y E5 perdió
13,00 %. S3-E20 sostiene aproximadamente 384,8 kbps, pero su latencia pre-envío
P95 de 104,04 ms indica que algunas ventanas permanecen mucho más tiempo en la
ruta previa al enlace que los 20 ms necesarios para formarlas. Aunque E20 usa
el paquete más fragmentado —cinco notificaciones—, sus 50 paquetes/s presionan
menos la cola que los 100 o 200 paquetes/s de E10 y E5. En esta arquitectura,
reducir la frecuencia de paquetes compensa el costo de transmitir más
fragmentos por paquete.

La barrida de estrés muestra que 960 B cada 10 ms ya no es sostenible con esta
arquitectura. El límite periódico se encuentra entre 50 pkt/s —S3-E20— y
100 pkt/s —STRESS-960-E10—. El mayor throughput recibido fue 754,94 kbps en
STRESS-960-E8, pero con 21,52 % de pérdida, por lo que no constituye una
configuración utilizable. Ese valor es throughput recibido, no capacidad
sostenible: la carga ofrecida era 960 kbps y casi una de cada cinco ventanas no
llegó. De forma análoga, los cuatro perfiles de estrés llenaron la cola; bajar
el período aumenta la carga ofrecida hasta 1,28 Mbps, pero no evita que el
throughput útil se estanque y la pérdida crezca.

El techo de 1,09 Mbps medido por saturación en el preexperimento no equivale a
una tasa periódica sostenible. En el experimento periódico, la producción es
independiente de BLE, el servicio se concentra alrededor de los eventos de
conexión y la cola debe absorber sus variaciones. El preexperimento responde
cuánto puede transferir el camino de envío cuando siempre tiene trabajo
disponible; el experimento periódico responde si un productor de ritmo fijo
puede convivir con ese servicio variable y con una memoria intermedia finita.

La recepción también queda agrupada por esos eventos. El jitter P95 se ubicó
entre aproximadamente 28 y 46 ms. Para no confundir pérdidas con jitter, se
calcula considerando los saltos de identificador:

```text
jitter = |delta_rx - delta_packet_id × período_de_generación|
```

La medición del PPK2 continúa pendiente antes de recomendar una ventana
definitiva.

---

## 7. Estado y conclusiones

La implementación reproduce el aspecto esencial de un sensor continuo: las
ventanas se generan con reloj propio aunque BLE esté ocupado. La cola,
fragmentación, reconstrucción, sincronización y los contadores permiten
observar dónde se acumula la demanda y contabilizar como pérdida toda ventana
que no llega completa.

La experiencia muestra que una ventana no debe elegirse igualando simplemente
su duración a la latencia media de transporte. Esa latencia varía y BLE puede
atender varias operaciones alrededor de un evento de conexión; la cola absorbe
variaciones breves. La ventana debe elegirse comprobando simultáneamente pérdida
nula, margen de cola y distribución de latencia bajo el caudal de la señal.

Con los datos actuales:

- S1 queda representado por E20, con 60 B cada 20 ms y pérdida nula.
- Para S2, E10 ofrece menor P95 extremo a extremo, mientras que E20 utiliza
  menos posiciones de cola y la mitad de paquetes; ambos son candidatos hasta
  medir energía.
- Para S3, E20 es la única alternativa evaluada que conserva el caudal completo.
- Los perfiles de estrés confirman que el mayor throughput recibido no es una
  configuración útil cuando se obtiene descartando ventanas.
- El primer sondeo radioeléctrico encontró al XIAO como anunciante BLE más
  fuerte y un único BSSID Wi-Fi visible en canal 4. Se adopta como perfil del
  entorno de la ejecución, aunque no permite explicar causalmente sus pérdidas.

Todavía no se declara una ventana óptima. Falta medir corriente y energía con
los marcadores implementados, y la conclusión se limita a esta arquitectura,
parámetros de conexión, profundidad de cola y estrategia de una notificación en
curso. Cambiar alguno de ellos requiere volver a caracterizar el sistema.

Los CSV y el detalle de procesamiento de la ejecución están en
[resultados/EXPERIMENTOS_UNIFICADOS_ANALISIS.md](resultados/EXPERIMENTOS_UNIFICADOS_ANALISIS.md);
el análisis del entorno está en
[resultados/SONDEO_REDES_ANALISIS.md](resultados/SONDEO_REDES_ANALISIS.md).

Para ejecutar:

- [sondeo BLE y Wi-Fi del entorno](EJECUTAR_SONDEO_REDES.md)
- [preexperimento](EJECUTAR_PREEXPERIMENTO.md)
- [experimento unificado](EJECUTAR_EXPERIMENTOS.md)

---

## 8. Referencias

- R. Merletti y G. L. Cerone, [*Surface EMG detection, conditioning and
  pre-processing: Best practices*](https://doi.org/10.1016/j.jelekin.2020.102440),
  Journal of Electromyography and Kinesiology, 2020. Fundamenta la banda,
  frecuencia de muestreo y resolución de conversión.
- M. Atzori et al., [*Electromyography data for non-invasive
  naturally-controlled robotic hand
  prostheses*](https://doi.org/10.1038/sdata.2014.53), Scientific Data, 2014.
  Proporciona el contexto multicanal de NinaPro.
- Y. Du et al., [*Surface EMG-Based Inter-Session Gesture Recognition Enhanced
  by Deep Domain Adaptation*](https://doi.org/10.3390/s17030458), Sensors, 2017.
  Describe CapgMyo y su adquisición EMG de alta densidad.
- Texas Instruments, [*ADS1299: Low-Noise, 8-Channel, 24-Bit Analog-to-Digital
  Converter for Biopotential
  Measurements*](https://www.ti.com/product/ADS1299). Referencia del formato de
  muestra y de una familia de front-ends simultáneos de 4, 6 y 8 canales.
- Bluetooth SIG, [*Bluetooth Core Specification:
  ATT*](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host/attribute-protocol--att-.html)
  y [*L2CAP*](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/host/logical-link-control-and-adaptation-protocol-specification.html).
  Fundamenta la elección de MTU y los tamaños de encabezado.
- Zephyr Project, [*Update the connection
  MTU*](https://docs.zephyrproject.org/latest/samples/bluetooth/mtu_update/README.html).
  Referencia de la configuración e intercambio de ATT MTU en la pila utilizada.
