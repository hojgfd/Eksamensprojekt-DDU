#include <M5Stack.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// --- DATA STRUKTUR ---
#define MAX_LISTS 5
#define MAX_TASKS 10

struct Task {
  String name;
  bool completed;
};

struct TodoList {
  String name;
  Task tasks[MAX_TASKS];
  int taskCount;
};

TodoList todoLists[MAX_LISTS];
int todoListCount = 0;
int selectedListIndex = 0;
int selectedTaskIndex = 0;  // Index of the currently selected task in OPEN_LIST

// --- TILSTANDE (STATES) ---
enum State {
  NEW_TASK, REMOVE_TASK, USER_INPUT, CONFIRM,
  MENU, FOCUS_MODE, TODO_LIST, CONNECTING_WIFI, TIMER, EVALUATION, ADVICE,
  LIST_MENU,
  SELECT_LIST,   // Added
  OPEN_LIST      // Added
};

State currentState = CONNECTING_WIFI;

// --- KONFIGURATION & DATA ---
const char* ssid = "bruv";
const char* password = "abekat38";
const char* serverURL = "https://oscar12345.pythonanywhere.com"; // CHANGE THIS

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

// --- ENCODING ---
String encodeURL(String str) {
  String encoded = "";
  char c;
  char code0;
  char code1;

  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);

    if (isalnum(c)) {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

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


void createNewListFromInput(String name) {
    if(todoListCount < MAX_LISTS) {
        todoLists[todoListCount].name = name;
        todoLists[todoListCount].taskCount = 0;
        todoListCount++;
    }
}

void addTaskToList(int listIndex, String taskName) {
  if(listIndex < todoListCount) {
    TodoList &list = todoLists[listIndex];
    if(list.taskCount < MAX_TASKS) {
      list.tasks[list.taskCount].name = taskName;
      list.tasks[list.taskCount].completed = false;
      list.taskCount++;
    }
  }
}

void removeTaskFromList(int listIndex, int taskIndex) {
  if(listIndex < todoListCount) {
    TodoList &list = todoLists[listIndex];
    if(taskIndex >= 0 && taskIndex < list.taskCount) {
      for(int i = taskIndex; i < list.taskCount - 1; i++) {
        list.tasks[i] = list.tasks[i + 1];
      }
      list.taskCount--;
    }
  }
}


void showTODOListsMenu() {
    currentState = LIST_MENU;
    drawHeader("TODO Lists");

    M5.Lcd.setTextSize(2);
    for(int i = 0; i < todoListCount; i++){
        M5.Lcd.setCursor(20, 60 + i*20);
        M5.Lcd.printf("%d. %s", i + 1, todoLists[i].name.c_str());
    }

    drawButtons("New List", "Open List", "Send All");
}

void showSelectList() {
  drawHeader("Select TODO-List");
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(60, 100);
  drawButtons("Up", "Down", "Select");
}

/*void showListMenu() {
  drawHeader(todoLists[selectedListIndex].name.c_str());
  M5.Lcd.setTextSize(2);
  
  for(int i=0; i<todoLists[selectedListIndex].taskCount; i++){
    M5.Lcd.setCursor(20, 60 + i*20);
    M5.Lcd.printf("%d. %s %s", i+1, 
        todoLists[selectedListIndex].tasks[i].name.c_str(),
        todoLists[selectedListIndex].tasks[i].completed ? "[X]" : "[ ]");
  }
  
  drawButtons("New Task", "Delete Task", "Send All");
}*/

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

    sendFocusToServer(durationStr, distractions, focusScore);

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

void showListTasksMenu() {
    currentState = OPEN_LIST;
    drawHeader(todoLists[selectedListIndex].name.c_str());
    
    for(int i = 0; i < todoLists[selectedListIndex].taskCount; i++){
        M5.Lcd.setCursor(20, 60 + i*20);
        if(i == selectedTaskIndex){
            M5.Lcd.setTextColor(GREEN);
        } else {
            M5.Lcd.setTextColor(WHITE);
        }
        M5.Lcd.printf("%d. %s %s", i+1, 
            todoLists[selectedListIndex].tasks[i].name.c_str(),
            todoLists[selectedListIndex].tasks[i].completed ? "[X]" : "[ ]");
    }

    drawButtons("Edit Task", "Toggle Done", "Back");
}

void showSelectListMenu() {
    currentState = SELECT_LIST;
    drawHeader("Select List");

    for(int i=0; i<todoListCount; i++){
        M5.Lcd.setCursor(20, 60 + i*20);
        if(i == selectedListIndex){
            M5.Lcd.setTextColor(GREEN);
        } else {
            M5.Lcd.setTextColor(WHITE);
        }
        M5.Lcd.print(todoLists[i].name);
    }
    drawButtons("Up", "Down", "Select");
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

enum InputAction {
  NONE,
  LIST_MENU_CREATE,   // Creating a new TODO list
  EDIT_TASK           // Editing a task in an existing list
};

InputAction nextStateAfterInput = NONE;

void loop() {
  M5.update();

  switch(currentState){
    case USER_INPUT:
    // Navigate left
    if (M5.BtnA.wasPressed()) {
      cursorIndex--;
      if (cursorIndex < 0) cursorIndex = keyboardSize - 1;
      showUserInput();
    }

    // Navigate right
    if (M5.BtnB.wasPressed()) {
      cursorIndex++;
      if (cursorIndex >= keyboardSize) cursorIndex = 0;
      showUserInput();
    }

    // Select key
    if (M5.BtnC.wasPressed()) {
      String selected = keyboard[cursorIndex];

      if (selected == "<-") {
        // Backspace
        if (inputText.length() > 0) inputText.remove(inputText.length() - 1);
      } else if (selected == "OK") {
        // Finished input
        currentState = CONFIRM;
        showConfirmInput();
        return;
      } else {
        // Add character
        inputText += selected;
      }

      showUserInput();
    }
  break;

  case CONFIRM:
    if (M5.BtnA.wasPressed()) {
      // Confirm input
      if (nextStateAfterInput == LIST_MENU_CREATE) {
        createNewListFromInput(inputText);
      } 
      else if (nextStateAfterInput == EDIT_TASK) {
        todoLists[selectedListIndex].tasks[selectedTaskIndex].name = inputText;
      }

      // Reset temporary input state
      inputText = "";
      nextStateAfterInput = NONE;

      // Return to list menu
      showTODOListsMenu();
      currentState = LIST_MENU;
    }

    if (M5.BtnB.wasPressed()) {
      // Cancel → return to input editing
      currentState = USER_INPUT;
      showUserInput();
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
      showTODOListsMenu();
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

  
    case LIST_MENU:
    if(M5.BtnA.wasPressed()){ 
        // New List → go to user input
        inputText = "";
        cursorIndex = 0;
        currentState = USER_INPUT;
        nextStateAfterInput = LIST_MENU_CREATE; // custom enum to know what to do with input
        showUserInput();
    }
    if(M5.BtnB.wasPressed()){ 
        // Open List
        selectedListIndex = 0;
        selectedTaskIndex = 0;
        showSelectListMenu();
    }
    if(M5.BtnC.wasPressed()){ 
        // Send all data
        sendAllListsToServer();
        showTODOListsMenu();
    }
    break;

    case SELECT_LIST:
        // Up/Down selection
        if(M5.BtnA.wasPressed()) selectedListIndex = max(0, selectedListIndex-1);
        if(M5.BtnB.wasPressed()) selectedListIndex = min(todoListCount-1, selectedListIndex+1);
        if(M5.BtnC.wasPressed()) showListTasksMenu();
        break;

    case OPEN_LIST:
        if(M5.BtnA.wasPressed()) {
            // Edit task → copy name to inputText and open USER_INPUT
            inputText = todoLists[selectedListIndex].tasks[selectedTaskIndex].name;
            currentState = USER_INPUT;
            nextStateAfterInput = EDIT_TASK;
        }
        if(M5.BtnB.wasPressed()) {
            // Toggle completed
            todoLists[selectedListIndex].tasks[selectedTaskIndex].completed = 
                !todoLists[selectedListIndex].tasks[selectedTaskIndex].completed;
            showListTasksMenu();
        }
        if(M5.BtnC.wasPressed()) showTODOListsMenu();
        break;
  }
}



// ---------------- FOCUS SERVER POST REQ ----------------
void sendFocusToServer(String duration, int distractions, int score) {

  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url = String(serverURL) + "/focus";
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"duration\":\"" + duration + "\",";
  json += "\"distractions\":" + String(distractions) + ",";
  json += "\"score\":" + String(score) + ",";
  json += "\"device\":\"M5Stack\"";
  json += "}";

  Serial.println("Sending JSON:");
  Serial.println(json);

  int httpResponseCode = http.POST(json);

  Serial.print("Focus response: ");
  Serial.println(httpResponseCode);

  http.end();
}

String buildAllListsJSON() {
  String json = "[";
  for(int l = 0; l < todoListCount; l++){
    json += "{";
    json += "\"todolist_id\":" + String(l + 1) + ",";
    json += "\"tasks\":[";
    for(int t = 0; t < todoLists[l].taskCount; t++){
      json += "{";
      json += "\"tasknum\":" + String(t + 1) + ",";
      json += String("\"completed\":") + (todoLists[l].tasks[t].completed ? "true" : "false");
      json += "}";
      if(t < todoLists[l].taskCount - 1) json += ",";
    }
    json += "]}";
    if(l < todoListCount - 1) json += ",";
  }
  json += "]";
  return json;
}

void sendAllListsToServer() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = String(serverURL) + "/todolist";
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  String json = buildAllListsJSON();
  Serial.println("Sending JSON:");
  Serial.println(json);

  int httpResponseCode = http.POST(json);
  Serial.print("Todo response: ");
  Serial.println(httpResponseCode);

  http.end();
}