#include <M5Stack.h>
#include <WiFi.h>
#include <HTTPClient.h>

// --- TILSTANDE (STATES) ---
enum State { NEW_TASK, REMOVE_TASK, USER_INPUT, CONFIRM, 
MENU, FOCUS_MODE, TODO_LIST, CONNECTING_WIFI, TIMER, EVALUATION, ADVICE,
LIST_MENU };
State currentState = CONNECTING_WIFI;

// --- KONFIGURATION & DATA ---
const char* todoLists[] = {"MyList", "Blud", "Shekibruv"};
const char* ssid = "bruv";
const char* password = "abekat38";
String focusModeScriptURL = "https://script.google.com/macros/s/AKfycbzl-xnkj_sglk8RS-LRgqJD_A63JQqANr5PPeK2SSnt7V69cWobKSwpVCzhxdU8W3SRBQ/exec";
String todoListScriptURL = "https://script.google.com/macros/s/AKfycbwpo7tY9ORE_DVNJHucpZv39JJ7U6dyVGdxa-wFIeAkXUe00jXHyhnjGT38Pkmvp4Oc/exec";
const char* selectedTodoList = todoLists[0];

int distractions = 0;
int focusScore =  100;
int selectedIndex = 3; // Standard 25 min
int timeOptions[5] = {1, 5, 15, 25, 45};

unsigned long focusDuration = timeOptions[selectedIndex] * 60 * 1000;
unsigned long startTime = 0;
bool sessionActive = false;


String inputText = "";

const char* keyboard[] = {
  "A","B","C","D","E","F","G","H","I","J",
  "K","L","M","N","O","P","Q","R","S","T",
  "U","V","W","X","Y","Z","<-","OK"
};

int keyboardSize = 28;

int cursorIndex = 0;

// --- HJÆLPEFUNKTIONER TIL UI ---
void drawHeader(const char* title) {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.fillRect(0, 0, 320, 40, DARKGREY);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.print(title);
  M5.Lcd.drawFastHLine(0, 40, 320, WHITE);
}

void drawButtons(const char* btnA, const char* btnB, const char* btnC) {
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(LIGHTGREY);
  if(btnA) { M5.Lcd.setCursor(60, 220); M5.Lcd.print(btnA); M5.Lcd.setCursor(65, 230); M5.Lcd.print("V"); }
  if(btnB) { M5.Lcd.setCursor(150, 220); M5.Lcd.print(btnB); M5.Lcd.setCursor(155, 230); M5.Lcd.print("V"); }
  if(btnC) { M5.Lcd.setCursor(240, 220); M5.Lcd.print(btnC); M5.Lcd.setCursor(245, 230); M5.Lcd.print("V"); }
}

// --- SKÆRM VISNINGER ---

void showMenu() {
  currentState = MENU;
  drawHeader("Main Menu");
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(60, 100);
  drawButtons("Focus Mode", "TODO-List", NULL);
}

void showFocusMode() {
  currentState = FOCUS_MODE;
  drawHeader("Focus Mode");
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(60, 100);
  M5.Lcd.printf("%d min", timeOptions[selectedIndex]);
  drawButtons("Change", "Start", NULL);
}

void showTODOMenu() {
  drawHeader("TODO-List");
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(60, 100);
  drawButtons("New", "Open", NULL);
}

void showSelectList() {
  drawHeader("Select TODO-List");
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(60, 100);
  drawButtons("Up", "Down", "Select");
}

void showListMenu() {
  drawHeader(selectedTodoList);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(60, 100);
  drawButtons("Change", "Select", "Mark as done");

}

void showUserInput() {

  drawHeader("User Input");

  M5.Lcd.setTextSize(2);

  int startX = 20;
  int startY = 60;
  int cols = 10;

  for (int i = 0; i < keyboardSize; i++) {
    
    int x = startX + (i % cols) * 28;
    int y = startY + (i / cols) * 30;

    if (i == cursorIndex) {
      M5.Lcd.fillRect(x-2, y-2, 26, 24, GREEN);
      M5.Lcd.setTextColor(BLACK);
    } else {
      M5.Lcd.setTextColor(WHITE);
    }
    if (keyboard[i] == "<-") {
      M5.Lcd.setTextColor(RED);
    } else if (keyboard[i] == "OK") {
      M5.Lcd.setTextColor(GREEN);
    }
    M5.Lcd.setCursor(x, y);
    M5.Lcd.print(keyboard[i]);
  }

  // Input tekst
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setCursor(20, 170);
  M5.Lcd.print("Input: " + inputText);

  drawButtons("Left", "Right", "Select");
}

void showRemoveTask() {
  drawHeader("Please select task to remove");
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(60, 100);
  drawButtons("Up", "Down", "Remove");
}

void showConfirmInput() {
  drawHeader("Are you sure?");
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(60, 100);
  drawButtons("Yes", "No", NULL);
}

void updateTimerUI() {
  unsigned long elapsed = millis() - startTime;
  if (elapsed >= focusDuration) {
    int minutes = (elapsed / 1000) / 60;
    int seconds = (elapsed / 1000) % 60;

    char durationStr[6];
    sprintf(durationStr, "%02d:%02d", minutes, seconds);

    focusScore = 100 - (10 * distractions);
    if (focusScore < 0) focusScore = 0;

    sendToGoogleSheets(durationStr, distractions, focusScore);

    currentState = EVALUATION;
    showEvaluation();
    return;
  }

  unsigned long remaining = focusDuration - elapsed;
  int m = (remaining / 1000) / 60;
  int s = (remaining / 1000) % 60;
  float progress = (float)elapsed / focusDuration;

  M5.Lcd.fillScreen(BLACK);
  // Cirkel timer
  int cx = 160, cy = 110, r = 70;
  M5.Lcd.drawCircle(cx, cy, r, DARKGREY);
  
  // Tegn fremskridts-bue (simpel version)
  float endAngle = (progress * 360.0) - 90.0;
  for(int i=-90; i < endAngle; i++) {
    float rad = i * 0.01745;
    M5.Lcd.drawPixel(cx + cos(rad)*r, cy + sin(rad)*r, GREEN);
    M5.Lcd.drawPixel(cx + cos(rad)*(r-1), cy + sin(rad)*(r-1), GREEN);
  }

  M5.Lcd.setTextSize(4);
  M5.Lcd.setCursor(110, 100);
  M5.Lcd.printf("%02d:%02d", m, s);
  drawButtons(NULL, NULL, "Log Distraction");
}

void showEvaluation() {
  currentState = EVALUATION;
  drawHeader("Session Evaluation");
  M5.Lcd.setCursor(20, 80);
  M5.Lcd.println("Were you feeling dizzy\nduring the session?");
  drawButtons("Yes", "No", "Skip");
}

void showAdvice(bool dizzy) {
  currentState = ADVICE;
  drawHeader("Advice for next time");
  M5.Lcd.setCursor(20, 80);
  if(dizzy) {
    M5.Lcd.println("Since you felt dizzy,\nremember to drink water\nand take a break.");
  } else {
    M5.Lcd.println("Great job!");
  }
  drawButtons(NULL, "Finish", NULL);
}

// --- SETUP & LOOP ---

void setup() {
  M5.begin();
  M5.Lcd.setBrightness(100);
  
  // WiFi Start
  drawHeader("Welcome");
  M5.Lcd.setCursor(20, 100);
  M5.Lcd.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  showMenu();
}

void loop() {
  M5.update();

  switch(currentState){
    case USER_INPUT:
      // venstre
      if (M5.BtnA.wasPressed()) {
        cursorIndex--;
        if (cursorIndex < 0) cursorIndex = keyboardSize - 1;
        showUserInput();
      }

      // højre
      if (M5.BtnB.wasPressed()) {
        cursorIndex++;
        if (cursorIndex >= keyboardSize) cursorIndex = 0;
        showUserInput();
      }

      // select
      if (M5.BtnC.wasPressed()) {

        String selected = keyboard[cursorIndex];

        if (selected == "<-") {
          // 🔙 backspace
          if (inputText.length() > 0) {
            inputText.remove(inputText.length() - 1);
          }
        }
        else if (selected == "OK") {
          // færdig
          currentState = CONFIRM;
          showConfirmInput();
          return;
        }
        else {
          // bogstav
          inputText += selected;
        }

        showUserInput();
      }
  break;

    case TODO_LIST:
   
    if (M5.BtnA.wasPressed()) {
      inputText = "";
      cursorIndex = 0;
      currentState = USER_INPUT;
      showUserInput();
    }
    if (M5.BtnB.wasPressed()) { 
      currentState = LIST_MENU; 
    }

    break;

    case FOCUS_MODE:
    if (M5.BtnA.wasPressed()) { 
      selectedIndex = (selectedIndex + 1) % 5;
      focusDuration = timeOptions[selectedIndex] * 60 * 1000;
      showFocusMode(); 
      }
    if (M5.BtnB.wasPressed()) { 
      startTime = millis(); 
      distractions = 0; 
      currentState = TIMER; 
    }


    break;
    case MENU:
    if (M5.BtnA.wasPressed()) { 
      currentState = FOCUS_MODE;
    }
    if (M5.BtnB.wasPressed()) { 
      currentState = TODO_LIST;
      showTODOMenu();
    }

    break;
    case TIMER:
    updateTimerUI();
    if (M5.BtnC.wasPressed()) {
      distractions++;
      M5.Lcd.fillScreen(RED);
      delay(200); // Hurtigt visuelt feedback
    }

    break;
    case EVALUATION:
    if (M5.BtnA.wasPressed()) showAdvice(true);
    if (M5.BtnB.wasPressed()) showAdvice(false);
    if (M5.BtnC.wasPressed()) showMenu();

    break;
    case ADVICE:
    if (M5.BtnB.wasPressed()) showMenu();
    break;


  }
}

// ---------------- GOOGLE LOGGING ----------------
void sendToGoogleSheets(String duration, int distractions, int score) {

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    String url = focusModeScriptURL;
    url += "?duration=" + duration;
    url += "&distractions=" + String(distractions);
    url += "&score=" + String(score);

    Serial.println(url);

    http.begin(url);
    http.GET();
    http.end();
  }
}