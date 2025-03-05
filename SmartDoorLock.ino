#define BLYNK_TEMPLATE_ID "TMPL6oJYK_Tk2"
#define BLYNK_TEMPLATE_NAME "Skripsi"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>

#define RST_PIN 22     
#define SS_PIN 21      
#define I2C_SDA 4
#define I2C_SCL 5
#define SOLENOID_PIN 27
#define SERVO_PIN 33
#define PIR_PIN 25 
#define BUZZER_PIN 13 
#define REED_SWITCH_PIN 26 

unsigned long previousMillis = 0;
const long debounceDelay = 3000; 

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

byte readCard[4];     
byte storedCard[4];   
int cardCount = 0;    

#define EEPROM_SIZE 512  
#define CARD_COUNT_ADDR 0 
#define MASTER_CARD_ADDR 100 

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servo; 

char auth[] = "anSpOJxgW1LqSg1WB9T1Eal8xsRUIQni"; 
char ssid[] = "luffyy";       
char pass[] = "azzahraa";  
String botToken = "7537455442:AAH8PdWczdEwPQEucLQ0aWXn1Sy3BqAU6cQ"; 
String chatID = "6218856086"; 
bool authorizedAccess = false;

bool isDoorOpen = false;
bool isMovementDetected = false;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Blynk.begin(auth, ssid, pass);
  SPI.begin();        
  rfid.PCD_Init();   
  EEPROM.begin(EEPROM_SIZE); 
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.begin(16, 2);    
  lcd.backlight();
  lcd.clear();
  pinMode(SOLENOID_PIN, OUTPUT);
  digitalWrite(SOLENOID_PIN, LOW); 
  pinMode(PIR_PIN, INPUT); 
  pinMode(BUZZER_PIN, OUTPUT); 
  digitalWrite(BUZZER_PIN, LOW); 
  pinMode(REED_SWITCH_PIN, INPUT_PULLUP); 
  servo.attach(SERVO_PIN); 
  servo.write(90); 
}

void loop() {
  Blynk.run();
  unsigned long currentMillis = millis();

  // sensor PIR
  int pirState = digitalRead(PIR_PIN);
  if (pirState == HIGH && currentMillis - previousMillis >= debounceDelay) { 
    isMovementDetected = true;
    Blynk.virtualWrite(V3, "Ada Gerakan");
    Blynk.virtualWrite(V7, 1);
    previousMillis = currentMillis;
  } else if (pirState == LOW && currentMillis - previousMillis >= debounceDelay) {
    isMovementDetected = false;
    Blynk.virtualWrite(V3, "Tidak Ada Gerakan");
    Blynk.virtualWrite(V7, 0);
    previousMillis = currentMillis;
  }

  // Cek sensor Reed Switch untuk mengetahui apakah pintu dibuka
  int reedState = digitalRead(REED_SWITCH_PIN);
  if (reedState == LOW) { 
    isDoorOpen = false;
    Blynk.virtualWrite(V8, "Pintu Tertutup");
    Blynk.virtualWrite(V9, 0);
    digitalWrite(BUZZER_PIN, LOW);
    Blynk.virtualWrite(V6, 0);  
  } else { 
    isDoorOpen = true;
    Blynk.virtualWrite(V8, "Pintu Terbuka");
    Blynk.virtualWrite(V9, 1);
    if (!authorizedAccess) {
      if (digitalRead(SOLENOID_PIN) == LOW || servo.read() == 90)  { 
        sendTelegramMessage("Pintu dibuka secara paksa!"); 
        turnOnBuzzer(); 
      } else {
    digitalWrite(BUZZER_PIN, LOW);
    Blynk.virtualWrite(V6, 0); 
    }
    } else {
      authorizedAccess = false;
    }
  }

  // Kode RFID tetap sama
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return; 
  }
  for (byte i = 0; i < 4; i++) {
    readCard[i] = rfid.uid.uidByte[i];
  }
  String uidString = "";
  for (byte i = 0; i < 4; i++) {
    uidString += String(readCard[i], HEX);
    if (i != 3) uidString += ":";
  }
  Blynk.virtualWrite(V0, uidString);

  Serial.print("Kartu dibaca dengan UID: ");
  Serial.println(uidString);
  if (isMasterCard(readCard)) {
    Serial.println("Master Card terdeteksi! Masuk ke mode manajemen.");
    manageCards();
  } else if (findID()) {
    Serial.println("Selamat Datang");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Selamat Datang");

    // Buka pintu otomatis menggunakan RFID
    digitalWrite(SOLENOID_PIN, HIGH);
    Blynk.virtualWrite(V1, "Terbuka");
    Blynk.virtualWrite(V4, 1);
    delay(1000);
    servo.write(180);
    Blynk.virtualWrite(V2, "Pintu Terbuka");
    Blynk.virtualWrite(V5, 1);
    delay(4000);
    servo.write(90);
    Blynk.virtualWrite(V2, "Pintu Tertutup");
    Blynk.virtualWrite(V5, 0);
    delay(1000);
    digitalWrite(SOLENOID_PIN, LOW);
    Blynk.virtualWrite(V1, "Terkunci");
    Blynk.virtualWrite(V4, 0);
  } else {
    Serial.println("Akses Ditolak:(!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Akses Ditolak:(");
    delay(2000);
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + botToken + "/sendMessage?chat_id=" + chatID + "&text=" + message;
    Serial.println("URL: " + url);
    http.begin(url);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.println("HTTP Response Code: " + String(httpResponseCode));
      String payload = http.getString();
      Serial.println("Response Payload: " + payload);
    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected!");
  }
}

bool isMasterCard(byte *uid) {
  byte masterCard[4];
  for (byte i = 0; i < 4; i++) {
    masterCard[i] = EEPROM.read(MASTER_CARD_ADDR + i);
  }
  return memcmp(uid, masterCard, 4) == 0;
}

bool findID() {
  for (int i = 0; i < cardCount; i++) {
    int startAddr = 1 + (i * 4);
    for (byte j = 0; j < 4; j++) {
      storedCard[j] = EEPROM.read(startAddr + j);
    }
    if (memcmp(readCard, storedCard, 4) == 0) {
      return true;
    }
  }
  return false; 
}

void addCard() {
  int startAddr = 1 + (cardCount * 4); 
  for (byte i = 0; i < 4; i++) {
    EEPROM.write(startAddr + i, readCard[i]);
  }
  cardCount++; 
  EEPROM.write(CARD_COUNT_ADDR, cardCount); 
  EEPROM.commit(); 
  Serial.println("Kartu berhasil ditambahkan!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Card Ditambahkan");
  delay(2000);
}

void manageCards() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mode Manajemen");
  lcd.setCursor(0, 1);
  lcd.print("Dekatkan kartu");
  delay(2000);
  waitForCard();
  if (findID()) {
    deleteCard(readCard);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Kartu Dihapus");
  } else {
    addCard();
  }
  delay(2000);
}

void deleteCard(byte *uid) {
  for (int i = 0; i < cardCount; i++) {
    int startAddr = 1 + (i * 4);
    for (byte j = 0; j < 4; j++) {
      storedCard[j] = EEPROM.read(startAddr + j);
    }
    if (memcmp(uid, storedCard, 4) == 0) {
      for (int j = i; j < cardCount - 1; j++) {
        int nextStartAddr = 1 + ((j + 1) * 4);
        for (byte k = 0; k < 4; k++) {
          EEPROM.write(1 + (j * 4) + k, EEPROM.read(nextStartAddr + k));
        }
      }
      cardCount--; 
      EEPROM.write(CARD_COUNT_ADDR, cardCount);
      EEPROM.commit();
      Serial.println("Kartu berhasil dihapus!");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Kartu Dihapus");
      delay(2000);
      return;
    }
  }
  Serial.println("Akses Ditolak");
}

void waitForCard() {
  while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {}
  for (byte i = 0; i < 4; i++) {
    readCard[i] = rfid.uid.uidByte[i];
  }
}

BLYNK_WRITE(V4) { 
  int pinValue = param.asInt();
  if (pinValue) {
    digitalWrite(SOLENOID_PIN, HIGH);
    Blynk.virtualWrite(V1, "Terbuka"); 
    Serial.println("Solenoid ON ");
  } else {
    delay(80);
    digitalWrite(SOLENOID_PIN, LOW);
    Blynk.virtualWrite(V1, "Terkunci"); 
    Serial.println("Solenoid OFF ");
  }
}

BLYNK_WRITE(V5) { 
  int pinValue = param.asInt();
  if (pinValue) {
    delay(80);
    servo.write(180);
    Blynk.virtualWrite(V2, "Pintu Terbuka"); 
    Serial.println("Pintu Terbuka");
  } else {
    servo.write(90);
    Blynk.virtualWrite(V2, "Pintu Tertutup"); 
    Serial.println("Pintu Tertutup");
  }
}

BLYNK_WRITE(V6) { 
  int pinValue = param.asInt();
  if (pinValue) {
    turnOnBuzzer(); 
  } else {
    turnOffBuzzer(); 
  }
}
void turnOnBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH); 
  Blynk.virtualWrite(V6, 1);  
  Serial.println("Buzzer ON");
}
void turnOffBuzzer() {
  digitalWrite(BUZZER_PIN, LOW);  
  Blynk.virtualWrite(V6, 0);  
  Serial.println("Buzzer OFF");
}