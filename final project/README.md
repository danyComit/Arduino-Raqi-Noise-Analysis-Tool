# Classroom Air Monitor (ESP32 + RAQI + Noise)

- ESP32 + MQ gas module (RAQI via analog) + analog mic (noise RMS & spikes)
- USB logger writes CSV to `logs/sensors_YYYY-MM-DD.csv` (1 Hz)
- Use `analyze.py` for quick plots & KPIs

## Wiring
- MQ: VCC=5V, GND=GND, AO -> (equal-resistor divider) -> ESP32 GPIO34
- Mic: VCC=3V3, GND=GND, AO -> ESP32 GPIO35
- Common GND; bring 3V3/5V/GND to rails from ESP32

## Run
```bash
source .venv/bin/activate
python autolog.py Room101