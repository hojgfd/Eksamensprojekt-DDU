#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>

// ===== WIFI =====
const char* ssid = "bruv";
const char* password = "abekat38";

// ===== API =====
const char* BASE = "http://192.168.89.188:5000/api";
String token = "";
String authHeader = "";

String commandToWrite =  R"cpp(exec("for line in open('hr_data.jsonl'): print(line, end='')"))cpp";

// ===== Nordic UART Service UUIDs =====
// NUS service + RX/TX characteristics
static NimBLEUUID NUS_SERVICE_UUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static NimBLEUUID NUS_RX_UUID     ("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"); // write to watch
static NimBLEUUID NUS_TX_UUID     ("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"); // notify from watch

static NimBLEAdvertisedDevice* advDevice = nullptr;
static NimBLEClient* client = nullptr;
static NimBLERemoteCharacteristic* rxChar = nullptr;
static NimBLERemoteCharacteristic* txChar = nullptr;

static bool doConnect = false;
static bool connected = false;
static bool firstFetch = true;
static String replBuffer = "";

// Hent filen hvert 15. sekund
static unsigned long lastFetchMs = 0;
static const unsigned long FETCH_INTERVAL_MS = 60000;

// Hold styr på hvor langt vi er nået i filen
static long lastProcessedLine = 0;

// ===== HTTP helpers =====
String httpPOST(String url, String jsonBody, String auth = "") {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  if (auth.length() > 0) {
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

void sendHeartRate(int hr) {
  String body = "{\"hr\":" + String(hr) + "}";

  httpPOST(
    String(BASE) + "/heartrate",
    body,
    authHeader
  );
}


bool loginApi() {
  String loginBody = "{\"username\":\"oskib\",\"password\":\"oskib\"}";
  String loginResponse = httpPOST(String(BASE) + "/login", loginBody);

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, loginResponse);
  if (err) {
    Serial.print("Login JSON parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  if (!doc["token"].is<String>()) {
    Serial.println("Login response missing token");
    return false;
  }

  token = doc["token"].as<String>();
  authHeader = "Bearer " + token;

  Serial.println("TOKEN OK");
  return true;
}

// ===== Parse JSONL from watch =====
// Forventer linjer som:
// {"hr": 72}
void parseJsonlAndSend(const String& chunk) {
  int start = 0;
  long lineNo = 0;

  while (start < (int)chunk.length()) {
    int end = chunk.indexOf('\n', start);
    if (end < 0) end = chunk.length();

    String line = chunk.substring(start, end);
    line.trim();

    if (line.length() > 0) {

      // FILTER OUT REPL / GARBAGE
      if (line.startsWith(">>>") ||
          line.indexOf("wasp.system.run") >= 0 ||
          line.indexOf("Watch already running") >= 0 ||
          line.indexOf("===END===") >= 0) {
        start = end + 1;
        continue;
      }

      // ONLY PROCESS JSON LINES
      if (!line.startsWith("{") || !line.endsWith("}")) {
        start = end + 1;
        continue;
      }

      lineNo++;

      // Skip already processed lines
      if (lineNo <= lastProcessedLine) {
        start = end + 1;
        continue;
      }

      // Parse JSON
      StaticJsonDocument<128> doc;
      DeserializationError err = deserializeJson(doc, line);

      if (!err && doc["hr"].is<int>()) {
        int hr = doc["hr"].as<int>();

        Serial.printf("Parsed HR: %d\n", hr);

        sendHeartRate(hr);

        lastProcessedLine = lineNo;
      } else {
        Serial.print("Skipping malformed JSON: ");
        Serial.println(line);
      }
    }

    start = end + 1;
  }
}

// ===== BLE notify callback =====
void notifyCallback(
    NimBLERemoteCharacteristic* pRemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify) {
  String s;
  s.reserve(length);
  for (size_t i = 0; i < length; i++) {
    s += (char)pData[i];
  }

  Serial.print(s);
  replBuffer += s;
}

// ===== BLE callbacks =====
class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) override {
    Serial.println("BLE connected");
  }

  void onDisconnect(NimBLEClient* pClient, int reason) override {
    Serial.printf("BLE disconnected, reason=%d\n", reason);
    connected = false;
    doConnect = false;
    rxChar = nullptr;
    txChar = nullptr;
    NimBLEDevice::getScan()->start(0, false, true);
  }
};

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    if (advertisedDevice->isAdvertisingService(NUS_SERVICE_UUID)) {
      Serial.printf("Found NUS device: %s\n", advertisedDevice->toString().c_str());
      NimBLEDevice::getScan()->stop();
      advDevice = const_cast<NimBLEAdvertisedDevice*>(advertisedDevice);
      doConnect = true;
    }
  }
};

// ===== BLE connect =====
bool connectToWatch() {
  if (!advDevice) return false;

  if (!client) {
    client = NimBLEDevice::createClient();
    client->setClientCallbacks(new ClientCallbacks(), false);
  }

  if (!client->connect(advDevice)) {
    Serial.println("Connect failed");
    return false;
  }

  NimBLERemoteService* svc = client->getService(NUS_SERVICE_UUID);
  if (!svc) {
    Serial.println("NUS service not found");
    client->disconnect();
    return false;
  }

  rxChar = svc->getCharacteristic(NUS_RX_UUID);
  txChar = svc->getCharacteristic(NUS_TX_UUID);

  if (!rxChar || !txChar) {
    Serial.println("NUS RX/TX characteristic missing");
    client->disconnect();
    return false;
  }

  if (txChar->canNotify()) {
    if (!txChar->subscribe(true, notifyCallback)) {
      Serial.println("Subscribe failed");
      client->disconnect();
      return false;
    }
  }

  connected = true;
  Serial.println("Ready for REPL over NUS");
  return true;
}

// ===== Write to REPL in small chunks =====
void writeRepl(const String& cmd, uint16_t delayMs = 300) {
  if (!connected || !rxChar) return;

  const size_t CHUNK = 20;
  for (size_t i = 0; i < cmd.length(); i += CHUNK) {
    String part = cmd.substring(i, i + CHUNK);
    rxChar->writeValue((uint8_t*)part.c_str(), part.length(), false);
    delay(30);
  }
  delay(delayMs);
}

// ===== Fetch file from watch =====
void fetchHeartFile() {
  if (!connected) return;

  replBuffer = "";

  // 1. STOP watch + enter REPL
  writeRepl(String((char)0x03), 800);

  // læs fil linje for linje i én one-liner
  writeRepl(commandToWrite, 5000);
  writeRepl("\r\n", 500);  // <-- THIS triggers execution
  writeRepl("print('===END===')\r\n", 800);

  // 3. Wait until full response arrives
  if (replBuffer.indexOf("===END===") == -1) {
    Serial.println("ERROR: Did not receive full file!");
    Serial.println(replBuffer);
    return;
  }

  // 4. DEBUG print
  Serial.println("RAW BUFFER:");
  Serial.println(replBuffer);

  // 5. Parse ONLY after full data received
  parseJsonlAndSend(replBuffer);

  // 6. Restart watch AFTER parsing
  //writeRepl("wasp.system.run()\r\n", 800);
} 

// ===== BLE setup =====
void setupBle() {
  NimBLEDevice::init("");
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(new ScanCallbacks(), false);
  scan->setInterval(45);
  scan->setWindow(15);
  scan->setActiveScan(true);
  scan->start(0, false, true);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Login
  if (!loginApi()) {
    Serial.println("API login failed");
  }

  // BLE
  setupBle();
}

void loop() {
  if (doConnect && !connected) {
    doConnect = false;
    connectToWatch();
  }

  if (connected && WiFi.status() == WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastFetchMs > FETCH_INTERVAL_MS || firstFetch == true) {
      lastFetchMs = now;
      Serial.println("\n=== Fetching hr_data.jsonl from watch ===");
      fetchHeartFile();
      Serial.println("\n=== Done ===");
      firstFetch = false;
    }
  }

  delay(50);
}
