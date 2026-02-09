/*
 * Water Tank Monitor - WiFi Version
 * 
 * Hardware:
 * - Arduino UNO R4 WiFi
 * - 0.5-5V Pressure Sensor on pin A0
 * 
 * Sends tank level data directly to Raspberry Pi via HTTP POST
 * No LoRaWAN required!
 */

#include <WiFiS3.h>

// WiFi credentials
const char* ssid = "IOT";
const char* password = "GU23enY5!";

// Raspberry Pi server
const char* serverIP = "192.168.55.192";
const int serverPort = 5001;

// Sensor Configuration
const int PRESSURE_SENSOR_PIN = A0;
const float SENSOR_MIN_VOLTAGE = 0.5;
const float SENSOR_MAX_VOLTAGE = 1.44;  // Calibrated to your sensor
const float ADC_REFERENCE = 5.0;
const int ADC_RESOLUTION = 1023;

// Timing
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 60000; // 1 minute
// const unsigned long SEND_INTERVAL = 600000; // 10 minutes for production

// WiFi client
WiFiClient client;

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  while (!Serial && millis() < 5000);
  
  Serial.println(F("================================="));
  Serial.println(F("Water Tank Monitor - WiFi Version"));
  Serial.println(F("================================="));
  Serial.println();
  
  // Initialize sensor pin
  pinMode(PRESSURE_SENSOR_PIN, INPUT);
  
  // Connect to WiFi
  connectToWiFi();
  
  Serial.println(F("Setup complete. Starting monitoring..."));
  Serial.println();
}

void loop() {
  unsigned long currentTime = millis();
  
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi disconnected! Reconnecting..."));
    connectToWiFi();
  }
  
  // Send data at regular intervals
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    float tankLevel = readPressureSensor();
    sendDataToServer(tankLevel);
    lastSendTime = currentTime;
  }
  
  delay(100);
}

void connectToWiFi() {
  Serial.print(F("Connecting to WiFi: "));
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.print(F("."));
    attempts++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("*** WiFi Connected! ***"));
    Serial.print(F("IP Address: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("Signal Strength (RSSI): "));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm"));
    Serial.println();
  } else {
    Serial.println(F("*** WiFi Connection Failed! ***"));
    Serial.println(F("Check SSID and password."));
  }
}

float readPressureSensor() {
  // Read analog value (average of 10 readings for stability)
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PRESSURE_SENSOR_PIN);
    delay(10);
  }
  int rawValue = sum / 10;
  
  // Convert to voltage
  float voltage = (rawValue / (float)ADC_RESOLUTION) * ADC_REFERENCE;
  
  // Convert voltage to percentage (0.5V = 0%, 1.44V = 100%)
  float percentage = ((voltage - SENSOR_MIN_VOLTAGE) / 
                     (SENSOR_MAX_VOLTAGE - SENSOR_MIN_VOLTAGE)) * 100.0;
  
  // Constrain to 0-100%
  percentage = constrain(percentage, 0, 100);
  
  Serial.println(F("--- Sensor Reading ---"));
  Serial.print(F("Raw ADC: "));
  Serial.println(rawValue);
  Serial.print(F("Voltage: "));
  Serial.print(voltage, 2);
  Serial.println(F(" V"));
  Serial.print(F("Tank Level: "));
  Serial.print(percentage, 1);
  Serial.println(F(" %"));
  
  return percentage;
}

void sendDataToServer(float tankLevel) {
  Serial.println(F("================================="));
  Serial.println(F("Sending data to Raspberry Pi..."));
  
  if (client.connect(serverIP, serverPort)) {
    Serial.print(F("Connected to server: "));
    Serial.print(serverIP);
    Serial.print(F(":"));
    Serial.println(serverPort);
    
    // Get WiFi signal strength and voltage
    int rssi = WiFi.RSSI();
    int rawValue = analogRead(PRESSURE_SENSOR_PIN);
    float voltage = (rawValue / (float)ADC_RESOLUTION) * ADC_REFERENCE;
    
    // Create JSON payload
    String jsonData = "{";
    jsonData += "\"tank_level\":";
    jsonData += String(tankLevel, 2);
    jsonData += ",\"voltage\":";
    jsonData += String(voltage, 2);
    jsonData += ",\"rssi\":";
    jsonData += String(rssi);
    jsonData += ",\"timestamp\":";
    jsonData += String(millis() / 1000);
    jsonData += "}";
    
    // Send HTTP POST request
    client.println("POST /api/tank-data HTTP/1.1");
    client.print("Host: ");
    client.println(serverIP);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonData.length());
    client.println("Connection: close");
    client.println();
    client.println(jsonData);
    
    Serial.print(F("Sent: "));
    Serial.println(jsonData);
    
    // Wait for response
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(F(">>> Client Timeout!"));
        client.stop();
        return;
      }
    }
    
    // Read response
    Serial.println(F("Server response:"));
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    Serial.println();
    
    client.stop();
    Serial.println(F("*** Data sent successfully! ***"));
    
  } else {
    Serial.println(F("*** Connection to server failed! ***"));
    Serial.println(F("Check server IP and port."));
  }
  
  Serial.println(F("================================="));
  Serial.println();
}
