#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Tombol
#define BTN_UP 3
#define BTN_DOWN 4
#define BTN_NEXT 7
#define BTN_BACK 8

int sensor[8];
int threshold = 500;  // nilai batas hitam-putih

// Pin MUX
const int muxS0 = A1;
const int muxS1 = A2;
const int muxS2 = A3;
const int muxSIG = A0;

// Motor pin H-Bridge
const int IN1 = 6;
const int IN2 = 5;

const int IN3 = 9;
const int IN4 = 10;

// Menu
const char* mainMenuItems[] = { "Line Following", "Line Maze", "Tuning" };
int mainMenuIndex = 0;

// EEPROM
int pathIndex = 0;
#define EEPROM_FLAG_ADDR 0
#define EEPROM_PATH_START 1
#define MAX_PATH_LENGTH 50
char pathData[MAX_PATH_LENGTH];

// PID
float kp = 1.0, ki = 0.5, kd = 0.2;
int tuningIndex = 0;
const char* tuningParams[] = { "Kp", "Ki", "Kd" };

enum Mode {
  MODE_NONE,
  MODE_LINEFOLLOW,
  MODE_LINEMAZE,
  MODE_TUNING
};

Mode currentMode = MODE_NONE;

void setup() {
  EEPROM.get(10, kp);
  EEPROM.get(14, ki);
  EEPROM.get(18, kd);
  // Jika kosong/baru, tetapkan default
  if (isnan(kp) || isnan(ki) || isnan(kd)) {
    kp = 1.0;
    ki = 0.5;
    kd = 0.2;
  }

  EEPROM.get(22, threshold);
  if (threshold <= 0 || threshold > 1023) threshold = 500;  // fallback default

  pinMode(BTN_UP, INPUT);
  pinMode(BTN_DOWN, INPUT);
  pinMode(BTN_NEXT, INPUT);
  pinMode(BTN_BACK, INPUT);

  pinMode(muxS0, OUTPUT);
  pinMode(muxS1, OUTPUT);
  pinMode(muxS2, OUTPUT);
  pinMode(muxSIG, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  tampilkanMenuUtama();
}

void tampilkanMenuUtama() {
  while (true) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("== Main Menu ==");

    for (int i = 0; i < 3; i++) {
      if (i == mainMenuIndex) display.print("> ");
      else display.print("  ");
      display.println(mainMenuItems[i]);
    }
    display.display();

    if (digitalRead(BTN_UP) == HIGH && mainMenuIndex > 0) {
      mainMenuIndex--;
      delay(200);
    }

    if (digitalRead(BTN_DOWN) == HIGH && mainMenuIndex < 2) {
      mainMenuIndex++;
      delay(200);
    }

    if (digitalRead(BTN_NEXT) == HIGH) {
      delay(200);
      switch (mainMenuIndex) {
        case 0:
          currentMode = MODE_LINEFOLLOW;
          return;
        case 1:
          currentMode = MODE_LINEMAZE;
          return;
        case 2:
          tampilkanTuningMenu();  // 🔧 Ganti dari tuningPID()
          break;
      }
    }
  }
}


void selectMUX(int channel) {
  digitalWrite(muxS0, channel & 1);
  digitalWrite(muxS1, (channel >> 1) & 1);
  digitalWrite(muxS2, (channel >> 2) & 1);
}

void readSensors() {
  for (int i = 0; i < 8; i++) {
    selectMUX(i);
    delayMicroseconds(5);
    sensor[i] = analogRead(muxSIG);
  }
}

void followLine() {
  // Dummy PID untuk line following
  int position = calculatePosition();  // 0~7000
  int error = position - 3500;
  float correction = kp * error;

  int baseSpeed = 100;
  int leftSpeed = baseSpeed + correction;
  int rightSpeed = baseSpeed - correction;

  motorControl(leftSpeed, rightSpeed);
}

int calculatePosition() {
  long weightedSum = 0;
  int sum = 0;

  for (int i = 0; i < 8; i++) {
    int value = sensor[i] > threshold ? 1 : 0;
    weightedSum += (long)value * i * 1000;
    sum += value;
  }

  if (sum == 0) return 3500;  // fallback ke tengah
  return weightedSum / sum;
}

void motorControl(int leftSpeed, int rightSpeed) {
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  // Motor kiri
  if (leftSpeed >= 0) {
    analogWrite(IN1, leftSpeed);
    digitalWrite(IN2, LOW);
  } else {
    analogWrite(IN2, -leftSpeed);
    digitalWrite(IN1, LOW);
  }

  // Motor kanan
  if (rightSpeed >= 0) {
    analogWrite(IN3, rightSpeed);
    digitalWrite(IN4, LOW);
  } else {
    analogWrite(IN4, -rightSpeed);
    digitalWrite(IN3, LOW);
  }
}

void displayMessage(const char* msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

void tampilkanSensorVisual() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Line Sensor:");

  for (int i = 0; i < 8; i++) {
    int x = i * 16;  // Spasi horizontal antar sensor
    int y = 20;

    if (sensor[i] > threshold) {
      // Putih = 0
      display.drawRect(x, y, 10, 10, SSD1306_WHITE);  // kotak kosong
    } else {
      // Hitam = 1
      display.fillRect(x, y, 10, 10, SSD1306_WHITE);  // kotak terisi
    }
  }

  display.display();
}

void tampilkanTuningMenu() {
  int tuningMenuIndex = 0;
  const char* tuningMenuItems[] = { "Tuning PID", "Tuning Threshold" };

  while (true) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("== Tuning Menu ==");

    for (int i = 0; i < 2; i++) {
      if (i == tuningMenuIndex) display.print("> ");
      else display.print("  ");
      display.println(tuningMenuItems[i]);
    }
    display.display();

    if (digitalRead(BTN_UP) == HIGH && tuningMenuIndex > 0) {
      tuningMenuIndex--;
      delay(200);
    }

    if (digitalRead(BTN_DOWN) == HIGH && tuningMenuIndex < 1) {
      tuningMenuIndex++;
      delay(200);
    }

    if (digitalRead(BTN_NEXT) == HIGH) {
      delay(200);
      if (tuningMenuIndex == 0) {
        tuningPID();
        return;
      } else if (tuningMenuIndex == 1) {
        tuningThreshold();
        return;
      }
    }

    if (digitalRead(BTN_BACK) == HIGH) {
      delay(200);
      return;
    }
  }
}

void tuningThreshold() {
  bool running = true;

  while (running) {
    readSensors();

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("== Threshold Tuning ==");
    display.print("Threshold: ");
    display.println(threshold);
    display.println("UP/DOWN: Ubah");
    display.println("NEXT: Simpan");
    display.println("BACK: Batal");

    // Tampilkan visual sensor IR
    for (int i = 0; i < 8; i++) {
      int x = i * 16;
      int y = 48;

      if (sensor[i] > threshold) {
        display.drawRect(x, y, 10, 10, SSD1306_WHITE);
      } else {
        display.fillRect(x, y, 10, 10, SSD1306_WHITE);
      }
    }

    display.display();

    if (digitalRead(BTN_UP) == HIGH) {
      threshold += 10;
      delay(200);
    }

    if (digitalRead(BTN_DOWN) == HIGH) {
      threshold -= 10;
      delay(200);
    }

    if (digitalRead(BTN_NEXT) == HIGH) {
      EEPROM.put(22, threshold);  // Simpan threshold ke EEPROM
      displayMessage("Threshold disimpan!");
      delay(1000);
      running = false;
    }

    if (digitalRead(BTN_BACK) == HIGH) {
      running = false;
    }
  }

  tampilkanMenuUtama();
}

void tuningPID() {
  bool tuning = true;
  display.clearDisplay();

  while (tuning) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("== PID Tuning ==");

    display.print(tuningParams[tuningIndex]);
    display.print(": ");

    float value;
    if (tuningIndex == 0) value = kp;
    else if (tuningIndex == 1) value = ki;
    else value = kd;

    display.println(value, 2);
    display.println("UP/DOWN: Ubah");
    display.println("NEXT: Ganti param");
    display.println("BACK: Simpan & Keluar");
    display.display();

    if (digitalRead(BTN_UP) == HIGH) {
      if (tuningIndex == 0) kp += 0.1;
      else if (tuningIndex == 1) ki += 0.1;
      else kd += 0.1;
      delay(200);
    }

    if (digitalRead(BTN_DOWN) == HIGH) {
      if (tuningIndex == 0) kp -= 0.1;
      else if (tuningIndex == 1) ki -= 0.1;
      else kd -= 0.1;
      delay(200);
    }

    if (digitalRead(BTN_NEXT) == HIGH) {
      tuningIndex = (tuningIndex + 1) % 3;
      delay(200);
    }

    if (digitalRead(BTN_BACK) == HIGH) {
      EEPROM.put(10, kp);
      EEPROM.put(14, ki);
      EEPROM.put(18, kd);
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("PID disimpan!");
      display.display();
      delay(1000);
      tuning = false;
    }
  }

  tampilkanMenuUtama();
}

void changeTuningValue(float delta) {
  if (tuningIndex == 0) kp = max(0, kp + delta);
  if (tuningIndex == 1) ki = max(0, ki + delta);
  if (tuningIndex == 2) kd = max(0, kd + delta);
}

void loop() {
  if (currentMode == MODE_LINEFOLLOW) {
    lineFollow();
  } else if (currentMode == MODE_LINEMAZE) {
    lineMaze();
  }
}

void lineFollow() {
  // Contoh loop navigasi
  readSensors();
  tampilkanSensorVisual();
  followLine();

  delay(100);
}

void lineMaze() {
  // Line maze bisa punya logika berbeda
  readSensors();  // atau fungsi khusus line maze
}