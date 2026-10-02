# Simulador CAT FT-817/818/857/897

`ft8x7_sim.py` responde por un puerto serie (8N2) como la radio, para probar el
display sin el equipo. Necesita `pyserial` (`pip3 install pyserial`).

```bash
# Con un adaptador USB-TTL de 3.3V conectado a la CYD
python3 tools/ft8x7_sim.py /dev/cu.usbserial-XXXX --baud 4800 --model 857 -v

# Sin hardware: crea un puerto virtual /dev/ttysNNN
python3 tools/ft8x7_sim.py --pty -v

# Opciones
#   --model 817|818|857|897   (0xBD medidores TX sólo en 817/818; bit "narrow" sólo en 857/897)
#   --static                  estado fijo, sin animación
#   --drop-rate 0.1           pierde el 10 % de las respuestas para probar timeouts
```

Sin `--static`, el simulador cambia de canal cada 10 s, mueve un poco la
sintonía y el S-meter, y transmite 4 s de cada 20 s, con SWR alta en FM.

## Cableado con la CYD (conector CN1)

Usar un adaptador USB-TTL **de 3.3V** (con uno de 5V hace falta el adaptador de nivel).

| USB-TTL | CYD CN1          |
|---------|------------------|
| TX      | IO27 (CAT RX)    |
| RX      | IO22 (CAT TX)    |
| GND     | GND              |

No conectes el VCC del adaptador si la CYD ya está alimentada por su USB.

## Tests

```bash
python3 -m unittest discover -s tools
```
