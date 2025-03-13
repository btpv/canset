#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <Adafruit_AHTX0.h>

// Sensor Objects
Adafruit_BMP280 bmp;
Adafruit_AHTX0 aht;
Adafruit_MPU6050 mpu1;
Adafruit_MPU6050 mpu2;


// GPS pins
#define GPS_RXPin 7
#define GPS_TXPin 6

// Create SoftwareSerial instance for GPS communication
SoftwareSerial gpsSerial(GPS_RXPin, GPS_TXPin);
TinyGPSPlus gps;  // TinyGPS++ object for parsing GPS data

// Sensor availability flags
bool bmp280Available = false;
bool aht20Available = false;
bool mpu1Available = false;
bool mpu2Available = false;
bool gpsAvailable = false;

// Global Variables
float initialAltitude = 0;  // To store the initial altitude

void scanI2C() {
  Serial.println("\nScanning I2C devices...");
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found device at 0x");
      Serial.println(address, HEX);
      if (address == 0x38) aht20Available = true;  // Moisture sensor placeholder
      if (address == 0x68) mpu1Available = true;
      if (address == 0x69) mpu2Available = true;
      if (address == 0x77) bmp280Available = true;
      delay(10);
    }
  }
  Serial.println("I2C scan complete.\n");
}

void setup() {
  Wire.begin();
  Serial.begin(115200);
  Serial.println("\nStarting system...");

  scanI2C();  // Run I2C scanner at startup

  // Initialize BMP280
  if (bmp280Available && bmp.begin(0x77)) {
    Serial.println("BMP280 sensor found.");
    initialAltitude = bmp.readAltitude(1013.25);
  } else {
    Serial.println("Warning: BMP280 sensor NOT found.");
  }
  if (aht20Available && aht.begin()) {
    Serial.println("AHT20 sensor found.");
  } else {
    Serial.println("Warning: AHT20 sensor NOT found.");
  }
  // Initialize MPU6050 #1
  if (mpu1Available && mpu1.begin(0x68)) {
    Serial.println("MPU6050 #1 found.");
  } else {
    Serial.println("Warning: MPU6050 #1 NOT found.");
  }

  // Initialize MPU6050 #2
  if (mpu2Available && mpu2.begin(0x69)) {
    Serial.println("MPU6050 #2 found.");
  } else {
    Serial.println("Warning: MPU6050 #2 NOT found.");
  }
  // Initialize MPU6050 #2

// Initialize GPS
  gpsSerial.begin(9600);
  gpsAvailable = true;
  Serial.println("GPS initialized...");

  // Start Serial1 for APC220
  Serial1.begin(9600);
  // Serial1.begin(115200);
  Serial.println("Broadcasting every second to APC220...");
}

void loop() {
  unsigned long start = millis();
  // GPS Data Handling
  Serial.print(gpsSerial.available());
  Serial.print("\t");
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
  // GPS Data
  float gpsLatitude = gps.location.isValid() ? gps.location.lat() : 0.0;
  float gpsLongitude = gps.location.isValid() ? gps.location.lng() : 0.0;
  float gpsAltitude = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
  int gpsSatellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
  float gpsSpeed = gps.speed.isValid() ? gps.speed.kmph() : 0.0;
  float gpsDirection = gps.course.isValid() ? gps.course.deg() : 0.0;

  // Sensor data with fallbacks
  float bmpTemperature = bmp280Available ? bmp.readTemperature() : -99.99;
  float pressure = bmp280Available ? bmp.readPressure() / 100.0F : -1.0;
  float altitude = bmp280Available ? bmp.readAltitude(1013.25) - initialAltitude : -1.0;

  sensors_event_t a1, g1, temp1, a2, g2, temp2, humidity, temp;
  if (aht20Available) {
    aht.getEvent(&humidity,&temp);
  }
  if (mpu1Available) {
    mpu1.getEvent(&a1, &g1, &temp1);
  }
  if (mpu2Available) {
    mpu2.getEvent(&a2, &g2, &temp2);
  }

  // Average acceleration and gyro values
  float avgAccelX = (a1.acceleration.x + a2.acceleration.x) / 2.0;
  float avgAccelY = (a1.acceleration.y + a2.acceleration.y) / 2.0;
  float avgAccelZ = (a1.acceleration.z + a2.acceleration.z) / 2.0;
  float avgGyroX = (g1.gyro.x + g2.gyro.x) / 2.0;
  float avgGyroY = (g1.gyro.y + g2.gyro.y) / 2.0;
  float avgGyroZ = (g1.gyro.z + g2.gyro.z) / 2.0;
  JsonDocument data;
  // Create telemetry string including GPS and sensor data
  data["TMP_BMP"] = bmpTemperature;
  data["TMP_AHT"] = temp.temperature;
  data["TMP_MPU1"] = temp1.temperature-18;
  data["TMP_MPU2"] = temp2.temperature-18;
  data["PRS"] = pressure;
  data["ALT"] = altitude;
  data["AX"] = avgAccelX;
  data["AY"] = avgAccelY;
  data["AZ"] = avgAccelZ;
  data["GX"] = avgGyroX;
  data["GY"] = avgGyroY;
  data["GZ"] = avgGyroZ;
  data["LAT"] = gpsLatitude;
  data["LNG"] = gpsLongitude;
  data["GPS_ALT"] = gpsAltitude;
  data["SAT"] = gpsSatellites;
  data["SPD"] = gpsSpeed;
  data["DIR"] = gpsDirection;
  data["MST"] = humidity.relative_humidity;
  data["TS"] = millis();
  while (Serial1.available()) {
    String input = Serial1.readStringUntil('\n');
    StaticJsonDocument<128> inputJson;
    DeserializationError error = deserializeJson(inputJson, input);
    if (!error) {
      if (inputJson.containsKey("MSG")) {
        data["MSG"] = inputJson["MSG"].as<String>();
      }
    } else {
      Serial.print("Invalid JSON: \"");
      Serial.print(input);
      Serial.print("\"error: ");
      Serial.println(error.c_str());
    }
  }
  char buffer[512];
  size_t len = serializeJson(data, buffer, sizeof(buffer));
  Serial1.write(buffer, len);
  Serial1.println();
  Serial.print(millis() - start);
  Serial.print(" ");
  while (abs(millis() - start) < 350) {
    if (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
    }
  }
  Serial.println(millis() - start);
}
