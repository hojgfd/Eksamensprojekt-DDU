#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "bruv";
const char* password = "abekat38";

// API endpoint
const char* serverUrl = "https://shekib.pythonanywhere.com/api/heartrate";

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    // Create JSON
    StaticJsonDocument<64> doc;
    int hr = random(60, 81);  // 60–80 like your Python code
    doc["hr"] = hr;

    String jsonString;
    serializeJson(doc, jsonString);

    // Send POST request
    int httpResponseCode = http.POST(jsonString);

    Serial.print("Sent HR: ");
    Serial.print(hr);
    Serial.print(" | Response: ");
    Serial.println(httpResponseCode);

    http.end();
  } else {
    Serial.println("WiFi disconnected");
  }

  delay(1000); // 1 second
}