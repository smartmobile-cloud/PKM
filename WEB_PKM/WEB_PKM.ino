#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h> 
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <addons/TokenHelper.h> // Tambahan untuk helper token
#include <addons/RTDBHelper.h>  // Tambahan untuk helper database

// ==========================================
// 1. KONFIGURASI (ISI DATA ANDA DI SINI)
// ==========================================
#define WIFI_SSID "dans"
#define WIFI_PASSWORD "brochacagaming"

// Masukkan API Key dari Firebase Project Settings
#define API_KEY "AIzaSyDwN39QZB-XIJmxWDJWtNayFbLbZeUSzLA" 

// Masukkan URL Database (harus lengkap dengan https://...)
#define DATABASE_URL "https://carlins-three-default-rtdb.asia-southeast1.firebasedatabase.app/" 

// ==========================================
// 2. SETUP HARDWARE
// ==========================================
#define GEIGER_PIN 4
#define BUZZER_PIN 14
LiquidCrystal_I2C lcd(0x27, 20, 4);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

volatile unsigned long pulseCount = 0;
unsigned long previousMillis = 0;
const int LOG_PERIOD = 1000; // Update Tiap 1 Detik

// Fungsi Interupsi (Wajib IRAM_ATTR untuk ESP32)
void IRAM_ATTR onPulse() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);
  
  // Setup Pin
  pinMode(GEIGER_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), onPulse, FALLING);

  // Setup LCD
  Wire.begin(21, 22); // SDA=21, SCL=22 (Standar ESP32)
  lcd.init(); 
  lcd.backlight();
  
  // Koneksi WiFi
  lcd.setCursor(0,0); lcd.print("Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi Connected!");
  lcd.setCursor(0,0); lcd.print("WiFi OK! Firebase...");

  // Konfigurasi Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  
  // Login Anonim
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Login Success");
    signupOK = true;
    lcd.setCursor(0,0); lcd.print("Firebase Ready! ");
  } else {
    Serial.printf("Error: %s\n", config.signer.signupError.message.c_str());
    lcd.setCursor(0,0); lcd.print("Firebase Error  ");
  }

  config.token_status_callback = tokenStatusCallback; // Helper fungsi
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // Hitung Setiap 1 Detik
  if (millis() - previousMillis >= LOG_PERIOD) {
    previousMillis = millis();
    
    // Ambil Data & Reset Counter (Pakai Atomic Block agar aman)
    noInterrupts();
    unsigned long counts = pulseCount;
    pulseCount = 0;
    interrupts();
    
    // Konversi Unit
    int cps = counts;
    int cpm = cps * 60;
    float uSv = cpm / 151.0; // Kalibrasi tabung J305 biasanya 151 CPM = 1 uSv/h

    // --- LOGIKA ALARM LOKAL ---
    if(cps >= 5) digitalWrite(BUZZER_PIN, HIGH);
    else digitalWrite(BUZZER_PIN, LOW);

    // --- UPDATE LCD ---
    lcd.setCursor(0, 1); lcd.print("CPS: "); lcd.print(cps); lcd.print("   ");
    lcd.setCursor(0, 2); lcd.print("CPM: "); lcd.print(cpm); lcd.print("   ");
    lcd.setCursor(0, 3); lcd.print(uSv, 4); lcd.print(" uSv");

    // --- KIRIM KE FIREBASE ---
    if (Firebase.ready() && signupOK) {
      FirebaseJson json;
      json.set("cps", cps);
      json.set("cpm", cpm);
      json.set("dose", uSv);

      // Kirim data ke path '/radiasi'
      if (Firebase.RTDB.setJSON(&fbdo, "/radiasi", &json)) {
         Serial.print("Data Terkirim -> CPM: "); Serial.println(cpm);
      } else {
         Serial.println("Gagal Kirim: " + fbdo.errorReason());
      }
    }
  }
}