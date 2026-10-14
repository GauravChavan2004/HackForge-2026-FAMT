#include "DHT.h"

// ==================================================
// PIN CONFIGURATION
// ==================================================
#define DHTPIN 6
#define DHTTYPE DHT11

const int PIR_PIN = 7;
const int TOUCH_PIN = 5;

const int LED_LOW = 8;
const int LED_BALANCED = 9;
const int LED_HIGH = 10;
const int LED_MOTION = 2;

const int BUZZER_PIN = 3;

// ==================================================
// TEMPERATURE THRESHOLDS
// ==================================================
const float TEMP_LOW = 22.0;
const float TEMP_HIGH = 40.0;

// ==================================================
// TIMING
// ==================================================
const unsigned long SENSOR_INTERVAL = 2500;
const unsigned long TOUCH_DEBOUNCE = 50;
const unsigned long MOTION_DEBOUNCE = 100;

// ==================================================
// SENSOR OBJECT AND VARIABLES
// ==================================================
DHT dht(DHTPIN, DHTTYPE);

float temperature = NAN;
float humidity = NAN;

bool assistanceRequested = false;

// Touch sensor state
bool lastTouchReading = LOW;
bool stableTouchState = LOW;
unsigned long touchChangeTime = 0;

// PIR sensor state
bool lastMotionReading = LOW;
bool stableMotionState = LOW;
unsigned long motionChangeTime = 0;

unsigned long lastSensorRead = 0;

// ==================================================
// SETUP
// ==================================================
void setup() {
  Serial.begin(9600);
  Serial.setTimeout(20);

  dht.begin();

  pinMode(PIR_PIN, INPUT);
  pinMode(TOUCH_PIN, INPUT);

  pinMode(LED_LOW, OUTPUT);
  pinMode(LED_BALANCED, OUTPUT);
  pinMode(LED_HIGH, OUTPUT);
  pinMode(LED_MOTION, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  // Turn all outputs off initially.
  digitalWrite(LED_LOW, LOW);
  digitalWrite(LED_BALANCED, LOW);
  digitalWrite(LED_HIGH, LOW);
  digitalWrite(LED_MOTION, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize input states.
  lastTouchReading = digitalRead(TOUCH_PIN);
  stableTouchState = lastTouchReading;

  lastMotionReading = digitalRead(PIR_PIN);
  stableMotionState = lastMotionReading;

  touchChangeTime = millis();
  motionChangeTime = millis();

  Serial.println("BURN_WARD_READY");
}

// ==================================================
// MAIN LOOP
// ==================================================
void loop() {
  unsigned long now = millis();

  // Always check for acknowledgement from Flask.
  processSerialCommands();

  // --------------------------------------------------
  // TOUCH SENSOR
  // One touch creates one assistance request.
  // Request remains active until Flask acknowledges it.
  // --------------------------------------------------
  bool touchReading = digitalRead(TOUCH_PIN);

  if (touchReading != lastTouchReading) {
    lastTouchReading = touchReading;
    touchChangeTime = now;
  }

  if (now - touchChangeTime >= TOUCH_DEBOUNCE) {
    if (stableTouchState != touchReading) {
      stableTouchState = touchReading;

      // Trigger only on LOW-to-HIGH transition.
      if (stableTouchState == HIGH) {
        assistanceRequested = true;

        tone(BUZZER_PIN, 2500, 150);

        Serial.println("Assistance requested");
      }
    }
  }

  // --------------------------------------------------
  // PIR MOTION SENSOR
  // Debounce input, but update the LED immediately.
  // --------------------------------------------------
  bool motionReading = digitalRead(PIR_PIN);

  // Motion LED directly follows the PIR output.
  digitalWrite(
    LED_MOTION,
    motionReading == HIGH ? HIGH : LOW
  );

  if (motionReading != lastMotionReading) {
    lastMotionReading = motionReading;
    motionChangeTime = now;
  }

  if (now - motionChangeTime >= MOTION_DEBOUNCE) {
    if (stableMotionState != motionReading) {
      stableMotionState = motionReading;

      if (stableMotionState == HIGH) {
        // Beep only when motion starts.
        tone(BUZZER_PIN, 2000, 150);
        Serial.println("Motion Detected");
      } else {
        Serial.println("Motion Stopped");
      }
    }
  }

  // Use stable PIR state in JSON.
  bool motion = stableMotionState;

  // --------------------------------------------------
  // DHT11 TEMPERATURE AND HUMIDITY
  // --------------------------------------------------
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      temperature = t;
      humidity = h;
    }

    updateTemperatureLEDs();
    sendStatus(motion);
  }
}

// ==================================================
// TEMPERATURE LED CONTROL
// ==================================================
void updateTemperatureLEDs() {
  if (isnan(temperature)) {
    digitalWrite(LED_LOW, LOW);
    digitalWrite(LED_BALANCED, LOW);
    digitalWrite(LED_HIGH, LOW);
    return;
  }

  // Below 22 degrees: LOW LED.
  digitalWrite(
    LED_LOW,
    temperature < TEMP_LOW ? HIGH : LOW
  );

  // 22 to 40 degrees inclusive: BALANCED LED.
  digitalWrite(
    LED_BALANCED,
    (temperature >= TEMP_LOW &&
     temperature <= TEMP_HIGH) ? HIGH : LOW
  );

  // Above 40 degrees: HIGH LED.
  digitalWrite(
    LED_HIGH,
    temperature > TEMP_HIGH ? HIGH : LOW
  );
}

// ==================================================
// SEND JSON STATUS TO FLASK
// ==================================================
void sendStatus(bool motion) {
  Serial.print("{\"temperature\":");

  if (isnan(temperature)) {
    Serial.print("null");
  } else {
    Serial.print(temperature, 1);
  }

  Serial.print(",\"humidity\":");

  if (isnan(humidity)) {
    Serial.print("null");
  } else {
    Serial.print(humidity, 1);
  }

  Serial.print(",\"motion\":");
  Serial.print(motion ? "true" : "false");

  Serial.print(",\"assistance\":");
  Serial.print(assistanceRequested ? "true" : "false");

  Serial.println("}");
}

// ==================================================
// RECEIVE COMMANDS FROM FLASK
// Expected command: {"ack":true}
// ==================================================
void processSerialCommands() {
  static String command = "";

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      command.trim();

      if (command.length() > 0) {
        processCommand(command);
      }

      command = "";
    } else {
      command += c;

      // Protect memory from unexpectedly long commands.
      if (command.length() > 100) {
        command = "";
      }
    }
  }
}

// ==================================================
// CLEAR ASSISTANCE REQUEST
// ==================================================
void processCommand(String command) {
  command.trim();

  if (command.indexOf("\"ack\":true") >= 0) {
    assistanceRequested = false;

    // Stop the buzzer if it is still sounding.
    noTone(BUZZER_PIN);

    Serial.println("Assistance acknowledged");
  }
}