# Análisis del experimento periódico unificado

## 1. Validez de la captura

Archivo analizado: `experimentos_unificados.txt`.

La sesión contiene:

- los once registros `P`;
- veinte registros de sincronización `S`;
- un registro de calibración `C`;
- ningún registro de error `E`;
- cierre `Session complete`;
- intervalo de conexión de 50 ms y latencia 0;
- PHY TX/RX 2M;
- Data Length TX/RX de 251 B.

La calibración temporal obtenida fue:

| Parámetro | Valor |
| --- | ---: |
| Offset inicial `central - XIAO` | −83.305.878 µs |
| Offset final `central - XIAO` | −83.307.459 µs |
| Intervalo XIAO | 232.651.681 µs |
| Intervalo central | 232.650.100 µs |
| Escala `central / XIAO` | 0,999993204433 |
| Deriva de la escala | −6,7956 ppm |

Durante los 232,65 s entre grupos de sincronización, el contador del XIAO
avanzó 1.581 µs más que el del central. Equivale a que el reloj del XIAO corrió
aproximadamente 6,8 ppm más rápido, unos 6,8 µs por segundo. El offset absoluto
se debe principalmente a que ambos contadores comenzaron en instantes distintos;
su variación y la escala son las magnitudes que permiten corregir la deriva.

El log analizado contiene 21.377 paquetes válidos, exactamente:

```text
25.760 generados - 4.340 descartes de cola - 43 descartes finales
= 21.377 transmitidos y recibidos
```

El registro global del central, `R,records,21377`, coincide con ese total. El
corrector temporal verifica además que todos los registros pertenezcan al
intervalo sincronizado.

## 2. Resultados

Las columnas de latencia muestran `media / P95`.

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

## 3. Interpretación

Los perfiles sostenibles en esta ejecución son:

- S1-E20;
- S2-E10;
- S2-E20;
- S3-E20.

S2-E5, S3-E5 y S3-E10 llenaron la cola y perdieron ventanas. Por lo tanto, una
ventana pequeña no resulta automáticamente mejor aunque reduzca el tiempo de
empaquetado: produce más paquetes por intervalo de conexión y exige absorber
ráfagas de servicio con una cola de solo ocho posiciones.

Para S2, E10 y E20 entregaron todo el caudal. E10 obtuvo menor latencia extremo
a extremo P95, pero alcanzó 7/8 posiciones; E20 utilizó 5/8 y requiere la mitad
de paquetes, aunque cada paquete tiene dos fragmentos. La medición energética
será importante para decidir entre ambos.

Para S3, E20 fue la única ventana sin pérdida. E10 perdió 2,79 % y E5 perdió
13,00 %. S3-E20 alcanza el caudal objetivo, pero su latencia pre-envío P95 de
104,04 ms muestra que opera con menos margen temporal que los perfiles S2.

La barrida de estrés confirma que 960 B cada 10 ms ya no es sostenible con la
arquitectura actual. El límite periódico está entre 50 pkt/s —S3-E20— y
100 pkt/s —STRESS-960-E10—. Los ensayos de 8, 7 y 6 ms aumentan la carga
ofrecida, pero la entrega cae y el throughput recibido alcanza un máximo no
utilizable de 754,94 kbps con 21,52 % de pérdida.

El techo de 1,09 Mbps observado por saturación en el preexperimento no se
traduce directamente en una tasa periódica sostenible. Con generación
periódica, el intervalo de conexión de 50 ms concentra el servicio, la cola
debe absorber las variaciones y el transmisor mantiene una sola notificación
de aplicación en vuelo.

## 4. Jitter

La recepción está agrupada por los eventos de conexión de 50 ms. Por eso el
jitter P95 se encuentra aproximadamente entre 28 y 46 ms, aunque la adquisición
sea periódica. El cálculo corregido usa el salto de `packet_id` para no convertir
una ventana descartada en jitter artificial:

```text
jitter = |delta_rx - delta_packet_id × período_de_generación|
```

## 5. Próximos pasos

1. Evitar reiniciar solo el central mientras el XIAO conserva una sesión activa,
   o detener la generación automáticamente al desconectar.
2. Medir con PPK2 los perfiles sostenibles y conservar también los perfiles con
   pérdida como referencia de sobrecarga.
3. Si se necesita localizar con mayor precisión el límite de 960 B, agregar
   períodos intermedios entre 10 y 20 ms.
4. Comparar S2-E10 y S2-E20 mediante energía por byte útil recibido.
