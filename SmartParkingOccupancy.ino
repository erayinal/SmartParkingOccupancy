#include <Wire.h>
#include <U8g2lib.h>
#include "DHT.h"
#include <ESP32Servo.h>
#include <time.h>
#include "arduino_secrets.h"
#include "thingProperties.h"

// OLED
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 22, 21);

// DHT11
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Ultrasonik
#define TRIG_PIN 5
#define ECHO_PIN 18

// RGB
#define RED_PIN 25
#define GREEN_PIN 26
#define BLUE_PIN 27

// Servo
#define SERVO_PIN 32
Servo myServo;
bool servoOpened = false;
bool servoManualOpened = false;
unsigned long servoOpenDurationMs = 6000; // servo süre

// Mesafe filtresi
#define DETECTION_CONFIRMATION_COUNT 4  // hata sayısı
int carPresentCounter = 0;
int carAbsentCounter = 0;

// Durumlar
bool isParked = false;
unsigned long parkStartTime = 0;
#define TOTAL_SLOTS 100
#define BASE_OCCUPIED 34

// Gün kontrolü
int lastDay = -1;

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  dht.begin();
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  myServo.attach(SERVO_PIN);
  myServo.write(0);

  configTime(3600 * 3, 0, "pool.ntp.org"); // UTC+3

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
}

void loop() {
  ArduinoCloud.update();

  long distance = getDistance();
  distanceCm = distance;

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  temperature = temp;
  humidity = hum;

  // Mesafe filtreleme
  if (distance < 5) {
    carPresentCounter++;
    carAbsentCounter = 0;
  } else {
    carAbsentCounter++;
    carPresentCounter = 0;
  }

  bool carPresent = false;
  if (carPresentCounter >= DETECTION_CONFIRMATION_COUNT) {
    carPresent = true;
  } else if (carAbsentCounter >= DETECTION_CONFIRMATION_COUNT) {
    carPresent = false;
  }

  int doluluk = BASE_OCCUPIED + (carPresent ? 1 : 0);
  int dolulukPercent = (doluluk * 100) / TOTAL_SLOTS;

  occupancyPercent = dolulukPercent;
  parkedStatus = carPresent;

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

  // Araç yeni geldi
  if (carPresent && !isParked) {
    isParked = true;
    parkStartTime = millis();
    messageText = "Hosgeldiniz!";
    servoOpened = false;
  }

  // Araç yeni çıktı
  if (!carPresent && isParked) {
    isParked = false;
    unsigned long durationSec = (millis() - parkStartTime) / 1000;
    float cost = (durationSec / 60.0) * 2.5;

    parkedDuration = 0;
    paymentAmount = cost;
    lastParkDuration = durationSec;
    vehicleCount++;
    dailyIncome += cost;
    messageText = "Gule Gule!";
    setLED(false);

    if (!servoOpened) {
      myServo.attach(SERVO_PIN);
      myServo.write(90);
      delay(servoOpenDurationMs);
      myServo.write(0);
      delay(1000);
      myServo.detach();
      servoOpened = true;
    }

    u8g2.clearBuffer();
    u8g2.setCursor(0, 15); u8g2.print("Ucret: "); u8g2.print(cost); u8g2.print(" TL");
    u8g2.setCursor(0, 40); u8g2.print("Gule Gule!");
    u8g2.sendBuffer();

    delay(3000);
  }

  // Park halinde
  if (carPresent) {
    setLED(true);
    unsigned long durationSec = (millis() - parkStartTime) / 1000;
    parkedDuration = durationSec;
    paymentAmount = 0;
    messageText = "Hosgeldiniz!";

    u8g2.clearBuffer();
    u8g2.setCursor(0, 10); u8g2.print("Hosgeldiniz!");
    u8g2.setCursor(0, 30); u8g2.print("Sure: "); u8g2.print(durationSec); u8g2.print(" s");
    u8g2.setCursor(0, 50); u8g2.print("Doluluk: %"); u8g2.print(dolulukPercent);
    u8g2.sendBuffer();
  }

  // Otopark boş
  if (!carPresent && !isParked) {
    parkedDuration = 0;
    paymentAmount = 0;
    messageText = "Otopark Bos";
    setLED(false);

    u8g2.clearBuffer();
    u8g2.setCursor(0, 10); u8g2.print("Otopark");
    u8g2.setCursor(0, 25); u8g2.print("Doluluk: %"); u8g2.print(dolulukPercent);
    u8g2.setCursor(0, 40); u8g2.print("Sicaklik: "); u8g2.print(temp); u8g2.print("C");
    u8g2.setCursor(0, 55); u8g2.print("Nem: "); u8g2.print(hum); u8g2.print("%");
    u8g2.sendBuffer();
  }

  // Manuel servo kontrol
  if (servoControl && !servoManualOpened) {
    myServo.attach(SERVO_PIN);
    myServo.write(90);
    delay(servoOpenDurationMs);
    myServo.write(0);
    delay(1000);
    myServo.detach();
    servoManualOpened = true;
    servoControl = false;
  }
  if (!servoControl) {
    servoManualOpened = false;
  }

  delay(500);
}

long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;
}

void setLED(bool occupied) {
  if (occupied) {
    analogWrite(RED_PIN, 255);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 0);
  } else {
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 255);
    analogWrite(BLUE_PIN, 0);
  }
}

// Cloud callback boş tanımlar
void onMessageTextChange() {}
void onHumidityChange() {}
void onPaymentAmountChange() {}
void onTemperatureChange() {}
void onOccupancyPercentChange() {}
void onParkedDurationChange() {}
void onParkedStatusChange() {}
void onDistanceCmChange() {}
void onVehicleCountChange() {}
void onDailyIncomeChange() {}
void onLastParkDurationChange() {}
void onServoControlChange() {}