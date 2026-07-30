# Periférico XIAO nRF52840

El periférico anuncia `Xiao_GATT_Min`, expone el servicio GATT del proyecto,
genera ventanas simuladas de muestras de 24 bits y las transmite al central.

## Firmwares

- `preexperiment.c`: caracterización por saturación de siete payloads. No
  incluye marcadores de consumo.
- `postexperiment.c`: secuencia periódica unificada de once perfiles, 20 s cada
  uno, con cola finita e instrumentación energética.

En el experimento unificado, un temporizador produce las ventanas con frecuencia
fija y un hilo separado las transmite:

```text
temporizador → cola de 8 ventanas → transmisor BLE
```

Si la cola se llena, se contabiliza el descarte sin retrasar la adquisición.

## Compilación

```bat
build_preexperiment.bat
build_postexperiment.bat
```

Resultados:

```text
build\preexperiment\zephyr\zephyr.uf2
build\postexperiment\zephyr\zephyr.uf2
```

## Pines del experimento unificado

| Pin | Uso |
| --- | --- |
| D0 / P0.02 | adquisición activa |
| D1 / P0.03 | transmisor activo |
| D2 / P0.28 | entrada de sincronización |
| D3 / P0.29 | advertising activo |
| D4 / P0.04 | conexión activa |
| D5 / P0.05 | perfil activo |

El overlay compartido deshabilita `i2c1` para liberar D4 y D5 en ambos builds.
Los marcadores se inicializan en nivel bajo y solamente se compilan en
`postexperiment`.

El formato del protocolo y los perfiles están en
[common/README.md](../../common/README.md).
