# Burn Ward ICU Room Monitor

A clinical monitoring prototype that combines an Arduino sensor controller with a Flask dashboard. It reports room temperature, humidity, patient movement, and staff assistance requests. The dashboard displays live readings, short-term charts, connection status, and an acknowledgement control.

> **Prototype notice:** This project is for demonstration and development. It is not a certified medical device and must not be used to make clinical decisions or replace hospital monitoring systems.

## Project structure

```text
hackthon/
├── app.py
├── templates/
│   └── index.html
└── sketch_oct10c/
    └── sketch_oct10c.ino
```

- `app.py` runs the Flask web server, reads Arduino serial telemetry, and provides the status and acknowledgement API.
- `templates/index.html` is the dashboard served by Flask. It uses Chart.js from jsDelivr and the browser Web Audio API for alarm sounds.
- `sketch_oct10c/sketch_oct10c.ino` reads the DHT11, PIR, and touch sensors, drives indicator LEDs and a buzzer, and exchanges JSON status/acknowledgement messages over USB serial.

## Features

- Live temperature and humidity readings with up to 15 recent samples charted.
- PIR motion status and a persistent assistance request triggered by a touch input.
- Temperature categories and indicator LEDs: below 22 °C, 22–40 °C inclusive, and above 40 °C.
- Visual and audio browser alerts for a newly raised assistance request.
- Dashboard acknowledgement that clears the Arduino assistance request.
- Automatic serial reconnect attempts when the Arduino is unavailable.

## Hardware

The sketch expects an Arduino-compatible board with these connections:

| Component | Arduino pin |
| --- | ---: |
| DHT11 data | 6 |
| PIR output | 7 |
| Touch sensor output | 5 |
| Low temperature LED | 8 |
| Balanced temperature LED | 9 |
| High temperature LED | 10 |
| Motion LED | 2 |
| Buzzer | 3 |

Connect sensor power and ground according to each module's requirements. Use appropriate current-limiting resistors for LEDs. Confirm the board's logic voltage and pin capabilities before wiring.

## Requirements

- Python 3
- Flask
- pyserial
- Arduino IDE or another compatible build/upload tool
- The Arduino `DHT` library
- A browser with JavaScript enabled and network access to load Chart.js from the CDN

Install the Python packages:

```bash
python -m pip install Flask pyserial
```

In Arduino IDE, install the `DHT sensor library` (and any dependency requested by the library manager), open `sketch_oct10c/sketch_oct10c.ino`, select the connected board and port, and upload the sketch.

## Run the dashboard

By default, the Flask app looks for the Arduino on `COM11` at 9600 baud. Set `ARDUINO_PORT` if your serial port is different.

PowerShell:

```powershell
$env:ARDUINO_PORT = "COM11"
python app.py
```

macOS/Linux:

```bash
ARDUINO_PORT=/dev/ttyACM0 python app.py
```

Replace the example port with the one assigned to your board (for example, `/dev/ttyUSB0` on some Linux systems). Close the Arduino IDE Serial Monitor or any other program using the port before starting Flask.

Open [http://127.0.0.1:5000](http://127.0.0.1:5000) in a browser. The server binds to localhost on port 5000 and is intended for local development.

## API

| Method | Endpoint | Description |
| --- | --- | --- |
| `GET` | `/api/status` | Returns the most recently received sensor readings and connection state as JSON. |
| `POST` | `/api/acknowledge` | Sends `{"ack":true}` to the Arduino to clear an active assistance request. Returns HTTP 503 if the Arduino is disconnected. |

Example status response:

```json
{
  "temperature": 25.4,
  "humidity": 48.0,
  "motion": false,
  "assistance": false,
  "connected": true,
  "updated_at": "12:34:56",
  "error": null
}
```

The Arduino emits a newline-terminated JSON object at approximately 2.5-second intervals. Its fields are `temperature`, `humidity`, `motion`, and `assistance`. Temperature and humidity can be `null` until a valid DHT11 reading is available.

## Troubleshooting

- **Dashboard reports Offline:** Check the USB connection and `ARDUINO_PORT`; ensure no other application has opened the serial port. The Flask reader retries the connection.
- **No temperature or humidity:** Check DHT11 wiring and library installation. DHT11 readings update at a 2.5-second interval.
- **Charts or audio unavailable:** Chart.js is loaded from a CDN, so the browser needs network access. Browser autoplay policies may require a prior user interaction before audio can play.
- **Port access error:** On Windows, use the correct `COM` port. On Linux, check serial device permissions.

## License

No license is included in this repository. Add a license before distributing or reusing the project.
