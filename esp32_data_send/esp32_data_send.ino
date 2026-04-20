#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

//  WIFI 
const char* ssid = "bruv";
const char* password = "abekat38";

//  API 
const char* BASE = "http://192.168.89.188:5000/api";

String token = "";
String listId = "";
String authHeader = "";

String httpPOST(String url, String jsonBody, String auth = "") {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  if (auth != "") {
    http.addHeader("Authorization", auth);
  }

  int code = http.POST(jsonBody);
  String payload = http.getString();

  Serial.println("POST " + url);
  Serial.println("Code: " + String(code));
  Serial.println("Response: " + payload);

  http.end();
  return payload;
}

String httpGET(String url, String auth) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Authorization", auth);

  int code = http.GET();
  String payload = http.getString();

  Serial.println("GET " + url);
  Serial.println("Code: " + String(code));
  Serial.println("Response: " + payload);

  http.end();
  return payload;
}

void sendHeartRate(int hr) {
  String body = "{\"hr\":" + String(hr) + "}";

  httpPOST(
    String(BASE) + "/heartrate",
    body,
    authHeader
  );
}

void sendFocusSession(int minutes, int distractions) {
  String date = "2026-04-20"; // or generate dynamically later

  String body = "{";
  body += "\"minutes\":" + String(minutes) + ",";
  body += "\"distractions\":" + String(distractions) + ",";
  body += "\"date\":\"" + date + "\"";
  body += "}";

  httpPOST(
    String(BASE) + "/focus-session",
    body,
    authHeader
  );
}

void setup() {
  Serial.begin(115200);

  // ===== WIFI CONNECT =====
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");

  // 1. LOGIN
  String loginBody = "{\"username\":\"oskib\",\"password\":\"oskib\"}";

  String loginResponse = httpPOST(String(BASE) + "/login", loginBody);

  StaticJsonDocument<512> doc;
  deserializeJson(doc, loginResponse);

  token = doc["token"].as<String>();
  authHeader = "Bearer " + token;

  Serial.println("TOKEN: " + token);


  // 2. CREATE LIST
  String listBody = "{\"name\":\"Test List2\"}";

  String listResponse = httpPOST(
    String(BASE) + "/todolists",
    listBody,
    authHeader
  );

  StaticJsonDocument<512> doc2;
  deserializeJson(doc2, listResponse);

  listId = doc2["id"].as<String>();

  Serial.println("LIST ID: " + listId);

  // 3. ADD TASKS
  for (int i = 0; i < 3; i++) {
    String taskBody = "{\"text\":\"Tazk " + String(i + 1) + "\"}";

    httpPOST(
      String(BASE) + "/todolists/" + listId + "/tasks",
      taskBody,
      authHeader
    );
  }

  // 4. FETCH TASKS
  String tasks = httpGET(
    String(BASE) + "/todolists/" + listId + "/tasks",
    authHeader
  );

  Serial.println("FINAL TASK LIST:");
  Serial.println(tasks);
}

unsigned long lastHR = 0;
unsigned long lastFocus = 0;

int currentHR = 75;

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    unsigned long now = millis();

    // ===== HEART RATE every 2 seconds =====
    if (now - lastHR > 2000) {
      lastHR = now;

      // simulate HR variation
      currentHR += random(-3, 4);
      currentHR = constrain(currentHR, 55, 140);

      Serial.println("Sending HR: " + String(currentHR));
      sendHeartRate(currentHR);
    }

    // ===== FOCUS SESSION every 60 seconds =====
    if (now - lastFocus > 60000) {
      lastFocus = now;

      int minutes = random(20, 90);
      int distractions = random(0, 5);

      Serial.println("Sending Focus Session:");
      Serial.println("Minutes: " + String(minutes));
      Serial.println("Distractions: " + String(distractions));

      sendFocusSession(minutes, distractions);
    }
  }
}
