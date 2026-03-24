#include "HallSensorRPM.h"

// ===== Sensor config =====
const uint8_t hallPin = 2;
const uint8_t magnetsPerRev = 4;

HallSensorRPM sensor(hallPin, magnetsPerRev);

// ===== Motor driver pins (XY160D) =====
const int IN1 = 5;
const int IN2 = 4;
const int ENA = 9;   // IMPORTANT: use 9 or 10 (not 6)

// ===== Setup =====
void setup() {
  Serial.begin(9600);

  sensor.begin();

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  Motor1_Brake();
  delay(100);              // allow system to settle
  Motor1_Forward(255);     // full speed
}

// ===== Main loop =====
void loop() {
  float rpm = sensor.getRPM();

  Serial.print("RPM: ");
  Serial.println(rpm);

  delay(200);
}

// ===== Motor control =====
void Motor1_Forward(int Speed) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, Speed);
}

void Motor1_Backward(int Speed) {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  analogWrite(ENA, Speed);
}

void Motor1_Brake() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}