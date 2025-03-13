#include <Arduino.h>
#include <ArduinoJson.h>

void setup() {
  Serial.begin(9600);
  Serial.println("Starting");
  pinMode(15, OUTPUT);
  pinMode(0, INPUT_PULLUP);
  randomSeed(analogRead(0));
}

void loop() {
  // Generate random data
  int tmp_avg = random(20, 40);
  int prs = random(900, 1100);
  int alt = random(100, 5000);
  int ax = random(-10, 10);
  int ay = random(-10, 10);
  int az = random(-10, 10);
  int gx = random(-250, 250);
  int gy = random(-250, 250);
  int gz = random(-250, 250);
  float lat = random(-90000000, 90000000) / 1000000.0;
  float lng = random(-180000000, 180000000) / 1000000.0;
  int gps_alt = random(100, 5000);
  int sat = random(3, 20);
  int spd = random(0, 150);
  int dir = random(0, 360);
  int mst = millis();
  StaticJsonDocument<512> jsonDoc;
  if (Serial.available()) {
    delay(10);
    String input = Serial.readStringUntil('\n');
    StaticJsonDocument<512> inputJson;
    DeserializationError error = deserializeJson(inputJson, input);
    if (!error) {
      if (inputJson.containsKey("MSG")) {
        Serial.print("Received MSG: ");
        Serial.println(inputJson["MSG"].as<String>());
        jsonDoc["MSG"] = inputJson["MSG"].as<String>();
      }
    }
  }

  // Create JSON object
  jsonDoc["TMP_AVG"] = tmp_avg;
  jsonDoc["PRS"] = prs;
  jsonDoc["ALT"] = alt;
  jsonDoc["AX"] = ax;
  jsonDoc["AY"] = ay;
  jsonDoc["AZ"] = az;
  jsonDoc["GX"] = gx;
  jsonDoc["GY"] = gy;
  jsonDoc["GZ"] = gz;
  jsonDoc["LAT"] = lat;
  jsonDoc["LNG"] = lng;
  jsonDoc["GPS_ALT"] = gps_alt;
  jsonDoc["SAT"] = sat;
  jsonDoc["SPD"] = spd;
  jsonDoc["DIR"] = dir;
  jsonDoc["MST"] = mst;

  // Serialize and send JSON
  serializeJson(jsonDoc, Serial);
  Serial.println();
  delay(250);
  while (!digitalRead(0));
}
