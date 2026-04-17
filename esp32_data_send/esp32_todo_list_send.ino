#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

//  WIFI 
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

//  API 
const char* BASE = "http://192.168.0.151:5000/api";

String token = "";
String listId = "";

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
  String authHeader = "Bearer " + token;

  Serial.println("TOKEN: " + token);

  // 2. CREATE LIST
  String listBody = "{\"name\":\"Test List\"}";

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
    String taskBody = "{\"text\":\"Task " + String(i + 1) + "\"}";

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

void loop() {
  // nothing
}
