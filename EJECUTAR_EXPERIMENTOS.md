# Ejecución del experimento periódico unificado

## 1. Compilar

Desde la raíz:

```bat
compilar_experimentos.bat
```

Esto genera:

```text
firmware\periferico\experimentos_xiao_nrf52840.uf2
firmware\central\experimentos_esp32s3_n16r8.bin
```

## 2. Grabar y cablear

Copiar el UF2 al XIAO en modo bootloader y grabar el central con:

```bat
firmware\flash_experimentos_central.bat COM11
```

Sincronización:

```text
ESP32-S3 GPIO4 --- resistencia 1 kΩ --- XIAO D2 / P0.28
ESP32-S3 GND -------------------------- XIAO GND
```

Para medir energía, alimentar el XIAO con el PPK2 y conectar las entradas
digitales requeridas según [PLAN_EXPERIMENTAL.md](PLAN_EXPERIMENTAL.md).
No alimentar simultáneamente el XIAO desde USB y desde el PPK2 salvo que el
montaje usado esté preparado expresamente para ello.

## 3. Capturar

Con ambos firmwares ejecutándose:

```bat
capturar_experimentos.bat COM11
```

La sesión comienza automáticamente después de conectarse. Ejecuta 11 perfiles
de 20 s con pausas de 1 s, sincroniza los relojes y recién entonces vuelca los
datos almacenados. No cerrar la terminal hasta leer `Session complete`.

El archivo predeterminado es:

```text
resultados\experimentos_unificados.txt
```

## 4. Corregir tiempos y analizar

```bat
python tools\corregir_tiempos.py ^
  resultados\experimentos_unificados.txt ^
  -o resultados\experimentos_unificados_corregido.csv

python tools\analizar_experimentos.py ^
  resultados\experimentos_unificados_corregido.csv ^
  resultados\experimentos_unificados.txt ^
  -o resultados\experimentos_metricas.csv
```

El CSV final contiene una fila por perfil, incluso si alguno no recibió
paquetes, y reúne producción, entrega, cola, throughput, latencia y jitter.
