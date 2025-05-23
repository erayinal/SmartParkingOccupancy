#include "arduino_secrets.h"
#include <Wire.h>
#include <U8g2lib.h>
#include "DHT.h"
#include <ESP32Servo.h>
#include <time.h>
#include "thingProperties.h"

// OLED ekran
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 22, 21);

// DHT11
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// HC-SR04 sensörleri
#define TRIG1_PIN 5
#define ECHO1_PIN 18
#define TRIG2_PIN 13
#define ECHO2_PIN 12

// RGB LED
#define RED_PIN 25
#define GREEN_PIN 26
#define BLUE_PIN 27

// Servo
#define SERVO_PIN 32
Servo myServo;
bool servoManualOpened = false;
unsigned long servoOpenDurationMs = 6000;

// Tespit eşiği
#define DETECTION_CONFIRMATION_COUNT 4
int car1Counter = 0, empty1Counter = 0;
int car2Counter = 0, empty2Counter = 0;
bool isParked1 = false;
bool isParked2 = false;
unsigned long startTime1 = 0;
unsigned long startTime2 = 0;

// Otopark bilgisi
#define TOTAL_SLOTS 2
int lastDay = -1;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG1_PIN, OUTPUT); pinMode(ECHO1_PIN, INPUT);
  pinMode(TRIG2_PIN, OUTPUT); pinMode(ECHO2_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT); pinMode(GREEN_PIN, OUTPUT); pinMode(BLUE_PIN, OUTPUT);

  dht.begin();
  u8g2.begin(); u8g2.setFont(u8g2_font_ncenB08_tr);
  myServo.attach(SERVO_PIN); myServo.write(0);

  configTime(3600 * 3, 0, "pool.ntp.org");
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
}

void loop() {
  ArduinoCloud.update();

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  temperature = temp;
  humidity = hum;

  long dist1 = getDistance(TRIG1_PIN, ECHO1_PIN);
  long dist2 = getDistance(TRIG2_PIN, ECHO2_PIN);
  distanceCm1 = dist1;
  distanceCm2 = dist2;

  bool car1Present = false;
  bool car2Present = false;

  if (dist1 < 5) { car1Counter++; empty1Counter = 0; } else { empty1Counter++; car1Counter = 0; }
  if (dist2 < 5) { car2Counter++; empty2Counter = 0; } else { empty2Counter++; car2Counter = 0; }

  if (car1Counter >= DETECTION_CONFIRMATION_COUNT) car1Present = true;
  if (empty1Counter >= DETECTION_CONFIRMATION_COUNT) car1Present = false;
  if (car2Counter >= DETECTION_CONFIRMATION_COUNT) car2Present = true;
  if (empty2Counter >= DETECTION_CONFIRMATION_COUNT) car2Present = false;

  int occupied = (car1Present ? 1 : 0) + (car2Present ? 1 : 0);
  int dolulukPercent = (occupied * 100) / TOTAL_SLOTS;
  occupancyPercent = dolulukPercent;
  parkedStatus = occupied > 0;

  // Günlük reset
  time_t now = time(NULL);
  struct tm* timeInfo = localtime(&now);
  int currentDay = timeInfo->tm_mday;
  if (lastDay != -1 && currentDay != lastDay) {
    vehicleCount = 0;
    dailyIncome = 0;
    lastParkDuration = 0;
  }
  lastDay = currentDay;

  handleParkingSlot(car1Present, isParked1, startTime1, parkedDuration, false);
  isParked1 = car1Present;

  handleParkingSlot(car2Present, isParked2, startTime2, parkedDuration2, true);
  isParked2 = car2Present;

  setLED(occupied);

  u8g2.clearBuffer();
  u8g2.setCursor(0, 10); u8g2.print("Doluluk: %"); u8g2.print(dolulukPercent);
  u8g2.setCursor(0, 25); u8g2.print("Sicaklik: "); u8g2.print(temp); u8g2.print("C");
  u8g2.setCursor(0, 40); u8g2.print("Nem: "); u8g2.print(hum); u8g2.print("%");
  u8g2.sendBuffer();

  if (servoControl && !servoManualOpened) {
    triggerServo();
    servoManualOpened = true;
    servoControl = false;
  }
  if (!servoControl) servoManualOpened = false;

  delay(500);
}

void handleParkingSlot(bool present, bool wasParked, unsigned long &startTime, int &durationOut, bool isSlot2) {
  if (present && !wasParked) {
    startTime = millis();
    messageText = "Hosgeldiniz!";
  }

  if (!present && wasParked) {
    unsigned long durationSec = (millis() - startTime) / 1000;
    float cost = (durationSec / 60.0) * 2.5;

    paymentAmount = cost;
    lastParkDuration = durationSec;
    vehicleCount++;
    dailyIncome += cost;
    messageText = "Gule Gule!";

    // Doluluk ve LED'i önce güncelle
    int currentOccupied = (isParked1 ? 1 : 0) + (isParked2 ? 1 : 0) - 1;
    occupancyPercent = (currentOccupied * 100) / TOTAL_SLOTS;
    setLED(currentOccupied);

    u8g2.clearBuffer();
    u8g2.setCursor(0, 20); u8g2.print("Ucret: "); u8g2.print(cost); u8g2.print(" TL");
    u8g2.setCursor(0, 40); u8g2.print("Gule Gule!");
    u8g2.sendBuffer();

    delay(1000); // ekran gösterimi için kısa gecikme

    triggerServo();
  }

  if (present) {
    unsigned long durationSec = (millis() - startTime) / 1000;
    durationOut = durationSec;
    if (isSlot2) parkedDuration2 = durationSec;
  }
}

long getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  return pulseIn(echo, HIGH) * 0.034 / 2;
}

void triggerServo() {
  myServo.attach(SERVO_PIN);
  myServo.write(90);
  delay(servoOpenDurationMs);
  myServo.write(0);
  delay(1000);
  myServo.detach();
}

void setLED(int occupied) {
  if (occupied >= TOTAL_SLOTS) {
    analogWrite(RED_PIN, 255);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 0);
  } else {
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 255);
    analogWrite(BLUE_PIN, 0);
  }
}

// Cloud callback'ler
void onMessageTextChange() {}
void onHumidityChange() {}
void onPaymentAmountChange() {}
void onTemperatureChange() {}
void onDistanceCm1Change() {}
void onOccupancyPercentChange() {}
void onParkedDurationChange() {}
void onParkedStatusChange() {}
void onServoControlChange() {}
void onDailyIncomeChange() {}
void onVehicleCountChange() {}
void onLastParkDurationChange() {}
void onDistanceCmChange() {}