#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "Base64.h"
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

WebServer server(80);

//=====================
// WIFI & SERVER
//=====================
const char* ssid = "STARLINK TECNO";
const char* password = "123456789";
const char* serverUrl = "http://10.210.63.171:8000/api/gate-in";
const char* heartbeatUrl = "http://10.210.63.171:8000/api/device/heartbeat";

//=====================
// PIN ESP32 WROVER DEV
//=====================
#define TRIG_PIN 14
#define ECHO_PIN 12
#define BUTTON_PIN 13
#define SERVO_PIN 15
#define LED_PIN 2

//=====================
// I2C LCD PINS
//=====================
#define I2C_SDA 32
#define I2C_SCL 33
LiquidCrystal_I2C lcd(0x27, 16, 2);

//=====================
// CAMERA PIN WROVER DEV
//=====================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     21
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       19
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM        5
#define Y2_GPIO_NUM        4
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

Servo palangServo;

bool mobilAda = false;
unsigned long lastTrigger = 0;
unsigned long lastHeartbeat = 0;

//================================================
// FUNGSI HELPER LCD
//================================================
void tampilLCD(String baris1, String baris2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(baris1);
  lcd.setCursor(0, 1);
  lcd.print(baris2);
}

//================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\nBooting ESP32-CAM Gerbang Masuk...");

  // Init LCD I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  tampilLCD("SYSTEM BOOTING", "MOHON TUNGGU...");

  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  palangServo.attach(SERVO_PIN);
  tutupPalang();

  //=====================
  // INIT CAMERA
  //=====================
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
    Serial.println("PSRAM ditemukan");
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("PSRAM tidak ditemukan");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera Error: 0x%x\n", err);
    tampilLCD("CAMERA ERROR!", "RESTARTING...");
    delay(1000);
    ESP.restart(); // Restart kalau kamera gagal
    return;
  }
  Serial.println("Camera OK");

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);   // 1 = Membalik gambar secara vertikal (atas-bawah)
  s->set_hmirror(s, 0); // 1 = Membalik cermin horizontal (kiri-kanan)

  s->set_brightness(s, 1);     // Naikkan kecerahan (opsi: -2 sampai 2)
  s->set_contrast(s, 1);       // Naikkan kontras agar objek tidak pudar
  s->set_exposure_ctrl(s, 1);  // Aktifkan Auto Exposure (otomatis atur cahaya)
  s->set_aec2(s, 1);           // Aktifkan Advanced Auto Exposure
  s->set_ae_level(s, 2);       // Tarik level pencahayaan ke maksimal (opsi: -2 sampai 2)
  s->set_gain_ctrl(s, 1);      // Aktifkan Auto Gain (Sensitivitas cahaya sensor)
  s->set_agc_gain(s, 30);      // Maksimalkan Gain (opsi: 0 - 30) khusus ruang gelap
  s->set_bpc(s, 1);            // Aktifkan koreksi piksel hitam (mengurangi noise/bintik)
  s->set_wpc(s, 1);            // Aktifkan koreksi piksel putih

  //=====================
  // CONNECT WIFI
  //=====================
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan WiFi");
  tampilLCD("KONEKSI WIFI...", ssid);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP()); // HARUS 10.210.63.217

  digitalWrite(LED_PIN, HIGH);
  Serial.println("Setup selesai. Mulai loop...");
  
  // Set tampilan default LCD
  tampilLCD("GATE MASUK SIAP", "SILAKAN MAJU");
}

//================================================
void loop() {
  //=====================
  // HEARTBEAT TIAP 5 DETIK
  //=====================
  if (millis() - lastHeartbeat > 5000) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(heartbeatUrl);
      int httpCode = http.POST("");
      
      Serial.print("Heartbeat: ");
      Serial.print(httpCode);
      if (httpCode == 200) {
        String payload = http.getString();
        Serial.println(" OK - " + payload);
      } else {
        Serial.println(" FAIL");
      }
      http.end();
    } else {
      Serial.println("WiFi PUTUS! Reconnect...");
      WiFi.reconnect();
    }
    lastHeartbeat = millis();
  }

  //=====================
  // DETEKSI MOBIL (MAAX 5 CM)
  //=====================
  long jarak = bacaUltrasonik();

  if (jarak > 0 && jarak <= 5) {
    if (!mobilAda) {
      mobilAda = true;
      Serial.println("Mobil terdeteksi. Tekan tombol untuk masuk.");
      tampilLCD("MOBIL TERDETEKSI", "TEKAN TOMBOL");
    }
  } else {
    // Jika tidak ada objek / mobil menjauh
    if (mobilAda) {
      mobilAda = false;
      Serial.println("Objek hilang. Menunggu mobil...");
      tampilLCD("GATE MASUK SIAP", "SILAKAN MAJU");
    }
  }

  //=====================
  // TOMBOL DITEKAN
  //=====================
  if (mobilAda && digitalRead(BUTTON_PIN) == LOW && millis() - lastTrigger > 3000) {
    lastTrigger = millis();
    prosesMasuk();
  }
  delay(100);
}

//================================================
long bacaUltrasonik() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long durasi = pulseIn(ECHO_PIN, HIGH, 30000);
  long jarak = durasi * 0.034 / 2;
  return jarak;
}

//================================================
void prosesMasuk() {
  digitalWrite(LED_PIN, LOW);
  Serial.println("Mengambil foto...");
  tampilLCD("MENGAMBIL FOTO..", "MOHON TUNGGU");
  camera_fb_t * dummy_fb = esp_camera_fb_get();
  if (dummy_fb) {
    esp_camera_fb_return(dummy_fb); // Langsung kembalikan/buang memori
  }
  delay(200); // Beri jeda sedikit untuk sensor stabil
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Gagal ambil foto");
    tampilLCD("GAGAL AMBIL FOTO", "COBA LAGI");
    delay(2000);
    digitalWrite(LED_PIN, HIGH);
    tampilLCD("MOBIL TERDETEKSI", "TEKAN TOMBOL");
    return;
  }

  Serial.print("Ukuran Foto: ");
  Serial.print(fb->len);
  Serial.println(" byte");

  String imageBase64 = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(25000);

  DynamicJsonDocument doc(200000);
  doc["image"] = imageBase64;
  String body;
  serializeJson(doc, body);

  Serial.println("Mengirim ke server...");
  tampilLCD("VERIFIKASI PLAT", "KE SERVER...");
  
  int httpCode = http.POST(body);

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("Response: " + response);

    DynamicJsonDocument resDoc(2048);
    DeserializationError error = deserializeJson(resDoc, response);

    if (!error) {
      String status = resDoc["status"];
      if (status == "success") {
        String ticketCode = resDoc["ticket_code"];
        String slotName = resDoc["slot_name"];
        
        Serial.println("====== MASUK SUKSES ======");
        Serial.println("Ticket: " + ticketCode);
        Serial.println("Slot: " + slotName);
        Serial.println("==========================");
        
        tampilLCD("MASUK SUKSES", "SLOT: " + slotName);
        bukaPalang();
        delay(5000);
        tutupPalang();
        
      } else if (status == "full") {
        Serial.println("PARKIR PENUH");
        tampilLCD("PARKIR PENUH", "MOHON MAAF");
        delay(3000);
      } else {
        Serial.println("PLAT TIDAK TERBACA");
        tampilLCD("PLAT TAK TERBACA", "COBA LAGI");
        delay(3000);
      }
    } else {
      Serial.println("JSON ERROR");
      tampilLCD("SERVER ERROR", "DATA INVALID");
      delay(3000);
    }
  } else {
    Serial.print("HTTP ERROR: ");
    Serial.println(httpCode);
    tampilLCD("KONEKSI GAGAL", "HTTP: " + String(httpCode));
    delay(3000);
  }

  http.end();
  digitalWrite(LED_PIN, HIGH);
  
  // Kembalikan status LCD sesuai ada tidaknya mobil
  if (bacaUltrasonik() <= 5) {
    tampilLCD("MOBIL TERDETEKSI", "TEKAN TOMBOL");
  } else {
    tampilLCD("GATE MASUK SIAP", "SILAKAN MAJU");
  }
}

//================================================
void bukaPalang() {
  palangServo.write(90);
  Serial.println("PALANG DIBUKA");
}

//================================================
void tutupPalang() {
  palangServo.write(0);
  Serial.println("PALANG DITUTUP");
}