from flask import Flask, jsonify, render_template
import serial
import threading
import json
import time
import os

app = Flask(__name__)

# Arduino serial configuration
SERIAL_PORT = os.getenv("ARDUINO_PORT", "COM11")
BAUD_RATE = 9600

arduino = None
serial_lock = threading.Lock()
data_lock = threading.Lock()

latest_data = {
    "temperature": None,
    "humidity": None,
    "motion": False,
    "assistance": False,
    "connected": False,
    "updated_at": None,
    "error": "Waiting for Arduino"
}


def serial_reader():
    global arduino

    while True:
        try:
            # Connect to Arduino when necessary.
            if arduino is None or not arduino.is_open:
                with serial_lock:
                    if arduino is None or not arduino.is_open:
                        arduino = serial.Serial(
                            SERIAL_PORT,
                            BAUD_RATE,
                            timeout=1
                        )

                # Arduino may reset when the port opens.
                time.sleep(2)

            # Read one complete line.
            with serial_lock:
                if arduino is None or not arduino.is_open:
                    continue

                line = arduino.readline().decode(
                    "utf-8", errors="ignore"
                ).strip()

            if not line:
                continue

            # Arduino also sends debug text.
            # Ignore anything that is not valid JSON.
            try:
                message = json.loads(line)
            except json.JSONDecodeError:
                print("Arduino:", line)
                continue

            if not isinstance(message, dict):
                continue

            # Accept only sensor status messages.
            if "temperature" not in message or "motion" not in message:
                continue

            with data_lock:
                latest_data.update({
                    "temperature": message.get("temperature"),
                    "humidity": message.get("humidity"),
                    "motion": bool(message.get("motion", False)),
                    "assistance": bool(
                        message.get("assistance", False)
                    ),
                    "connected": True,
                    "updated_at": time.strftime("%H:%M:%S"),
                    "error": None
                })

        except (serial.SerialException, OSError) as exc:
            print("Serial connection error:", exc)

            old_arduino = arduino
            arduino = None

            if old_arduino is not None:
                try:
                    old_arduino.close()
                except Exception:
                    pass

            with data_lock:
                latest_data["connected"] = False
                latest_data["error"] = str(exc)

            time.sleep(2)

        except Exception as exc:
            print("Reader error:", exc)

            with data_lock:
                latest_data["error"] = str(exc)

            time.sleep(1)


@app.route("/")
def dashboard():
    # Flask loads templates/index.html
    return render_template("index.html")


@app.route("/api/status", methods=["GET"])
def status():
    with data_lock:
        return jsonify(latest_data.copy())


@app.route("/api/acknowledge", methods=["POST"])
def acknowledge():
    global arduino

    try:
        with serial_lock:
            if arduino is None or not arduino.is_open:
                return jsonify({
                    "ok": False,
                    "error": "Arduino is not connected"
                }), 503

            # Arduino expects a newline-terminated command.
            arduino.write(b'{"ack":true}\n')
            arduino.flush()

        return jsonify({
            "ok": True,
            "message": "Acknowledgement sent"
        })

    except Exception as exc:
        print("Acknowledgement error:", exc)

        return jsonify({
            "ok": False,
            "error": str(exc)
        }), 500


if __name__ == "__main__":
    # Start exactly one background serial reader.
    threading.Thread(
        target=serial_reader,
        daemon=True
    ).start()

    app.run(
        host="127.0.0.1",
        port=5000,
        debug=False,
        use_reloader=False
    )