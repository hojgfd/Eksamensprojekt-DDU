#include <M5Stack.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>

// ---------------- WIFI ----------------
const char* ssid = "bruv";
const char* password = "abekat38";
String googleScriptURL = "https://script.google.com/macros/s/AKfycbxYxv8DwFWoNnR4I3y2irrx5B-CCwvroIhKpIhQcgxCchSv44cJkK0CWBHdNYAB2FMx_g/exec";

// ---------------- FOKUS DATA ----------------
unsigned long focusDuration = 25 * 60 * 1000;
unsigned long startTime = 0;
bool sessionActive = false;

int distractionCount = 0;
int focusScore = 100;
int points = 0;
int level = 1;

int timeOptions[5] = {1, 5, 15, 25, 45};
int selectedIndex = 1;

// ---------------- SETUP ----------------
void setup() {
  M5.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.fillScreen(BLACK);

  connectWiFi();
  showMenu();
}

// ---------------- LOOP ----------------
void loop() {
  M5.update();

  if (M5.BtnA.wasPressed() && !sessionActive) {
    selectedIndex = (selectedIndex + 1) % 3;
    focusDuration = timeOptions[selectedIndex] * 60 * 1000;
    showMenu();
  }

  if (M5.BtnB.wasPressed()) {
    if (!sessionActive) startSession();
    else endSession(false);
  }

  if (M5.BtnC.wasPressed() && sessionActive) {
    distractionCount++;
    showDistractionMessage();
  }

  if (sessionActive) updateTimer();
}

// ---------------- WIFI ----------------
void connectWiFi() {
  WiFi.begin(ssid, password);

  M5.Lcd.setCursor(20, 200);
  M5.Lcd.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }

  M5.Lcd.println(" Connected!");
}

// ---------------- MENU ----------------
void showMenu() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(60, 60);
  M5.Lcd.printf("Focus: %d min", timeOptions[selectedIndex]);
  M5.Lcd.setCursor(40, 120);
  M5.Lcd.print("A: Change  B: Start");
}

// ---------------- START ----------------
void startSession() {
  sessionActive = true;
  distractionCount = 0;
  startTime = millis();
  M5.Speaker.tone(1000, 200);
}

// ---------------- TIMER ----------------
void updateTimer() {
  unsigned long elapsed = millis() - startTime;

  if (elapsed >= focusDuration) {
    endSession(true);
    return;
  }

  unsigned long remaining = focusDuration - elapsed;
  int seconds = (remaining / 1000) % 60;
  int minutes = (remaining / 1000) / 60;

  float progress = (float)elapsed / focusDuration;

  drawFocusUI(minutes, seconds, progress);
}

// ---------------- PROFESSIONEL UI ----------------
void drawFocusUI(int minutes, int seconds, float progress) {

  M5.Lcd.fillScreen(BLACK);

  int centerX = 160;
  int centerY = 120;
  int radius = 80;

  M5.Lcd.drawCircle(centerX, centerY, radius, DARKGREY);

  int angle = progress * 360;

  for (int i = 0; i < angle; i++) {
    float rad = i * DEG_TO_RAD;
    int x = centerX + cos(rad) * radius;
    int y = centerY + sin(rad) * radius;
    M5.Lcd.drawPixel(x, y, GREEN);
  }

  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(100, 100);
  M5.Lcd.printf("%02d:%02d", minutes, seconds);

  delay(200);
}

void endSession(bool completed) {

  sessionActive = false;

  unsigned long elapsed = millis() - startTime;

  int minutes = (elapsed / 1000) / 60;
  int seconds = (elapsed / 1000) % 60;

  char durationStr[6];
  sprintf(durationStr, "%02d:%02d", minutes, seconds);

  focusScore = 100 - (10 * distractionCount);
  if (focusScore < 0) focusScore = 0;

  if (completed) {
    points += focusScore;
    if (points >= level * 200) level++;
    M5.Speaker.tone(1500, 500);
  }

  sendToGoogleSheets(
    durationStr,
    distractionCount,
    focusScore,
    points
  );

  showResult(completed);
}

// ---------------- RESULT ----------------
void showResult(bool completed) {

  M5.Lcd.fillScreen(BLACK);

  uint16_t color;
  if (focusScore > 70) color = GREEN;
  else if (focusScore > 40) color = YELLOW;
  else color = RED;

  M5.Lcd.setTextColor(color);
  M5.Lcd.setCursor(40, 40);

  if (completed) M5.Lcd.print("Session Complete!");
  else M5.Lcd.print("Session Stopped");

  M5.Lcd.setTextColor(WHITE);

  M5.Lcd.setCursor(40, 80);
  M5.Lcd.printf("Distractions: %d", distractionCount);

  M5.Lcd.setCursor(40, 110);
  M5.Lcd.printf("Score: %d", focusScore);

  M5.Lcd.setCursor(40, 140);
  M5.Lcd.printf("Level: %d", level);

  delay(5000);
  showMenu();
}

// ---------------- DISTRACTION ----------------
void showDistractionMessage() {
  M5.Lcd.fillScreen(RED);
  M5.Lcd.setCursor(40, 100);
  M5.Lcd.print("Distraction Logged!");
  delay(800);
}

// ---------------- GOOGLE LOGGING ----------------
void sendToGoogleSheets(String duration, int distractions, int score, int pts, int lvl) {

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    String url = googleScriptURL;
    url += "?duration=" + duration;
    url += "&distractions=" + String(distractions);
    url += "&score=" + String(score);
    url += "&points=" + String(pts);
    url += "&level=" + String(lvl);

    Serial.println(url);

    http.begin(url);
    http.GET();
    http.end();
  }
}