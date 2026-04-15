#include <Wire.h>
#include <MPU6050.h>
#include <DHT.h>
#include <SoftwareSerial.h>

/************ DHT22 ************/
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

/************ MPU6050 ************/
MPU6050 mpu;

/************ Ultrasonic ************/
#define TRIG_PIN 8
#define ECHO_PIN 9

/************ Vibration (SW-420) ************/
#define VIBRATION_PIN 12

/************ Heart Rate (Analog) ************/
#define HEART_PIN A0

/************ SIM800L (GSM) ************/
#define SIM800_TX 10  // Arduino TX -> SIM800 RX
#define SIM800_RX 11  // Arduino RX <- SIM800 TX
SoftwareSerial sim800(SIM800_TX, SIM800_RX);

// Your emergency phone number (plain ASCII only)
String phoneNumber = "+8801893072703";

/************ Fall Detection ************/
int  fallThreshold = 15000;   // same threshold scheme you used
bool fallDetected  = false;

/************ Heartbeat detection state ************/
const unsigned long SAMPLE_INTERVAL_MS = 2;    // ~500 Hz sampling
unsigned long lastSampleMs = 0;

int   rawVal = 0;
float lpFiltered = 0.0f;       // low-pass filtered signal
float baseline  = 520.0f;      // running baseline (auto-learns)
float threshOffset = 12.0f;    // threshold above baseline (tune 8–20)
bool  aboveThreshold = false;

unsigned long lastBeatMs = 0;
const unsigned long REFRACTORY_MS = 250;       // ignore beats <250ms apart
unsigned long ibiMs = 0;
unsigned long ibiWindow[5] = {0,0,0,0,0};
uint8_t ibiIdx = 0;
bool haveBpm = false;
int  bpm = 0;

/************ Timers ************/
unsigned long lastPrintMs = 0;

/************ Helpers ************/
void heartbeatSampleAndDetect() {
  unsigned long now = millis();
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) return;
  lastSampleMs = now;

  rawVal = analogRead(HEART_PIN);                      // 0..1023
  lpFiltered = 0.9f * lpFiltered + 0.1f * rawVal;      // simple LPF
  baseline  = 0.999f * baseline + 0.001f * lpFiltered; // slow drift track

  float threshold = baseline + threshOffset;
  bool isAbove = (lpFiltered > threshold);
  unsigned long dt = now - lastBeatMs;

  if (isAbove && !aboveThreshold && dt > REFRACTORY_MS) {
    // Rising edge => beat detected
    if (lastBeatMs != 0) {
      ibiMs = dt;
      if (ibiMs >= 300 && ibiMs <= 2000) { // approx 30..200 BPM
        ibiWindow[ibiIdx] = ibiMs;
        ibiIdx = (ibiIdx + 1) % 5;

        unsigned long sum = 0; uint8_t n = 0;
        for (uint8_t i = 0; i < 5; i++) {
          if (ibiWindow[i] > 0) { sum += ibiWindow[i]; n++; }
        }
        if (n > 0) {
          float avgIbi = (float)sum / n;
          bpm = (int)(60000.0f / avgIbi + 0.5f);
          haveBpm = true;
        }
      }
    }
    lastBeatMs = now;
  }
  aboveThreshold = isAbove;
}

long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000UL); // 30ms timeout
  if (duration == 0) return -1;
  return (long)(duration * 0.034 / 2);
}

void makeCall() {
  Serial.println("Dialing...");
  sim800.print("ATD");
  sim800.print(phoneNumber);
  sim800.println(";");
  delay(20000);               // ring ~20s
  sim800.println("ATH");      // hang up
  Serial.println("Call finished");
}

void sendSMSAlert(float temp, float hum, long dist) {
  Serial.println("Sending SMS...");
  sim800.println("AT+CMGF=1");               // text mode
  delay(400);
  sim800.print("AT+CMGS=\""); sim800.print(phoneNumber); sim800.println("\"");
  delay(400);

  sim800.print("ALERT: Fall detected! Immediate help required.");
  sim800.print("\nBPM: ");
  if (haveBpm) sim800.print(bpm); else sim800.print("N/A");

  sim800.print("\nTemp: "); sim800.print(isnan(temp) ? 0 : temp, 1); sim800.print(" C");
  sim800.print("\nHum: ");  sim800.print(isnan(hum)  ? 0 : hum, 1);  sim800.print(" %");
  sim800.print("\nDist: "); sim800.print(dist); sim800.print(" cm");

  delay(300);
  sim800.write(26); // Ctrl+Z to send
  Serial.println("SMS sent");
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // MPU6050
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while (1);
  }
  Serial.println("MPU6050 OK");

  // DHT
  dht.begin();

  // Ultrasonic & Vibration
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(VIBRATION_PIN, INPUT);

  // Heart sensor
  pinMode(HEART_PIN, INPUT);

  // SIM800L
  sim800.begin(9600);
  delay(800);
  sim800.println("AT");
  delay(500);
  while (sim800.available()) Serial.write(sim800.read());
  Serial.println("SIM800L ready");

  Serial.println("Setup complete");
}

void loop() {
  // Keep heartbeat sampling frequent
  heartbeatSampleAndDetect();

  // Read MPU + Vibration frequently for fall detection
  static unsigned long lastFastMs = 0;
  if (millis() - lastFastMs >= 50) {
    lastFastMs = millis();

    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    long totalAcceleration = (long)ax * ax + (long)ay * ay + (long)az * az;
    bool mpuFall = (totalAcceleration < fallThreshold);

    bool vibrationDetected = (digitalRead(VIBRATION_PIN) == HIGH);

    if (mpuFall && vibrationDetected) {
      if (!fallDetected) {
        Serial.println("FALL DETECTED (MPU + Vibration)!");
        fallDetected = true;

        // Grab current readings for the SMS
        float temp = dht.readTemperature();
        float hum  = dht.readHumidity();
        long  dist = readDistanceCM();

        makeCall();
        sendSMSAlert(temp, hum, dist);
      }
    } else {
      fallDetected = false;
    }
  }

  // Slow status print (1 Hz)
  if (millis() - lastPrintMs >= 1000) {
    lastPrintMs = millis();

    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    long  dist = readDistanceCM();
    bool  vib  = (digitalRead(VIBRATION_PIN) == HIGH);

    Serial.print("Accel X="); Serial.print(ax);
    Serial.print(" Y="); Serial.print(ay);
    Serial.print(" Z="); Serial.print(az);

    Serial.print(" | T="); if (!isnan(temp)) Serial.print(temp,1); else Serial.print("NaN");
    Serial.print("C H=");  if (!isnan(hum))  Serial.print(hum,1);  else Serial.print("NaN");
    Serial.print("%");

    Serial.print(" | Dist="); Serial.print(dist); Serial.print(" cm");
    Serial.print(" | Vib=");  Serial.print(vib ? "YES" : "NO");

    Serial.print(" | HeartRaw="); Serial.print(rawVal);
    Serial.print(" | BPM=");     if (haveBpm) Serial.print(bpm); else Serial.print("--");

    Serial.println();
  }
}
