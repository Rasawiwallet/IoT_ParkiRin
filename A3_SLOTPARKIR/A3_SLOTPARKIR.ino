#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "Base64.h"
#include <ArduinoJson.h>

// 1. GANTI INI KALAU BERUBAH
const char* ssid = "STARLINK TECNO";
const char* password = "123456789";
const char* serverUrl = "http://10.210.63.171:8000/api/slot-image";
const char* heartbeatUrl = "http://10.210.63.171:8000/api/device/heartbeat";
const char* area_name = "lantai1";

// 2. PIN KAMERA ESP32 WROVER DEV
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    21
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      19
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM      5
#define Y2_GPIO_NUM      4
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

#define LED_PIN 2 
#define BUZZER_PIN 32 

unsigned long lastCapture = 0;
const long interval = 5000; // 5 detik
unsigned long lastHeartbeat = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Init Kamera
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

  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x", err);
    blinkError();
    return;
  }

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

  // Konek WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  digitalWrite(LED_PIN, LOW);
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  beepOK(); // Beep 1x pas boot sukses
}

void loop() {
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("WiFi lost. Reconnecting...");
    WiFi.reconnect();
    delay(5000);
    return;
  }

  if (millis() - lastHeartbeat > 5000) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }

  if (millis() - lastCapture > interval) {
    sendImageToServer();
    lastCapture = millis();
  }
}

void sendHeartbeat() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(heartbeatUrl);
    http.setTimeout(3000);
    int httpCode = http.POST("");
    if (httpCode > 0) {
      Serial.println("Heartbeat: " + String(httpCode));
    }
    http.end();
  }
}

void sendImageToServer() {
  digitalWrite(LED_PIN, HIGH);
  
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    blinkError();
    digitalWrite(LED_PIN, LOW);
    return;
  }

  Serial.printf("Foto diambil. Size: %d bytes\n", fb->len);
  String imageBase64 = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);

  String httpRequestData = "{\"image\":\"" + imageBase64 + "\",\"area_name\":\"" + area_name + "\"}";
  int httpResponseCode = http.POST(httpRequestData);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("HTTP Response: ");
    Serial.println(httpResponseCode);
    Serial.println(response);

    // CEK RESPONSE DARI SERVER
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    
    String status = doc["status"]; // "ok", "wrong_slot", "mismatch"
    
    if(status == "wrong_slot" || status == "mismatch"){
      Serial.println("SALAH PARKIR! ALARM!");
      beepSalahParkir(); // BUNYI TUTUTUTUTUTUTUTTTTTT
    }
    
  } else {
    Serial.print("Error kirim POST: ");
    Serial.println(httpResponseCode);
    blinkError();
  }
  
  http.end();
  digitalWrite(LED_PIN, LOW);
}

// BUZZER SALAH PARKIR: tutututututututtttt
void beepSalahParkir(){
  for(int i=0; i<10; i++){ // tututututu 10x cepet
    digitalWrite(BUZZER_PIN, HIGH);
    delay(80);
    digitalWrite(BUZZER_PIN, LOW);
    delay(80);
  }
  digitalWrite(BUZZER_PIN, HIGH); // tututtttt panjang
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
}

void beepOK(){
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}

void blinkError() {
  for(int i=0; i<6; i++){
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}