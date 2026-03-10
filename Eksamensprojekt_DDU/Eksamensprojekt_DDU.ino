#include <M5Stack.h>

unsigned long focusDuration = 25 * 60 * 1000; // default 25 min
unsigned long startTime = 0;
bool sessionActive = false;
bool flowMode = false;

int distractionCount = 0;
int focusScore = 100;
int points = 0;
int level = 1;

int timeOptions[3] = {15, 25, 45};
int selectedIndex = 1; // default 25 min

// ---------- SETUP ----------
void setup() {
  M5.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.fillScreen(BLACK);
  showMenu();
}

// ---------- LOOP ----------
void loop() {
  M5.update();

  // Select focus time
  if (M5.BtnA.wasPressed() && !sessionActive) {
    selectedIndex = (selectedIndex + 1) % 3;
    focusDuration = timeOptions[selectedIndex] * 60 * 1000;
    showMenu();
  }

  // Start / Stop
  if (M5.BtnB.wasPressed()) {
    if (!sessionActive) {
      startSession();
    } else {
      endSession(false);
    }
  }

  // Log distraction
  if (M5.BtnC.wasPressed() && sessionActive) {
    distractionCount++;
    showDistractionMessage();
  }

  if (sessionActive) {
    updateTimer();
  }
}

// ---------- MENU ----------
void showMenu() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(40, 40);
  M5.Lcd.printf("Focus Time:\n%d min", timeOptions[selectedIndex]);
  M5.Lcd.setCursor(20, 120);
  M5.Lcd.print("A: Change  B: Start");
}

// ---------- START ----------
void startSession() {
  sessionActive = true;
  distractionCount = 0;
  startTime = millis();
  flowMode = true;

  M5.Speaker.tone(1000, 200);
}

// ---------- TIMER ----------
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

  if (flowMode) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(220, 10);
    M5.Lcd.printf("%02d:%02d", minutes, seconds);
  } else {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(90, 40);
    M5.Lcd.printf("%02d:%02d", minutes, seconds);

    // Progress bar
    M5.Lcd.drawRect(40, 120, 240, 20, WHITE);
    M5.Lcd.fillRect(40, 120, 240 * progress, 20, GREEN);
  }

  delay(200);
}

// ---------- END ----------
void endSession(bool completed) {
  sessionActive = false;

  focusScore = 100 - (10 * distractionCount);
  if (focusScore < 0) focusScore = 0;

  if (completed) {
    points += focusScore;
    if (points >= level * 200) {
      level++;
    }
    M5.Speaker.tone(1500, 500);
  }

  showResult(completed);
}

// ---------- RESULT ----------
void showResult(bool completed) {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(40, 30);

  if (completed) {
    M5.Lcd.print("Session Complete!");
  } else {
    M5.Lcd.print("Session Stopped");
  }

  M5.Lcd.setCursor(40, 70);
  M5.Lcd.printf("Distractions: %d", distractionCount);

  M5.Lcd.setCursor(40, 100);
  M5.Lcd.printf("Score: %d", focusScore);

  M5.Lcd.setCursor(40, 130);
  M5.Lcd.printf("Level: %d", level);

  delay(4000);
  showMenu();
}

// ---------- DISTRACTION ----------
void showDistractionMessage() {
  M5.Lcd.fillScreen(RED);
  M5.Lcd.setCursor(50, 100);
  M5.Lcd.print("Distraction logged!");
  delay(800);
}