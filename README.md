# Transmisión continua de datos tipo EMG por Bluetooth Low Energy

Informe del proyecto TIoT. Presenta el problema de transportar muestras de
EMG de forma continua, la metodología experimental, el funcionamiento de los
programas de ambos dispositivos y los resultados de los ensayos realizados con
Bluetooth Low Energy (BLE).

## Contenido

1. [Introducción](#1-introducción)
2. [Objetivos](#2-objetivos)
3. [Metodología](#3-metodología)
4. [Resultados del sondeo radioeléctrico](#4-resultados-del-sondeo-radioeléctrico)
5. [Resultados del preexperimento de capacidad](#5-resultados-del-preexperimento-de-capacidad)
6. [Resultados del experimento con enventanado temporal](#6-resultados-del-experimento-con-enventanado-temporal)
7. [Conclusiones](#7-conclusiones)
8. [Referencias](#8-referencias)

---

## 1. Introducción

La electromiografía de superficie (sEMG) registra la actividad eléctrica asociada
a la contracción muscular mediante electrodos sobre la piel. El análisis de estas
señales permite estudiar la activación muscular y el movimiento, y puede aplicarse
en rehabilitación, interfaces persona-máquina y control de prótesis. Cuando se
requiere analizar la señal durante la actividad, los datos adquiridos deben llegar
de forma continua al equipo que los procesa, con retraso y pérdida controlados;
disponer únicamente de un registro al final de la sesión
no cumple el mismo propósito ([revisión de aplicaciones en rehabilitación](https://www.mdpi.com/2079-9292/12/7/1520);
[control de prótesis con EMG](https://pmc.ncbi.nlm.nih.gov/articles/PMC10382439/)).

Un caso concreto es el monitoreo remoto de la actividad muscular durante ejercicios
o tareas realizadas en exteriores, donde el sujeto necesita moverse sin quedar
unido por cables al equipo de registro. En ese escenario, un sensor portátil puede
enviar los datos de forma inalámbrica a un dispositivo cercano que los recibe
(el **receptor**) para observar la actividad mientras ocurre
([monitoreo inalámbrico de actividad física con sEMG](https://pmc.ncbi.nlm.nih.gov/articles/PMC6245594/);
[adquisición inalámbrica de sEMG durante ejercicio en exteriores](https://www.nature.com/articles/s41467-024-55417-1)).

También importa dónde se convierte la señal en datos digitales. La sEMG es una
señal de baja amplitud y los cables que llevan la señal analógica desde los
electrodos hasta un amplificador distante pueden captar interferencia
electromagnética y generar artefactos al moverse. Integrar los electrodos y la
electrónica de adquisición en un módulo colocado sobre el músculo acorta ese
recorrido analógico; después, los datos digitalizados pueden transmitirse sin
cables. Esto puede reducir una fuente de ruido, aunque la calidad de la medida
también depende del contacto entre el electrodo y la piel y del diseño del
acondicionamiento de señal ([sistema de adquisición EMG digital](https://www.pure.ed.ac.uk/ws/portalfiles/portal/471117370/AghchehliEtalIEEETNSRE2024DigitalSensingSystems.pdf)).

Si se colocan varios módulos en distintos músculos de una misma persona, estos
pueden enviar sus datos a un receptor común y formar una red inalámbrica de
sensores corporales. Esta posibilidad permite observar varias zonas a la vez,
pero suma tráfico y puede exigir sincronizar las mediciones entre sensores
([sistema de monitoreo con varios nodos sEMG](https://pmc.ncbi.nlm.nih.gov/articles/PMC6245594/)).

![Esquema de una red corporal con sensores EMG sobre distintos músculos que transmiten por BLE a un dispositivo cercano](body_sensor_network.png)

**Figura 1.** Ilustración generada con IA de una posible red de sensores
corporales. Muestra el monitoreo simultáneo de varios músculos; el montaje de
este trabajo utiliza un solo nodo transmisor y no incluye adquisición de EMG
real ni una aplicación de visualización.

El enlace inalámbrico introduce una dificultad: la adquisición produce muestras
a un ritmo definido, mientras BLE transmite los paquetes que las contienen en
eventos de conexión. Una tasa de datos nominalmente suficiente no garantiza que
cada paquete llegue a tiempo. Si el envío se retrasa, los paquetes esperan en
una **cola**, es decir, un espacio de memoria para datos pendientes.
Esa espera aumenta la latencia y, si la cola se llena, causa pérdida.

En este trabajo una **ventana** es el conjunto de muestras de todos los canales
de medición correspondiente a un intervalo temporal de duración `W`, expresada en
**milisegundos (ms)**. Al terminar cada ventana se forma un paquete de aplicación:
por ejemplo, con cuatro canales a 1 kHz y `W = 10 ms` se reúnen diez muestras
por canal, 40 muestras en total. Como cada muestra ocupa tres bytes, la ventana
contiene 120 bytes de muestras. Ventanas más cortas generan paquetes con mayor
frecuencia; ventanas más largas reúnen más datos y deben completarse antes de
enviarse. Aquí «ventana» designa ese intervalo de agrupación, no un evento de
conexión BLE.

El estudio comienza con un **preexperimento de capacidad**: se envían paquetes
de distintos tamaños tan rápido como lo permite la implementación BLE, sin
imponer una frecuencia de adquisición. Así se mide la tasa útil máxima observada
para cada tamaño bajo la configuración BLE utilizada. Esa capacidad sirve
de referencia para el **experimento principal con enventanado temporal**. En
este, el temporizador avisa cada `W` milisegundos al hilo que forma una nueva
ventana, aunque BLE todavía esté enviando las anteriores.

La pregunta central es **qué duraciones de ventana entregan todos los datos
generados por cada configuración representativa de adquisición EMG y cuál de
ellas conviene al considerar la latencia, el margen de cola y el consumo
energético**. La cantidad de canales, la frecuencia de muestreo y los bits por
muestra determinan la tasa de datos que debe transportar BLE; la duración de
ventana determina cuántas muestras se agrupan en cada paquete y con qué
frecuencia se envía.

El estudio utiliza un XIAO nRF52840 como **periférico BLE** (el dispositivo que
envía los datos) y un ESP32-S3 N16R8 como **central BLE** (el receptor que se
conecta a él), ambos programados con el sistema operativo Zephyr. El periférico
genera datos deterministas que reproducen los tamaños y períodos de varios
escenarios EMG; **no adquiere una señal EMG
real**. Por ello, los resultados caracterizan el transporte de datos tipo EMG en
esta arquitectura, no la calidad clínica de una señal ni el desempeño de un
algoritmo de análisis. Tampoco se ensaya aquí una red con varios sensores sobre
un mismo sujeto: ese escenario motiva el estudio, pero requiere una evaluación
específica.

![Diagrama de comunicación BLE entre el XIAO nRF52840 como periférico y el ESP32-S3 como central](<Comunicación BLE entre XIAO y ESP32-S3.png>)

**Figura 2.** Diagrama generado con IA de los roles y el sentido de la
comunicación BLE en el montaje experimental. El XIAO envía datos tipo EMG
generados por software al ESP32-S3; los bloques de adquisición EMG y
visualización representan una aplicación posible y no forman parte del ensayo
realizado.

```mermaid
flowchart LR
    A["Generación de datos<br/>tipo EMG"] --> B["Ventana temporal<br/>de muestras"]
    B --> C["Cola finita<br/>8 paquetes"]
    C --> D["Fragmentación<br/>hasta 212 B útiles"]
    D --> E["Notificaciones BLE"]
    E --> F["Verificación de paquete completo<br/>ESP32-S3"]
    F --> G["Corrección temporal<br/>y métricas"]
```

En el ensayo con enventanado, el temporizador, la cola y el enlace quedan
separados para observar cuándo la producción de ventanas supera la capacidad
de envío.

### Estructura del repositorio

| Ruta | Contenido |
| --- | --- |
| [`periferico/peripherial_min/`](../periferico/peripherial_min/) | Programa (firmware) del XIAO nRF52840 |
| [`central/central_min/`](../central/central_min/) | Firmware del ESP32-S3 central |
| [`common/`](../common/) | Protocolo binario compartido |
| [`tools/`](../tools/) | Captura serie, corrección temporal y análisis |
| [`firmware/`](../firmware/) | Binarios compilados y scripts de grabación |
| [`resultados/`](../resultados/) | Capturas, tablas de datos (CSV) e informes derivados |

---

## 2. Objetivos

### Objetivo general

Determinar, para cada configuración representativa de hardware de adquisición
de EMG definida por número de canales, frecuencia de muestreo y bits por
muestra, qué duración de ventana entre las evaluadas permite transmitir sus
datos por esta implementación BLE sin pérdida y ofrece el mejor compromiso
entre latencia, margen de cola y consumo energético.

### Objetivos específicos

1. Definir configuraciones representativas de adquisición EMG y las duraciones
   de ventana que se evaluarán.
2. Implementar y caracterizar un enlace BLE para transmitir datos tipo EMG.
3. Comparar las ventanas por pérdida de datos, latencia, ocupación de cola y
   consumo energético.
4. Identificar la duración más conveniente para cada configuración y justificar
   los compromisos cuando no haya una opción única.

---

## 3. Metodología

Las configuraciones de adquisición se representan mediante datos deterministas
con el tamaño de paquete y la frecuencia de generación previstos; el montaje no
incluye electrónica de adquisición EMG real.

### De la señal EMG a la tasa de datos

La sEMG posee contenido espectral hasta aproximadamente 450–500 Hz. Por el
criterio de Nyquist, 1 kHz queda en la frontera teórica para representar esa
banda y deja muy poco margen para el filtro previo al muestreo; las guías consultadas
indican que en la práctica se utilizan frecuencias superiores a 1,5 kHz y se
prefieren 2 kHz o 2048 Hz. En consecuencia, el proyecto usa 1 kHz como caso
base de menor tasa y 2 kHz para representar una adquisición de mayor resolución
temporal y demanda de transmisión
([Merletti y Cerone, 2020](https://doi.org/10.1016/j.jelekin.2020.102440)).

La cantidad de canales depende de la aplicación. Un canal diferencial permite
seguir un músculo o una localización; varios canales permiten observar
simultáneamente distintos músculos o posiciones. Como referencia de escala, la
base NinaPro DB2 emplea 12 electrodos sEMG a 2 kHz para movimientos de mano y
CapgMyo utiliza una matriz de alta densidad de 128 canales a 1 kHz. El proyecto
selecciona 1, 4 y 8 canales para recorrer desde un dispositivo de un solo canal
hasta uno multicanal compacto.
Cuatro canales constituyen el punto intermedio de ingeniería
([Atzori et al., 2014](https://doi.org/10.1038/sdata.2014.53);
[Du et al., 2017](https://doi.org/10.3390/s17030458)).

Cada muestra se representa con una palabra de 24 bits, es decir, tres bytes.
Esta elección modela la salida sin comprimir de circuitos multicanal de
adquisición de biopotenciales como la familia ADS1299, disponible con 4, 6 u 8
convertidores analógico-digitales (ADC) simultáneos de 24 bits y tasas de
muestreo desde 250 hasta 16.000 muestras por segundo. La resolución efectiva depende
del ruido, la ganancia y el rango de entrada; la bibliografía recomienda al
menos 12 bits y prefiere 16 bits para una cadena correctamente amplificada
([ADS1299](https://www.ti.com/product/ADS1299);
[Merletti y Cerone, 2020](https://doi.org/10.1016/j.jelekin.2020.102440)).

Con estos supuestos, la tasa de datos útiles antes de agregar encabezados se
calcula en kilobits por segundo (`kbps`):

```text
tasa_de_datos [kbps] = canales × frecuencia_muestreo [muestras/s]
                       × 24 bits/muestra / 1000
```

| Escenario | Interpretación | Canales | Muestreo | Palabra | Tasa de datos útiles |
| --- | --- | ---: | ---: | ---: | ---: |
| S1 | adquisición mínima de un punto muscular | 1 | 1 kHz | 24 bits | 24 kbps |
| S2 | nodo multicanal compacto | 4 | 1 kHz | 24 bits | 96 kbps |
| S3 | nodo multicanal de mayor demanda | 8 | 2 kHz | 24 bits | 384 kbps |

Estos valores representan solo los bits de las muestras. El tráfico real sobre
la radio es mayor porque cada paquete añade información de control del
experimento y de las distintas capas del protocolo BLE.

### De la ventana temporal al tamaño de paquete

La ventana definida en la introducción fija el tamaño de cada paquete y la
frecuencia con que se genera.
Sea `W` su duración **en ms** y sea `frecuencia_muestreo` la cantidad de muestras
por segundo y por canal. El factor `1000` convierte milisegundos a segundos:

```text
muestras por canal = frecuencia_muestreo [muestras/s] × W [ms] / 1000
datos por ventana [B] = canales × frecuencia_muestreo [muestras/s]
                       × W [ms] / 1000 × 3 B/muestra
paquetes por segundo = 1000 ms/s / W [ms]
```

Por ejemplo, S2 con `W = 10 ms` produce `4 × 1000 × 10 / 1000 × 3 = 120 B`
por ventana y `1000 / 10 = 100` paquetes por segundo. Los bytes del encabezado
experimental y de BLE se agregan después y no forman parte de esos 120 B.
En este informe, los tamaños de 15 a 960 B designan **solo datos de muestras**:
es lo que el firmware guarda en el campo `payload`. Al transmitir, cada
fragmento agrega un encabezado experimental de 32 B. Ese encabezado sí forma
parte del valor transportado en la notificación BLE, pero no de los datos de
muestras ni del throughput útil. Por ejemplo, una ventana de 120 B que cabe en
un fragmento ocupa 152 B en el valor de la notificación (120 B de muestras más
32 B de encabezado experimental), además de los encabezados de BLE.

Se eligieron 5, 10 y 20 ms para comparar tres compromisos de transporte. Una
ventana corta hace disponibles las muestras antes, pero multiplica la cantidad
de paquetes y el costo de sus encabezados; una ventana larga amortiza ese costo,
pero debe completarse antes de poder enviarse y agrega latencia de formación.
Diez milisegundos funciona como punto intermedio. Los tamaños derivados son
(en la tabla, `pkt/s` significa paquetes por segundo):

| Escenario | 5 ms | 10 ms | 20 ms |
| --- | ---: | ---: | ---: |
| S1: 1 canal, 1 kHz | 15 B, 200 pkt/s | 30 B, 100 pkt/s | 60 B, 50 pkt/s |
| S2: 4 canales, 1 kHz | 60 B, 200 pkt/s | 120 B, 100 pkt/s | 240 B, 50 pkt/s |
| S3: 8 canales, 2 kHz | 240 B, 200 pkt/s | 480 B, 100 pkt/s | 960 B, 50 pkt/s |

En el ensayo con enventanado, **kbps** expresa el volumen de datos de muestras
generado por segundo (la **carga útil ofrecida**), mientras que **pkt/s** expresa
la frecuencia nominal de ventanas formadas (`1000 / W`); no indica cuántos
paquetes llegaron al receptor. Por ejemplo, S2 ofrece 96 kbps con las tres
ventanas, pero genera 200, 100 o 50 paquetes/s para 5, 10 o 20 ms,
respectivamente. Esa frecuencia influye en la ocupación de la cola y el trabajo
de envío. El throughput útil recibido puede ser menor que la carga ofrecida si
se pierden ventanas. Además, un paquete puede dividirse en varias notificaciones
BLE, por lo que paquetes/s tampoco equivale a notificaciones/s.

### Etapa 1: preexperimento de capacidad

Antes del experimento principal se ejecuta un preexperimento de saturación. Este
envía paquetes tan rápido como puede la implementación y mide la tasa útil
máxima observada para siete tamaños de carga útil: 15, 30, 60, 120, 240, 480 y
960 bytes de muestras por paquete. La medición permite identificar
qué tamaños aprovechan mejor el enlace bajo la configuración BLE utilizada y
establece una referencia para interpretar las tasas del experimento principal.

El preexperimento opera sin frecuencia fija de adquisición, sin cola de paquetes
generados por temporizador ni instrumentación de consumo. El **throughput** es la
tasa efectiva de datos recibidos, no la tasa nominal de la radio BLE.

### Lógica del programa con enventanado temporal

Antes de elegir las configuraciones de prueba (o **perfiles**) de la segunda
etapa, conviene distinguir quién produce una ventana y quién la envía. El
periférico usa tres hilos de aplicación y un temporizador de Zephyr. El
temporizador vence cada `W ms`, independientemente de cuándo se transmitan los
paquetes anteriores.

En esta sección, **`nombre()`** señala una función implementada en este proyecto;
`nombre()` sin negrita señala una función proporcionada por Zephyr. El mismo
formato en negrita identifica los recursos definidos por el proyecto. Las funciones
`k_*` pertenecen al núcleo de Zephyr (temporizadores, semáforos y colas), no a
su biblioteca BLE. La llamada BLE de Zephyr usada para enviar una notificación
es `bt_gatt_notify_cb()`.

| Elemento | Función en el periférico |
| --- | --- |
| **`sequence_thread()`** | Recorre los once perfiles. Activa el temporizador con `k_timer_start()` durante unos 20 s por perfil y deja una pausa de 1 s entre perfiles. |
| **`acquisition_timer_handler()`** | Se ejecuta al vencer el temporizador y avisa al hilo productor mediante `k_sem_give()`; no envía datos por BLE. |
| **`acquisition_thread()`** | Espera el aviso con `k_sem_take()`, asigna identificador y tiempo nominal, genera los datos con **`generate_payload()`** e intenta encolar el paquete mediante `k_msgq_put(..., K_NO_WAIT)`. |
| **`packet_queue`** | Guarda hasta ocho paquetes pendientes. Si está llena, se descarta el paquete nuevo; los que ya estaban en cola permanecen. |
| **`transmitter_thread()`** | Espera un paquete mediante `k_msgq_get()` y llama a **`send_packet()`** para dividirlo y enviarlo. |
| **`send_packet()`** | Agrega el encabezado experimental a cada fragmento y llama a **`custom_gatt_send()`**. |
| **`custom_gatt_send()`** | Encapsula `bt_gatt_notify_cb()`, la llamada de la biblioteca BLE de Zephyr, y espera el aviso local de finalización antes de enviar el siguiente fragmento. |

![Esquema de los tres hilos del XIAO: secuencia, productor y transmisor, con una cola compartida y recepción en el ESP32-S3](flujo_programa.svg)

**Figura 3.** El hilo de secuencia controla el perfil. Durante sus 20 s, el hilo
productor genera ventanas cada `W ms` y el transmisor toma las disponibles en la
cola, de forma independiente. El temporizador de Zephyr avisa al productor, pero
no constituye otro hilo.

El conteo de «paquete transmitido» corresponde al resultado local del XIAO;
la entrega completa se verifica por separado en el ESP32-S3. Si faltan
fragmentos, el central no registra esa ventana como completa.

El productor no espera a que BLE termine de transmitir para generar la próxima
ventana. Esto permite medir el efecto de un enlace temporalmente más lento: los
paquetes esperan en cola y crece la latencia. Si se llenan sus ocho posiciones,
el siguiente paquete generado se descarta. Al cerrar un perfil se detiene el
temporizador, se cuentan y purgan los paquetes que aún estaban pendientes y se
espera hasta 2 s por el envío que ya está en curso.

El período de generación se expresa en milisegundos y se programa con el
temporizador `k_timer` de Zephyr. El identificador de paquete y el inicio nominal
de la ventana se calculan desde el comienzo del perfil y ese período. Además se registra el
instante en que el transmisor toma el paquete para enviar el primer fragmento.
El central registra la recepción al completar el último fragmento. Los relojes
de ambos dispositivos se alinean después mediante pulsos eléctricos entre pines
de propósito general (GPIO); así se pueden comparar los tres instantes sin
detener el envío para imprimir por el puerto serie (UART).

Esta separación se usa en el **experimento principal**. En el preexperimento de
saturación, un único hilo genera y envía paquetes tan rápido como puede, sin
temporizador ni cola de paquetes generados por temporizador. Las funciones y sus
implementaciones están en
[`postexperiment.c`](../periferico/peripherial_min/src/postexperiment.c),
[`preexperiment.c`](../periferico/peripherial_min/src/preexperiment.c) y
[`experiment_receiver.c`](../central/central_min/src/experiment_receiver.c).

### Etapa 2: experimento principal con enventanado temporal

Con la caracterización anterior se descartan S1-E5 y S1-E10. Ambas son
duraciones de ventana posibles para S1, pero sus paquetes con 15 y 30 B de
muestras mostraron una utilización anómala y muy baja en saturación; S1-E20
conserva la misma tasa de 24 kbps con un paquete más eficiente. Se mantienen las
tres ventanas de S2 y S3 para observar cómo el empaquetado modifica la
sostenibilidad de una misma tasa de datos.

El firmware unifica esos siete perfiles EMG y cuatro perfiles de estrés en una
sola sesión. Cada perfil dura 20 segundos y hay una pausa de un segundo entre
perfiles. La duración es un compromiso operativo que produce entre 1.000 y
4.000 ventanas nominales por perfil, según su período, sin volver
inmanejable la captura serie. En los nombres de los perfiles, `S1`, `S2` y `S3`
identifican el escenario de señal y `E` indica el período de generación en
milisegundos (por ejemplo, `S2-E10` usa una ventana de 10 ms):

| Orden | Perfil | Relación con EMG | Muestras por paquete | Período | Carga ofrecida |
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

Los primeros siete representan los perfiles de datos tipo EMG seleccionados. Los
cuatro últimos conservan el mayor bloque de muestras, para amortizar encabezados, y
aumentan su frecuencia para localizar el límite sostenible. Estos cuatro perfiles son
cargas artificiales destinadas a caracterizar el límite de la comunicación.

La cola descrita antes reserva ocho posiciones de hasta 960 B de muestras más
metadatos, aproximadamente 7,8 kB. Su profundidad se mantiene fija en todos los
perfiles para comparar las mismas condiciones de espera y descarte. Una cola
mayor podría absorber demoras transitorias, pero usaría más memoria y permitiría
mayor latencia acumulada; no aumentaría la tasa que el enlace puede atender de
forma sostenida.

### Transporte y tiempo

El sistema usa un servicio GATT propio, es decir, una interfaz de datos definida
sobre BLE, para transportar ventanas binarias de muestras, marcas temporales,
identificadores y datos de fragmentación. Como los dos extremos pertenecen al
mismo sistema, la interfaz incluye la información necesaria para identificar los
fragmentos y comprobar si cada ventana llegó completa. El ESP32-S3 valida sus
encabezados y cuenta los fragmentos y bytes recibidos; no vuelve a formar un
arreglo de muestras ni analiza la señal EMG. La especificación completa de los
identificadores (UUID), las características, propiedades, permisos y el formato
binario está en
[common/README.md](../common/README.md).

Los datos se envían mediante notificaciones GATT. A diferencia de una indicación,
una notificación no espera una confirmación del protocolo de atributos de BLE
(ATT), lo que permite una mayor tasa de transmisión. La función llamada al
terminar el envío registra el resultado local de la transmisión. La recepción
en el central se verifica mediante identificadores de paquete, recuentos de
fragmentos y contadores al final de cada perfil.

Se usa un tamaño máximo de unidad ATT (MTU) de 247 B porque la extensión de
longitud de datos de BLE (Data Length) permite 251 B en una unidad de datos
(PDU) de la capa de enlace (Link Layer), y el encabezado del protocolo de
adaptación L2CAP ocupa 4 B. Así, una PDU ATT de
247 B cabe sin segmentación L2CAP. Cada notificación ofrece 244 B (`MTU - 3`),
de los cuales 32 corresponden al encabezado experimental; quedan 212 B de
muestras por fragmento:

```text
Data Length de Link Layer                   251 B
- encabezado Basic L2CAP                     4 B
= ATT MTU sin segmentación inferior         247 B

ATT MTU                                    247 B
- código y atributo de notificación           3 B
- encabezado experimental                    32 B
= muestras por fragmento                    212 B
```

El máximo lógico configurable de ATT es 517 B. Con Data Length 251, un MTU
superior a 247 B introduce segmentación L2CAP. El proyecto usa un MTU de 247 y
realiza una fragmentación de aplicación explícita, visible y medible. Por ello,
los paquetes con 15, 30, 60 y 120 B de muestras usan un fragmento; 240 B usa
dos; 480 B usa tres y 960 B usa cinco.

El transmisor mantiene un paquete de aplicación en curso y espera el aviso local
de finalización de cada notificación antes de entregar el fragmento siguiente.
Esta decisión simplifica el seguimiento de los fragmentos, acota el estado y
permite asociar el tiempo de transporte a un paquete completo. Los resultados
caracterizan esta estrategia de una notificación en curso.

Para que la comparación responda principalmente al empaquetado, todos los
perfiles comparten la misma conexión. En la ejecución analizada se negociaron
el modo de radio de 2 Mb/s (PHY 2M), Data Length de 251 B, ATT MTU de 247 B,
latencia de periférico cero e intervalo de conexión de 50 ms. Este último
establece cada cuánto se abren oportunidades programadas de comunicación y
agrupa el servicio del enlace en ráfagas con varios paquetes por evento. En 50 ms
se generan 10 ventanas de 5 ms, 5 de 10 ms
o 2–3 de 20 ms, lo que ayuda a explicar por qué dos perfiles con la misma tasa
de datos útiles ejercen distinta presión sobre una cola de ocho posiciones.

El tiempo de recepción de un paquete se define cuando llega su último fragmento;
si no hay fragmentación, es la recepción de su única notificación. Como cada
microcontrolador posee un reloj local independiente, comparar directamente sus
marcas temporales introduciría una diferencia inicial entre relojes (offset) y
una desviación que crece con el tiempo (deriva). El central genera diez pulsos
GPIO antes de la secuencia y diez después. El XIAO marca esos mismos flancos y los
devuelve fuera de la fase de datos. La sincronización queda separada del tráfico
BLE de los perfiles:

```text
ESP32-S3 GPIO4 --- resistencia 1 kΩ --- XIAO D2 / P0.28
ESP32-S3 GND -------------------------- XIAO GND
```

Dos grupos permiten ajustar una transformación lineal: el primero fija el
origen relativo y la separación entre los grupos estima la diferencia de
frecuencia. Diez pulsos por grupo reducen la influencia de la variación de una
única interrupción. El método alinea las escalas temporales para el análisis,
y conserva sin cambios los contadores físicos de ambos relojes.

### Métricas y criterios de cálculo

Ninguna métrica aislada decide la ventana. El throughput recibido puede ser alto
aunque se descarten muestras; una pérdida nula puede ocultar una cola casi llena;
y una media de latencia puede ocultar una cola larga. Por eso se combinan
contadores del productor, recepción extremo a extremo, ocupación de cola y
distribuciones temporales.

Cada paquete completo conserva tres instantes:

| Timestamp | Dispositivo | Definición |
| --- | --- | --- |
| `acquisition_time_us` | XIAO | Inicio nominal de la ventana de muestras simuladas |
| `tx_time_us` | XIAO | Momento en que el transmisor comienza a entregar el primer fragmento a GATT |
| `rx_time_us` | ESP32-S3 | Recepción del último fragmento y comprobación de que llegó el paquete completo |

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
frecuencia entre los dos relojes sobre los timestamps procesados.

Además de corregir cada paquete, se informan los parámetros de calibración:

```text
offset_inicial = central_inicio - xiao_inicio
offset_final   = central_fin - xiao_fin

escala = (central_fin - central_inicio)
         / (xiao_fin - xiao_inicio)

deriva_escala [ppm] = (escala - 1) × 10⁶
```

El offset depende principalmente del instante en que arrancó cada
microcontrolador. Su cambio entre el inicio y el final evidencia la deriva
acumulada. `ppm` significa partes por millón. Una
`deriva_escala` negativa significa que, durante el ensayo, el contador del XIAO
avanzó ligeramente más rápido que el del central.

#### Producción, pérdida y throughput

El XIAO informa por perfil:

- `generated`: ventanas generadas por el hilo productor tras el aviso del temporizador;
- `enqueued`: ventanas incorporadas a la cola;
- `transmitted`: paquetes entregados completamente al software BLE local;
- `queue_dropped`: ventanas nuevas descartadas porque la cola estaba llena;
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

Se toma una duración nominal de 20 s por perfil. El resultado se compara con la
**carga útil ofrecida**, calculada como
`bytes_de_muestras × 8 / período_en_ms`: si la pérdida es nula ambos valores deben coincidir,
salvo el pequeño efecto de la duración real de la etapa. Si existe pérdida, un
throughput recibido alto puede coexistir con una carga ofrecida insostenible.

#### Cola y latencias

La ocupación máxima se expresa como posiciones utilizadas sobre las ocho
disponibles. Alcanzar `8/8` registra que la cola llegó a llenarse; esta métrica
conserva el máximo y omite la duración de ese estado.

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
en cola. El formato actual agrupa estos componentes en una sola métrica de
pre-envío. La latencia de transporte incluye la espera por una oportunidad BLE,
el envío de todos los fragmentos y la comprobación de recepción completa en el
central.

Para cada latencia se informa:

- **media:** promedio aritmético de todos los paquetes recibidos;
- **P95:** percentil por rango más próximo; al menos el 95 % de las
  observaciones es menor o igual a ese valor;
- **máximo:** conservado en el CSV de métricas para diagnosticar valores
  extremos.

Las tablas del informe muestran `media / P95`.

#### Intervalo de recepción y jitter

Aquí **jitter** es la variación del intervalo entre recepciones respecto del
intervalo esperado de generación. Para paquetes consecutivos recibidos:

```text
delta_rx = rx_time_actual - rx_time_anterior
```

Cuando existen descartes, dos paquetes recibidos pueden estar separados por más
de un período de generación. El cálculo incorpora el salto de identificador:

```text
jitter =
    |delta_rx - delta_packet_id × período_de_generación|
```

Se calculan media, desviación estándar y P95 del jitter absoluto. El intervalo
de conexión BLE condiciona estas métricas porque agrupa las recepciones
alrededor de sus eventos.

#### Criterio de sostenibilidad

Un perfil se considera sostenible en esta ejecución cuando:

- la pérdida extremo a extremo es 0 %;
- no hay errores de envío;
- todos los paquetes transmitidos figuran como recibidos completos en el central.

La ocupación máxima y las latencias se utilizan para estimar el margen restante.
Por ejemplo, un perfil con pérdida nula pero `7/8` posiciones ocupadas tiene
menos margen ante una demora transitoria que otro que alcance solamente `3/8`.

La elección se hace en dos pasos: primero se consideran viables las ventanas
que entregan todas las muestras generadas en el ensayo; después se comparan su
latencia P95, la ocupación máxima de cola y, cuando esté medida, la energía por
byte útil. Si distintos criterios favorecen ventanas distintas, se informan
las alternativas según la prioridad de la aplicación en vez de declarar un
óptimo único.

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
  resultados\20261011\ble_central.txt ^
  -o resultados\20261011\ble_corregido.csv

python tools\analizar_experimentos.py ^
  resultados\20261011\ble_corregido.csv ^
  resultados\20261011\ble_central.txt ^
  -o resultados\20261011\ble_metricas.csv
```

### Consumo energético

La comparación de ventanas combina sostenibilidad temporal y consumo. Dos
perfiles pueden entregar todos sus datos y activar el procesador y la radio con
frecuencias distintas. El consumo se mide sobre el firmware del ensayo con
enventanado que representa la arquitectura final.

La sesión se divide en emisión de anuncios BLE (advertising), conexión sin
transmisión de muestras y once perfiles activos. Los anuncios y el establecimiento
de la conexión son costos comunes para
alcanzar el estado conectado y se miden una vez para describir el ciclo
completo. La comparación entre ventanas se realiza durante cada pulso de perfil
activo.

| Marcador | Pin XIAO |
| --- | --- |
| adquisición | D0 / P0.02 |
| transmisor de aplicación | D1 / P0.03 |
| sincronización | D2 / P0.28 |
| advertising | D3 / P0.29 |
| conexión | D4 / P0.04 |
| perfil activo | D5 / P0.05 |

I²C se deshabilita en los builds del XIAO para liberar D4 y D5. Los marcadores
se incluyen en el ensayo con enventanado. Advertising y conexión se miden una
vez por sesión como etapas comunes a todos los perfiles.

Las métricas energéticas definidas para cada perfil son:

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

`ACQUISITION_ACTIVE` y `TRANSMITTER_ACTIVE` separan visualmente el trabajo de
generación y la actividad del hilo transmisor. Los GPIO proporcionan referencias
temporales para alinear la captura externa de corriente, mientras que el medidor
PPK2 registra la potencia. En la primera captura, los pulsos de adquisición
resultaron demasiado breves para contarlos todos en el CSV exportado a 10 kHz;
el marcador del transmisor incluye la espera del callback BLE y no equivale a
tiempo de emisión de radio. Advertising, conexión y perfil sí quedaron
delimitados por sus marcadores.

El diseño completo está en [PLAN_EXPERIMENTAL.md](../PLAN_EXPERIMENTAL.md) y la
instalación reproducible en
[GUIA_INSTALACION_WINDOWS.md](../GUIA_INSTALACION_WINDOWS.md).
El montaje, la captura simultánea y el procesamiento del PPK2 están en la
[guía de consumo](../EJECUTAR_CONSUMO.md). La primera medición de corriente y su
registro BLE simultáneo se analizan en
[resultados/20261011/ANALISIS_CONSUMO.md](../resultados/20261011/ANALISIS_CONSUMO.md).

### Reproducibilidad

La [guía de instalación y compilación](../GUIA_INSTALACION_WINDOWS.md) forma parte
de la metodología: enumera las herramientas para Windows, instala los SDK de
Zephyr correspondientes al nRF52840 y al ESP32-S3, verifica el entorno, compila
ambos extremos y comprueba en los artefactos los parámetros BLE relevantes. Los
scripts de compilación generan por separado el firmware del ensayo sin
enventanado y el del ensayo con enventanado. Cada etapa se repite mediante su
script correspondiente.

La ejecución se documenta en
[EJECUTAR_PREEXPERIMENTO.md](../EJECUTAR_PREEXPERIMENTO.md) y
[EJECUTAR_EXPERIMENTOS.md](../EJECUTAR_EXPERIMENTOS.md). La segunda guía incluye
grabación, conexión del pulso de sincronización, captura del puerto serie y los
comandos de corrección y análisis. Los binarios ya construidos se conservan en
`firmware/`, mientras que los fuentes, configuraciones y scripts permiten
regenerarlos.

### Perfil del entorno radioeléctrico

El entorno radioeléctrico puede influir en el enlace. Para registrarlo se preparó
un firmware independiente del ESP32-S3 que inventaría los emisores visibles
desde la posición del central. Para asociar ese inventario a una ejecución de
rendimiento, el procedimiento previsto es:

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
observaciones, primera y última detección, y potencia de señal recibida (RSSI)
mínima, media y máxima. Para Wi-Fi se almacenan hasta 64 identificadores de
puntos de acceso (BSSID), con el nombre de red (SSID), banda, canal, seguridad,
cantidad de observaciones y la misma distribución de RSSI. El ESP32-S3 detecta Wi-Fi
solamente en 2,4 GHz.

Para que el perfil sea comparable deben conservarse:

- posición y orientación del central;
- distancia y ubicación de XIAO, computadora, PPK2 y demás equipos;
- alimentación utilizada;
- fecha, hora y ubicación del ensayo;
- duración y configuración impresas en los registros `SETTING`;
- contadores `SUMMARY_BLE` y `SUMMARY_WIFI`, incluidos posibles desbordamientos.

El resultado describe cuántos emisores eran visibles, en qué canales Wi-Fi
operaban y qué potencia recibía el central. El alcance del inventario comprende
visibilidad y RSSI; la ocupación temporal, el tráfico y la interferencia
requieren instrumentación adicional. El procedimiento completo y el formato de
salida están en [EJECUTAR_SONDEO_REDES.md](../EJECUTAR_SONDEO_REDES.md).

---

## 4. Resultados del sondeo radioeléctrico

Se realizó un nuevo sondeo en octubre. El archivo disponible,
[sondeo_redes_10_2026.txt](../resultados/sondeo_redes_10_2026.txt), contiene una
sesión finalizada correctamente: cinco direcciones BLE sobre una capacidad de
128 y un BSSID Wi-Fi sobre una capacidad de 64, con todos los reportes
almacenados. Sin embargo, sus resultados coinciden exactamente con los del
[archivo de julio](../resultados/sondeo_redes.txt); entre ambos registros solo
cambia la fecha de compilación del firmware. Las cifras siguientes quedan
pendientes de revisión contra el registro original del nuevo sondeo.

### Entorno BLE

Durante 60 s se recibieron 1.703 reportes correspondientes a cinco direcciones:

> Por privacidad, las direcciones BLE, el BSSID, el SSID y los nombres de
> dispositivos ajenos al proyecto fueron sustituidos por seudónimos
> consistentes. Se conservaron las métricas radioeléctricas y temporales.

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
media de −39,78 dBm muestra la variación del RSSI durante la captura.

### Entorno Wi-Fi

Se completaron 18 barridos activos en 2,4 GHz. Todos los resultados
correspondieron a un único BSSID:

| BSSID | SSID | Canal | Seguridad reportada | Observaciones | RSSI mín./medio/máx. |
| --- | --- | ---: | --- | ---: | ---: |
| `02:00:00:00:01:01` | `red_entorno_2.4` | 4 | `UNKNOWN` | 16 | −41 / −33,31 / −28 dBm |

El punto de acceso presentó una señal fuerte en la ubicación del central. El
valor `UNKNOWN` corresponde al estado de seguridad sin clasificar informado por
el controlador.

### Alcance de estos resultados

El sondeo describe un entorno visible acotado, con cinco direcciones BLE y un
punto de acceso Wi-Fi cercano. La caracterización comprende presencia, canal y
RSSI de los emisores observados.

El archivo con sufijo `10_2026` registra una compilación del 11 de octubre de
2026, pero no incluye la fecha ni la hora de inicio de la captura. Hasta
verificar las métricas del nuevo sondeo y las condiciones del montaje, estas
cifras no se usan para caracterizar el entorno del ensayo de rendimiento y
consumo ni para atribuir sus pérdidas a los emisores observados. El
[análisis completo](../resultados/SONDEO_REDES_ANALISIS.md) detalla esa captura.

---

## 5. Resultados del preexperimento de capacidad

La caracterización válida utilizó PHY 2M, Data Length 251, MTU 247 e intervalo
de conexión de 50 ms. El ESP32-S3 recibió 30.869 paquetes completos; no se
registraron descartes, errores, paquetes incompletos ni desbordamiento:

| Muestras por paquete | Fragmentos | Capacidad | Throughput útil | Latencia transporte P95 |
| ---: | ---: | ---: | ---: | ---: |
| 15 B | 1 | 60,10 pkt/s | 7,21 kbps | 49,50 ms |
| 30 B | 1 | 80,29 pkt/s | 19,27 kbps | 48,79 ms |
| 60 B | 1 | 1191,79 pkt/s | 572,06 kbps | 5,33 ms |
| 120 B | 1 | 907,28 pkt/s | 870,99 kbps | 6,23 ms |
| 240 B | 2 | 452,69 pkt/s | 869,16 kbps | 6,83 ms |
| 480 B | 3 | 264,44 pkt/s | 1015,46 kbps | 8,79 ms |
| 960 B | 5 | 142,43 pkt/s | 1093,83 kbps | 12,20 ms |

Con 15 y 30 B de muestras se observaron ráfagas de aproximadamente tres y cuatro
paquetes, respectivamente, separadas por intervalos cercanos a los 50 ms de la
conexión. Con 60 B, las recepciones sucesivas pasaron a separarse
aproximadamente 0,8 ms después del primer evento. Al pasar de 30 a 60 B, la
tasa observada subió de 80,29 a 1191,79 paquetes/s y la latencia P95 bajó de
48,79 a 5,33 ms.

Una hipótesis compatible con estos registros y con la configuración de tres
búferes de transmisión del controlador es que, para los paquetes pequeños, el
software no repone datos pendientes a tiempo para prolongar el evento de
conexión. Con 60 B podría alcanzar a reponerlos durante el evento. Los registros
del host no muestran directamente el estado de los controladores ni permiten
confirmar esa secuencia causal; el detalle y las pruebas propuestas están en el
[análisis del preexperimento](../resultados/PREEXPERIMENTO_01_ANALISIS.md).

Entre 120 y 960 B, aumentar el paquete reduce la cantidad de paquetes por
segundo, pero el throughput crece porque se amortizan los encabezados y las
llamadas al software BLE. El paquete con 960 B de muestras necesita cinco
fragmentos y obtuvo el mayor throughput útil, 1.090 kbps, con un tiempo de
transporte P95 de 12,20 ms.
Para elegir una duración de ventana en el ensayo siguiente se utiliza el tiempo
del paquete completo, medido al recibir el último fragmento.

Estos resultados justificaron dos decisiones de la etapa con enventanado: excluir
S1-E5 y S1-E10, que usarían paquetes con 15 y 30 B de muestras, y agregar
una barrida de estrés con 960 B para exigir al enlace con el tamaño que alcanzó
la mayor tasa de transmisión.

---

## 6. Resultados del experimento con enventanado temporal

Los resultados de rendimiento y consumo de esta sección corresponden a la
**misma ejecución**, registrada el 11 de octubre de 2026. El log BLE del central
es `resultados/20261011/ble_central.txt`, copia conservada de
`resultados/monitor_esp32.txt`; la corriente y los marcadores proceden del PPK2.
La ejecución anterior sin PPK2 se conserva por separado en
`resultados/EXPERIMENTOS_UNIFICADOS_ANALISIS.md`.

### Validez de la ejecución

La sesión analizada utilizó intervalo de conexión de 50 ms, latencia de
periférico 0, PHY 2M y Data Length de 251 B. Se recibieron los once registros
de estadísticas, las veinte marcas de sincronización y la calibración final,
sin errores de protocolo, fragmentos inválidos ni desbordamiento del central.

El log de trabajo contiene 25.074 registros de paquetes completos. Sus
contadores cierran exactamente:

```text
25.761 generados - 667 descartes por cola llena
                  -  20 descartes al finalizar
= 25.074 transmitidos y recibidos
```

### Offset y deriva de los relojes

Los promedios de los diez pulsos iniciales y finales produjeron:

| Parámetro | Valor medido |
| --- | ---: |
| Marca inicial XIAO | 12.158.260 µs |
| Marca inicial central | 26.230.000 µs |
| Offset inicial `central - XIAO` | +14.071.740 µs |
| Marca final XIAO | 244.810.045 µs |
| Marca final central | 258.880.000 µs |
| Offset final `central - XIAO` | +14.069.955 µs |
| Intervalo medido por el XIAO | 232.651.785 µs |
| Intervalo medido por el central | 232.650.000 µs |
| Escala `central / XIAO` | 0,999992327589 |
| Deriva de la escala | −7,6724 ppm |

El offset disminuyó 1.785 µs durante 232,65 s. Expresado de forma intuitiva,
el reloj del XIAO avanzó aproximadamente 7,6725 ppm más rápido que el del
central, unos 7,7 µs por segundo. A ese ritmo acumularía alrededor de
27,6 ms por hora. La corrección temporal incorpora el offset inicial y la
pendiente.

### Métricas de transmisión por perfil

La latencia extremo a extremo se mide desde el inicio nominal de la ventana
generada por software hasta la recepción de su último fragmento. Se divide en
latencia pre-envío y latencia de transporte desde que el transmisor toma el
paquete. Las columnas de latencia muestran `media / P95`.

| Perfil | Muestras por paquete | Ventana | Pérdida | Throughput útil | Cola máx. | Pre-envío media/P95 | Extremo a extremo media/P95 | Transporte media/P95 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| S1-E20 | 60 B | 20 ms | 0,00 % | 24,02 kbps | 0/8 | 30,71 / 39,56 ms | 56,76 / 79,67 ms | 26,05 / 47,76 ms |
| S2-E5 | 60 B | 5 ms | 3,49 % | 92,86 kbps | 8/8 | 31,72 / 56,39 ms | 45,91 / 73,05 ms | 14,19 / 44,69 ms |
| S2-E10 | 120 B | 10 ms | 0,00 % | 96,19 kbps | 6/8 | 23,88 / 36,20 ms | 45,37 / 66,48 ms | 21,49 / 44,90 ms |
| S2-E20 | 240 B | 20 ms | 0,00 % | 96,19 kbps | 4/8 | 31,97 / 42,50 ms | 56,84 / 82,47 ms | 24,86 / 46,72 ms |
| S3-E5 | 240 B | 5 ms | 0,62 % | 382,85 kbps | 8/8 | 24,74 / 41,52 ms | 33,84 / 51,42 ms | 9,10 / 31,27 ms |
| S3-E10 | 480 B | 10 ms | 0,00 % | 384,96 kbps | 4/8 | 29,75 / 45,12 ms | 41,57 / 58,01 ms | 11,82 / 35,94 ms |
| S3-E20 | 960 B | 20 ms | 0,00 % | 384,77 kbps | 5/8 | 37,14 / 49,93 ms | 56,97 / 72,87 ms | 19,84 / 42,68 ms |
| STRESS-960-E10 | 960 B | 10 ms | 0,05 % | 770,30 kbps | 6/8 | 30,50 / 53,54 ms | 43,04 / 66,61 ms | 12,54 / 34,98 ms |
| STRESS-960-E8 | 960 B | 8 ms | 0,08 % | 961,54 kbps | 8/8 | 53,03 / 84,64 ms | 64,03 / 95,84 ms | 11,00 / 12,77 ms |
| STRESS-960-E7 | 960 B | 7 ms | 0,63 % | 1093,63 kbps | 8/8 | 49,43 / 103,73 ms | 59,60 / 114,18 ms | 10,17 / 12,38 ms |
| STRESS-960-E6 | 960 B | 6 ms | 14,97 % | 1092,48 kbps | 8/8 | 80,09 / 98,91 ms | 90,45 / 109,25 ms | 10,36 / 12,38 ms |

### Interpretación del rendimiento

En esta ejecución no perdieron ventanas S1-E20, S2-E10, S2-E20, S3-E10 y
S3-E20. Su throughput útil se aproximó a los 24, 96 y 384 kbps derivados del
modelo de señal. S2-E5 perdió 3,49 % y S3-E5 perdió 0,62 %; ambos ocuparon las
ocho posiciones de la cola. La frecuencia de paquetes importa además de la tasa
de datos: para S2, E5 genera 200 paquetes/s, E10 100 y E20 50.

Para S2, E10 y E20 recibieron todos los paquetes. E10 obtuvo una latencia
extremo a extremo P95 de 66,48 ms y ocupó hasta 6/8 posiciones de cola; E20
alcanzó 82,47 ms y 4/8 posiciones. E10 prioriza latencia, mientras E20 deja
mayor margen en la cola y genera la mitad de paquetes.

Para S3, E10 y E20 tampoco perdieron paquetes. E10 alcanzó 58,01 ms de
latencia P95 y 4/8 posiciones de cola; E20 alcanzó 72,87 ms y 5/8. La ventana
E5 llegó a 51,42 ms, pero tuvo pérdida y llenó la cola. E20 requiere cinco
notificaciones por ventana y E10 tres; la comparación energética de estas dos
ventanas se presenta abajo.

Los perfiles de estrés tuvieron pérdida: 0,05 % con E10, 0,08 % con E8,
0,63 % con E7 y 14,97 % con E6. El throughput útil máximo fue 1093,63 kbps
en E7. Al pasar de E7 a E6, la carga ofrecida subió de aproximadamente
1097 a 1280 kbps, pero el throughput recibido permaneció cerca de 1093 kbps
y aumentó la pérdida. E8, E7 y E6 llenaron la cola de ocho posiciones.

El techo de aproximadamente 1090 kbps observado en el preexperimento es
coherente con esta saturación. En el ensayo con enventanado, la producción es
independiente de BLE y la cola absorbe variaciones del servicio del enlace.
La recepción queda agrupada por los eventos de conexión. El jitter P95 se ubicó
entre 1,7 y 37,9 ms y su cálculo considera los saltos de identificador:

```text
jitter = |delta_rx - delta_packet_id × período_de_generación|
```

### Consumo energético de la misma ejecución

Se capturaron corriente y marcadores digitales del XIAO durante los once
perfiles de rendimiento descritos arriba. El PPK2 alimentó la
placa a 5,0 V y registró 300 s a 10 kHz. La tabla incluye los siete perfiles
de adquisición y los cuatro de estrés. La energía corresponde al pulso completo
de aproximadamente 20,1 s y el denominador energético cuenta los bytes útiles
recibidos en esa misma ejecución.

La columna de pérdida procede del mismo log BLE que la tabla de rendimiento,
no del PPK2. Se verificó que los once valores de pérdida y los conteos de paquetes
recibidos usados para calcular energía por byte coinciden exactamente con
`resultados/20261011/ble_metricas.csv`.

| Perfil | Pérdida (BLE con PPK2) | Corriente media | Energía | Energía/byte recibido | Latencia extremo a extremo P95 |
| --- | ---: | ---: | ---: | ---: | ---: |
| S1-E20 | 0,00 % | 3,674 mA | 368,52 mJ | 6,136 µJ/B | 79,67 ms |
| S2-E5 | 3,49 % | 4,549 mA | 456,85 mJ | 1,968 µJ/B | 73,05 ms |
| S2-E10 | 0,00 % | 4,119 mA | 413,15 mJ | 1,718 µJ/B | 66,48 ms |
| S2-E20 | 0,00 % | 4,082 mA | 409,86 mJ | 1,704 µJ/B | 82,47 ms |
| S3-E5 | 0,62 % | 6,156 mA | 618,57 mJ | 0,646 µJ/B | 51,42 ms |
| S3-E10 | 0,00 % | 5,702 mA | 572,85 mJ | 0,595 µJ/B | 58,01 ms |
| S3-E20 | 0,00 % | 5,472 mA | 549,91 mJ | 0,572 µJ/B | 72,87 ms |
| STRESS-960-E10 | 0,05 % | 7,538 mA | 757,39 mJ | 0,393 µJ/B | 66,61 ms |
| STRESS-960-E8 | 0,08 % | 8,538 mA | 859,04 mJ | 0,357 µJ/B | 95,84 ms |
| STRESS-960-E7 | 0,63 % | 9,246 mA | 930,38 mJ | 0,340 µJ/B | 114,18 ms |
| STRESS-960-E6 | 14,97 % | 9,260 mA | 931,69 mJ | 0,341 µJ/B | 109,25 ms |

Para S2, E20 ahorró 0,8 % de energía frente a E10, con 16,0 ms más de latencia
P95. Para S3, E20 ahorró 4,0 % frente a E10, con 14,9 ms más de latencia P95.
La primera diferencia energética es pequeña para juzgarla con una sola captura.
S3-E10 y S3-E20 no perdieron paquetes en esta ejecución. Los perfiles E5 sí
perdieron paquetes y llenaron la cola.

Los perfiles de estrés muestran cómo el consumo aumenta al exigir una tasa de
datos mayor. STRESS-960-E6 alcanzó 9,260 mA de corriente media y perdió 14,97 %
de las ventanas; estos perfiles no representan una configuración de adquisición
EMG y no se consideran candidatos para elegir la ventana.

El [análisis completo](../resultados/20261011/ANALISIS_CONSUMO.md) detalla las
once filas, la energía fuera de perfiles y el tratamiento de los valores `X`
aislados en los cambios de marcadores. La medición abarca la placa XIAO completa,
incluido el regulador de entrada. Faltan repeticiones para cuantificar la
variación de consumo y pérdida entre sesiones.

---

## 7. Conclusiones

En S1 se ensayó E20 sin pérdidas; E5 y E10 se descartaron después del
preexperimento de capacidad, por lo que no se compararon bajo generación
temporal. Para S2, E10 y E20 tampoco perdieron ventanas. E10 redujo la latencia
P95 extremo a extremo de 82,47 a 66,48 ms, mientras E20 redujo la ocupación
máxima de cola de 6/8 a 4/8 y generó la mitad de paquetes. La diferencia de
energía de 0,8 % a favor de E20 en una sola captura no permite establecer una
ventaja energética firme.

Para S3, E10 y E20 conservaron todas las ventanas. E10 obtuvo menor latencia
P95 (58,01 frente a 72,87 ms) y menor ocupación máxima de cola (4/8 frente a
5/8); E20 consumió 4,0 % menos energía en esta captura. E5 dio menor latencia,
pero perdió ventanas y llenó la cola. En los perfiles de estrés, la pérdida
alcanzó 14,97 % mientras el throughput recibido se mantuvo cerca de 1090 kbps.

---

## 8. Referencias

- A. Visconti et al., [*Electromyography Monitoring Systems in Rehabilitation:
  A Review of Clinical Applications, Wearable Devices and Signal Acquisition
  Methodologies*](https://www.mdpi.com/2079-9292/12/7/1520), Electronics, 2023.
  Contexto de análisis muscular, rehabilitación y dispositivos portables.
- [*Recent trends and challenges of surface electromyography in prosthetic
  applications*](https://pmc.ncbi.nlm.nih.gov/articles/PMC10382439/), 2023.
  Contexto del uso de EMG en interfaces de control de prótesis.
- [*Human activity monitoring system based on wearable sEMG and accelerometer
  wireless sensor nodes*](https://pmc.ncbi.nlm.nih.gov/articles/PMC6245594/), 2018.
  Ejemplo de monitoreo de actividad física con varios nodos inalámbricos.
- [*Robust skin-integrated conductive biogel for high-fidelity detection under
  mechanical stress*](https://www.nature.com/articles/s41467-024-55417-1), 2024.
  Ejemplo de adquisición inalámbrica de sEMG durante ejercicio en exteriores.
- E. Aghchehli et al., [*Digital Sensing Systems for
  Electromyography*](https://doi.org/10.1109/TNSRE.2024.3435740), IEEE Transactions
  on Neural Systems and Rehabilitation Engineering, 2024. Explica la captación
  de interferencias en cables analógicos y la digitalización cerca del electrodo.
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
