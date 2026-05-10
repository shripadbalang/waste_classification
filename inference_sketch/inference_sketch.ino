#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// ================= WIFI =================
const char* ssid = "Wifi";
const char* password = "11111111";
const char* serverURL = "http://10.68.232.230:5000/predict";

// ================= SERVO =================
Servo gate;
#define SERVO_PIN 18

int closeAngle = 90;
int openAngle  = 180;

// ================= BUZZER =================
int buzzerPin = 19;

// ================= STEPPER =================
int IN1 = 12;
int IN2 = 13;
int IN3 = 14;
int IN4 = 15;

int stepIndex = 0;
int currentPos = 0;

// ✅ YOUR CALIBRATED VALUE (120°)
const int steps120 = 950;

// ================= TIMER =================
unsigned long lastCheck = 0;
const int interval = 1000;

// ================= STEP SEQUENCE =================
int seq[8][4] = {
  {1,0,0,0},{1,1,0,0},{0,1,0,0},{0,1,1,0},
  {0,0,1,0},{0,0,1,1},{0,0,0,1},{1,0,0,1}
};

// ================= STEPPER FUNCTION =================
void stepMotor(int steps, bool dir) {

  unsigned long startTime = millis();

  for(int i=0; i<steps; i++) {

    if(dir) stepIndex = (stepIndex + 1) % 8;
    else    stepIndex = (stepIndex - 1 + 8) % 8;

    digitalWrite(IN1, seq[stepIndex][0]);
    digitalWrite(IN2, seq[stepIndex][1]);
    digitalWrite(IN3, seq[stepIndex][2]);
    digitalWrite(IN4, seq[stepIndex][3]);

    delay(2);  // faster stepping
  }

  // stop motor
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  Serial.print("Stepper time: ");
  Serial.println(millis() - startTime);
}

// ================= POSITION CONTROL =================
void moveTo(int target){

  int diff = target - currentPos;

  if(diff > 1) diff -= 3;
  if(diff < -1) diff += 3;

  bool dir = diff > 0;

  stepMotor(abs(diff) * steps120, dir);

  currentPos = target;
}

// ================= SERVO + BUZZER =================
void dropItem(){

  // 🔊 buzzer before action
  digitalWrite(buzzerPin, HIGH);
  delay(300);
  digitalWrite(buzzerPin, LOW);

  // 🚮 OPEN
  gate.write(openAngle);
  delay(2000);   // from your 2nd code

  // 🚮 CLOSE
  gate.write(closeAngle);
  delay(500);
}

// ================= CLASS HANDLER =================
void handleClass(String type){

  type.toLowerCase();

  if(type.indexOf("paper") >= 0 || type.indexOf("plastic_cap") >= 0) {
    Serial.println("Dry bin");
    moveTo(0);      // rotate first
    dropItem();     // then open
  }
  else if(type.indexOf("garlic") >= 0){
    Serial.println("Wet bin");
    moveTo(1);
    dropItem();
  }
  else if(type.indexOf("nailcutter") >= 0 || type.indexOf("screw") >= 0){
    Serial.println("Metal bin");
    moveTo(2);
    dropItem();
  }
  else{
    Serial.println("Unknown");
  }
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(buzzerPin, OUTPUT);

  gate.attach(SERVO_PIN);
  gate.write(closeAngle);

  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected 🚀");
}

// ================= LOOP =================
void loop() {

  if (millis() - lastCheck > interval) {

    if (WiFi.status() == WL_CONNECTED) {

      HTTPClient http;
      http.begin(serverURL);

      int code = http.GET();

      if (code > 0) {

        String payload = http.getString();
        Serial.println(payload);

        StaticJsonDocument<200> doc;
        deserializeJson(doc, payload);

        String result = doc["class"];

        handleClass(result);
      }

      http.end();
    }

    lastCheck = millis();
  }
}