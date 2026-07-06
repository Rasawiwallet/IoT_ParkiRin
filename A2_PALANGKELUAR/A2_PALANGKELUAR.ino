#include <WiFi.h>
#include <HTTPClient.h>
// #include "esp_camera.h"   // KAMERA DINONAKTIFKAN
// #include "Base64.h"       // KAMERA DINONAKTIFKAN
#include <ESP32Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WebServer.h>

WebServer server(80);

const char* ssid = "STARLINK TECNO";
const char* password = "123456789";
const char* serverGateOut = "http://10.210.63.171:8000/api/gate-out";
const char* serverConfirmPay = "http://10.210.63.171:8000/api/confirm-payment";
const char* heartbeatUrl = "http://10.210.63.171:8000/api/device/heartbeat";

// ================= PINOUT AMAN =================
#define TRIG_PIN 14
#define ECHO_PIN 12
#define SERVO_PIN 2

#define SS_PIN 5
#define RST_PIN 23
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 13

#define OLED_SDA 32
#define OLED_SCL 33
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===============================================
// KAMERA PIN WROVER DEV (DINONAKTIFKAN)
// ===============================================
/*
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
*/

// Scanner QR Fisik - GA DIPAKE TAPI BIARIN AJA
HardwareSerial scannerSerial(2);
#define SCANNER_RX 15
#define SCANNER_TX -1

Servo palangServo;
MFRC522 rfid(SS_PIN, RST_PIN);

String ticketCodeDariScanner = "";
long currentFee = 0;
bool waitingRFID = false;
bool mobilAda = false;
bool ticketReady = false;
unsigned long lastHeartbeat = 0;

void tampilOLED(String baris1, String baris2 = ""){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println(baris1);
  if(baris2!= ""){
    display.setCursor(0,16);
    display.println(baris2);
  }
  display.display();
}

void tutupPalang(){ palangServo.write(0); }
void bukaPalang(){ palangServo.write(90); }

void resetState(){
  ticketCodeDariScanner = "";
  currentFee = 0;
  waitingRFID = false;
  ticketReady = false;
  tampilOLED("Gate Keluar", "SIAP");
}

// FUNGSI INI TARO DI SINI, DI LUAR FUNGSI LAIN
void handleScanFromHP() {
  if (server.hasArg("ticket")) {
    String data = server.arg("ticket");
    data.trim();
    if(data.startsWith("PRK-") && data.length() == 10){
      ticketCodeDariScanner = data;
      ticketReady = true;
      Serial.println("Karcis dari HP OK: " + data);
      tampilOLED("Karcis OK", data);
      server.send(200, "text/plain", "OK: Tiket diterima");
    } else {
      Serial.println("Karcis dari HP Invalid: " + data);
      tampilOLED("Karcis Invalid");
      server.send(400, "text/plain", "Invalid Ticket");
    }
  } else {
    server.send(400, "text/plain", "Missing ticket param");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nBooting Gate Keluar...");

  scannerSerial.begin(9600, SERIAL_8N1, SCANNER_RX, SCANNER_TX);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED gagal");
    while(1);
  }
  tampilOLED("Booting...", "V1.0 Wrover");

  palangServo.attach(SERVO_PIN);
  tutupPalang();

  // ===============================================
  // INIT KAMERA (DINONAKTIFKAN)
  // ===============================================
  /*
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
    tampilOLED("CAMERA ERROR!", "RESTARTING...");
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
  */

  // ===============================================
  // LANJUTAN SETUP...
  // ===============================================

  tampilOLED("Connect WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status()!= WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // === INISIALISASI RFID KE SINI ===
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();
  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("RC522 Version: 0x");
  Serial.println(version, HEX);
  if (version == 0x00 || version == 0xFF) {
    Serial.println("WARNING: RFID Gagal Komunikasi!");
  } else {
    Serial.println("RFID Ready");
  }

  // DAFTARIN ENDPOINT CUMA 1X DI SINI
  server.on("/scan", HTTP_POST, handleScanFromHP);
  server.begin();
  Serial.println("HTTP Server started");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());

  tampilOLED("Gate Keluar", WiFi.localIP().toString());
  Serial.println("Gate Keluar SIAP.");

  delay(2000);
  resetState();
}

void loop() {
  server.handleClient(); // WAJIB ADA

  if (millis() - lastHeartbeat > 5000) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }

  bacaScannerQR(); // Biarin aja, buat jaga2

  long jarak = bacaUltrasonik();

  if(jarak > 0 && jarak <= 5){
    if(!mobilAda){
      mobilAda = true;
      Serial.println(">>> Mobil Masuk Area Sensor (<= 5 cm) <<<");
      tampilOLED("Mobil Detected", "Scan via HP");
    }
  } else {
    if(mobilAda){
       Serial.println(">>> Mobil Keluar Area Sensor <<<");
       mobilAda = false;
       resetState();
    }
  }

  if(mobilAda && ticketReady &&!waitingRFID){
    verifikasiPlat();
  }

  if(mobilAda && waitingRFID){
    cekRFID();
  }

  delay(10);
}

void sendHeartbeat() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(heartbeatUrl);
    http.setTimeout(3000);
    int httpCode = http.POST("");
    http.end();
  }
}

void bacaScannerQR(){
  if(scannerSerial.available()){
    String data = scannerSerial.readStringUntil('\n');
    data.trim();
    if(data.startsWith("PRK-") && data.length() == 10){
      ticketCodeDariScanner = data;
      ticketReady = true;
      Serial.println("Karcis OK: " + data);
      tampilOLED("Karcis OK", data);
      delay(1000);
    } else {
      Serial.println("Karcis Invalid: " + data);
      tampilOLED("Karcis Invalid");
      delay(1000);
      if(mobilAda) {
         tampilOLED("Mobil Detected", "Scan via HP");
      } else {
         resetState(); // HAPUS handleScanFromHP() DARI SINI
      }
    }
  }
}

long bacaUltrasonik(){
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long durasi = pulseIn(ECHO_PIN, HIGH, 30000);
  return durasi * 0.034 / 2;
}

void verifikasiPlat(){
  tampilOLED("Verifikasi...", "Cek Data Server");
  Serial.println("Memulai proses verifikasi tiket (Tanpa Kamera)...");
  
  // ===============================================
  // PROSES AMBIL FOTO DINONAKTIFKAN
  // ===============================================
  /*
  camera_fb_t * dummy_fb = esp_camera_fb_get();
  if (dummy_fb) {
    esp_camera_fb_return(dummy_fb); // Langsung kembalikan/buang memori
  }
  delay(200); // Beri jeda sedikit untuk sensor stabil
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb){
    tampilOLED("Gagal Foto");
    Serial.println("ERROR: Gagal mengambil foto!");
    delay(2000);
    resetState();
    return;
  }
  String imageBase64 = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);
  */

  tampilOLED("Upload Server...");
  HTTPClient http;
  http.begin(serverGateOut);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(25000);

  // Ukuran dokumen dikecilkan karena gambar Base64 sudah tidak dikirim
  DynamicJsonDocument doc(1024); 
  doc["ticket_code"] = ticketCodeDariScanner;
  doc["image"] = "";
  // doc["image"] = imageBase64; // GAMBAR DINONAKTIFKAN
  
  String body;
  serializeJson(doc, body);

  int httpCode = http.POST(body);

  if(httpCode == 200){
    String response = http.getString();
    DynamicJsonDocument resDoc(1024);
    deserializeJson(resDoc, response);
    String status = resDoc["status"];

    if(status == "verified"){
      currentFee = resDoc["fee"];
      tampilOLED("TIKET COCOK", "Rp " + String(currentFee));
      Serial.println("Tiket Cocok! Menunggu kartu RFID...");
      delay(2000);
      tampilOLED("SILAKAN TAP", "KARTU RFID");
      waitingRFID = true;
      ticketReady = false;
    } else {
      tampilOLED("GAGAL!", "DATA SALAH");
      Serial.println("Verifikasi Gagal: Data Tidak Sesuai.");
      delay(3000);
      resetState();
    }
  } else {
    tampilOLED("Server Error", String(httpCode));
    Serial.println("Server Error HTTP: " + String(httpCode));
    delay(3000);
    resetState();
  }
  http.end();
}

void cekRFID(){
  if(!rfid.PICC_IsNewCardPresent() ||!rfid.PICC_ReadCardSerial()){
    return;
  }

  String uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uidString += String(rfid.uid.uidByte[i] < 0x10? "0" : "");
    uidString += String(rfid.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();

  Serial.println("Kartu Terdeteksi, UID: " + uidString);
  tampilOLED("UID: " + uidString, "Proses Bayar...");
  delay(1500);

  // Bebaskan kartu DULU sebelum mengirim request ke server
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  konfirmasiBayar();
  waitingRFID = false;
}

void konfirmasiBayar(){
  HTTPClient http;
  http.begin(serverConfirmPay);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(256);
  doc["ticket_code"] = ticketCodeDariScanner;
  String body;
  serializeJson(doc, body);

  int httpCode = http.POST(body);

  if(httpCode == 200){
    DynamicJsonDocument resDoc(512);
    deserializeJson(resDoc, http.getString());

    if(resDoc["status"] == "success"){
      tampilOLED("BAYAR SUKSES", "Palang Buka");
      Serial.println("Bayar SUKSES! Membuka Palang...");
      bukaPalang();
      delay(5000);
      tutupPalang();
      tampilOLED("Terima Kasih", "Hati-hati");
      delay(2000);
    } else {
      tampilOLED("BAYAR GAGAL", "Coba Lagi");
      Serial.println("Bayar GAGAL dari respon server.");
      delay(3000);
    }
  } else {
    tampilOLED("Server Error", "Bayar Gagal");
    Serial.println("Gagal konfirmasi bayar, HTTP: " + String(httpCode));
    delay(3000);
  }
  http.end();

  if(bacaUltrasonik() > 5){
    resetState();
  } else {
    tampilOLED("Mobil Detected", "Scan via HP");
  }
}