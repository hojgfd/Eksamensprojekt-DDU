#include <M5Stack.h>

int counter = 0;

void setup() {
  M5.begin();
  
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(40, 40);
  M5.Lcd.println("Ready!");
  M5.Lcd.setCursor(40, 70);
  M5.Lcd.println("Press A, B or C");
}

void loop() {
  M5.update(); 

  // Knap A
  if (M5.BtnA.wasPressed()) {
    counter++;
    M5.Lcd.fillScreen(BLUE);
    M5.Lcd.setCursor(40, 40);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.printf("Button A\nCounter: %d", counter);
  }

  // Knap B
  if (M5.BtnB.wasPressed()) {
    M5.Lcd.fillScreen(GREEN);
    M5.Lcd.setCursor(40, 40);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.println("Button B pressed");
  }

  // Knap C
  if (M5.BtnC.wasPressed()) {
    M5.Lcd.fillScreen(RED);
    M5.Lcd.setCursor(40, 40);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.println("Button C pressed");
  }
}