#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// ================= WIFI =================
const char* ssid = "Wifi";
const char* password = "11111111";
const char* serverURL = "http://10.68.232.230:5000/result";

// ================= SERVO =================
Servo gate;
#define SERVO_PIN 13

int restAngle = 80;     // pause position
int openAngle = 130;    // fully open (adjust if needed)

// ================= STEPPER =================
int IN1 = 12;
int IN2 = 13;
int IN3 = 14;
int IN4 = 15;

int stepIndex = 0;
int currentPos = 0;

const int steps120 = 682;

int seq[8][4] = {
  {1,0,0,0},{1,1,0,0},{0,1,0,0},{0,1,1,0},
  {0,0,1,0},{0,0,1,1},{0,0,0,1},{1,0,0,1}
};

// ================= TIMER =================
unsigned long lastCheck = 0;
const int interval = 5000;

// ================= FUNCTIONS =================
void stepMotor(int steps, bool dir) {
  for(int i=0;i<steps;i++){
    if(dir) stepIndex = (stepIndex+1)%8;
    else stepIndex = (stepIndex-1+8)%8;

    digitalWrite(IN1, seq[stepIndex][0]);
    digitalWrite(IN2, seq[stepIndex][1]);
    digitalWrite(IN3, seq[stepIndex][2]);
    digitalWrite(IN4, seq[stepIndex][3]);

    delay(3);
  }

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void moveTo(int target){
  int diff = target - currentPos;

  if(diff > 1) diff -= 3;
  if(diff < -1) diff += 3;

  bool dir = diff > 0;

  stepMotor(abs(diff)*steps120, dir);
  currentPos = target;
}

// ================= SERVO ACTION =================
void dropItem(){
  Serial.println("Opening gate...");
  gate.write(openAngle);   // open fully
  delay(1500);

  Serial.println("Returning to rest...");
  gate.write(restAngle);   // back to 80°
  delay(500);
}

// ================= CLASS HANDLER =================
void handleClass(String type){

  type.toLowerCase();

  if(type.indexOf("nail") >= 0 || type.indexOf("screw") >= 0){
    Serial.println("Bin 0 → nail/screw");
    moveTo(0);
    dropItem();
  }
  else if(type.indexOf("paper") >= 0 || type.indexOf("bottle") >= 0){
    Serial.println("Bin 1 → paper/bottle");
    moveTo(1);
    dropItem();
  }
  else if(type.indexOf("garlic") >= 0){
    Serial.println("Bin 2 → garlic");
    moveTo(2);
    dropItem();
  }
  else{
    Serial.println("Unknown item");
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  gate.attach(SERVO_PIN);
  gate.write(restAngle);   // start at 80°

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
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
      else {
        Serial.println("HTTP Error");
      }

      http.end();
    }

    lastCheck = millis();
  }
}