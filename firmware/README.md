# Firmware compilado

Las parejas principales son:

```text
central\preexperimento_esp32s3_n16r8.bin
periferico\preexperimento_xiao_nrf52840.uf2

central\experimentos_esp32s3_n16r8.bin
periferico\experimentos_xiao_nrf52840.uf2

central\sondeo_redes_esp32s3_n16r8.bin
```

Los dos archivos de cada pareja deben grabarse juntos. El firmware
`experimentos_*` ejecuta automáticamente los once perfiles de 20 s y contiene
los marcadores digitales para medir consumo. El firmware `preexperimento_*`
caracteriza el enlace por saturación y no contiene esos marcadores.
`sondeo_redes_*` se graba solamente en el central y genera el inventario BLE y
Wi-Fi del entorno.

Para grabar el central unificado:

```bat
flash_experimentos_central.bat COM11
```

Para el XIAO, pulsar dos veces Reset y copiar el UF2 a la unidad del bootloader.

Para grabar el sondeo:

```bat
flash_sondeo_redes_central.bat COM11
```

Procedimientos:

- [preexperimento](../EJECUTAR_PREEXPERIMENTO.md)
- [experimento unificado](../EJECUTAR_EXPERIMENTOS.md)
- [sondeo de redes](../EJECUTAR_SONDEO_REDES.md)
