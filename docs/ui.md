# Interfaz táctil

Maqueta interactiva: [`ui/mockup.html`](ui/mockup.html) (abrir en el navegador; `#band`, `#mode`, … abren cada pantalla).
Coordenadas en píxeles reales de la pantalla 320×240 (rotación 1).

## Reglas

- Pantalla resistiva: botones de al menos 44 px de alto; un toque = una acción.
- Entre toques, 250 ms sin contacto y al menos 5 lecturas firmes (el resistivo rebota).
- Toda subpantalla tiene «◀ Volver» arriba a la izquierda y vuelve sola a la principal tras 10 s sin tocar.
- Banda y modo se envían sin confirmación (se deshacen fácilmente). **Nunca PTT desde la pantalla.**
- Sin enlace CAT las acciones no se envían y aparece el aviso «Sin enlace CAT».
- Colores, fuentes y estilo en una estructura de tema (tarea del tema «emisora antigua»).

## Mapa de pantallas

```
PRINCIPAL ─┬─ toca frecuencia ─→ TECLADO (MHz)
           ├─ toca modo ───────→ MODO (3×3)
           ├─ toca banda ──────→ BANDA (4×4)
           ├─ ◀ / ▶ / PASO / A/B (en la propia pantalla)
           └─ MENU ────────────→ MENÚ ─┬─ SPLIT on/off
                                       ├─ CLARIFICADOR
                                       ├─ REPETIDOR / TONO (sólo FM)
                                       ├─ LOCK on/off
                                       ├─ AJUSTES (modelo, baudios, brillo, calibrar táctil)
                                       └─ DIAGNÓSTICO (enlace, contadores, puerto)
```

## Pantalla principal

| Zona (x, y, w, h) | Muestra | Al tocar |
|---|---|---|
| 0, 22, 320, 56 | Frecuencia | Abre el teclado |
| 4, 90, 92, 44 | Modo (USB, CW-N…) | Abre el selector de modo |
| 100, 90, 76, 44 | Banda (20m, GEN…) | Abre el selector de banda |
| 182, 90 / 114 | SPLIT, SQL/CLAR | — (indicadores) |
| 246, 90, 70, 44 | RX / TX | — |
| 40, 154, 270, 18 | S-meter (RX) / PO (TX) | — |
| 4, 192, 60, 44 | ◀ | Baja un paso (mantener = repetir) |
| 68, 192, 60, 44 | PASO | 10 Hz → 100 Hz → 1k → 10k → 100k → 1M |
| 132, 192, 60, 44 | ▶ | Sube un paso (mantener = repetir) |
| 196, 192, 58, 44 | A/B | Conmuta VFO |
| 258, 192, 58, 44 | MENU | Abre el menú |

## Acciones → comandos CAT

Trama `[P1 P2 P3 P4 OPCODE]`. Frecuencias en BCD.

| Acción | Trama | Notas |
|---|---|---|
| Fijar frecuencia | `F1 F2 F3 F4 01` | 8 dígitos BCD en pasos de 10 Hz: `01 45 50 00` = 145,500 MHz |
| Modo | `MM 00 00 00 07` | 00 LSB, 01 USB, 02 CW, 03 CWR, 04 AM, 06 WFM, 08 FM, 0A DIG, 0C PKT |
| Banda | fijar frecuencia (+ modo) | No existe comando de banda: el display guarda la última frecuencia y modo por banda (NVS) |
| VFO A/B | `00 00 00 00 81` | Conmuta; la radio no informa del VFO activo por CAT |
| Split ON / OFF | `00 00 00 00 02` / `82` | |
| Clarificador ON / OFF | `00 00 00 00 05` / `85` | |
| Offset clarificador | `SS 00 C1 C2 F5` | SS = 00 «+», ≠00 «−»; C1 C2 BCD en 10 Hz: `12 34` = 12,34 kHz |
| Desplazamiento repetidor | `PP 00 00 00 09` | 09 «−», 49 «+», 89 simplex |
| Offset repetidor | `O1 O2 O3 O4 F9` | BCD en Hz: `00 60 00 00` = 0,6 MHz |
| Modo de tono | `PP 00 00 00 0A` | 0A DCS, 2A CTCSS (TSQ), 4A encoder, 8A OFF |
| Tono CTCSS | `T1 T2 00 00 0B` | `08 85` = 88,5 Hz |
| Código DCS | `D1 D2 00 00 0C` | `00 23` = 023 |
| LOCK ON / OFF | `00 00 00 00 00` / `80` | |

Lectura (sondeo continuo): `03` frecuencia y modo, `E7` estado RX, `F7` estado TX.
Prohibido: `BC` (escritura de EEPROM).

## Lecturas no documentadas

Sacadas de hamlib (`rigs/yaesu/ft817.c`, `ft857.c`, `ft897.c`). Si la radio no responde a alguna
tres veces seguidas mientras las lecturas normales sí, el display deja de pedirla (algunos FT-857
no responden a `BB`) sin dar el enlace por perdido. Diagnóstico → «Extras» muestra el resultado.

| Dato | Trama | FT-817/818 | FT-857/897 | Cuándo |
|---|---|---|---|---|
| VFO activo | `HH LL 00 00 BB` | EEPROM `0x55`, bit 0 (1 = B) | EEPROM `0x68`, bit 0 | En RX, cada segundo y tras A/B o SPLIT |
| Split | `HH LL 00 00 BB` | EEPROM `0x7A`, bit 7 | EEPROM `0x8D`, bit 7 | Igual |
| Medidores TX | `00 00 00 00 BD` | 2 bytes: `[PWR·16+ALC] [SWR·16+MOD]` | Igual (en RX responde un solo `FF`) | Sólo transmitiendo |

`BB` se pide en dirección par y la radio devuelve 2 bytes (`HH LL` y el siguiente). El split ya no
se toma del bit 5 de `F7`, cuya polaridad no coincide entre el manual y hamlib.

## Bandas

160m 1,840 LSB · 80m 3,700 LSB · 60m 5,3515 USB (no en FT-817 original) · 40m 7,100 LSB · 30m 10,120 CW ·
20m 14,200 USB · 17m 18,120 USB · 15m 21,200 USB · 12m 24,940 USB · 10m 28,500 USB · 6m 50,150 USB ·
2m 145,500 FM · 70cm 433,500 FM · AIR 125,000 AM · FM-BC 98,000 WFM
(valores iniciales; luego se recuerda la última frecuencia y modo de cada banda).

Rango de recepción de los cuatro modelos: 0,1–56 MHz, 76–108 MHz, 118–164 MHz, 420–470 MHz.
