/*
  esp32_7seg.ino
  ====================
  Sketch optimizado para ESP32-CAM (AI-Thinker o compatible).
*/

#include "esp_camera.h"
#include "esp_http_server.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Librerías para deshabilitar el Brownout Detector
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// =================================================================== //
//  CONFIGURACION DE RED Y SERVIDOR
// =================================================================== //

const char* WIFI_SSID     = "INTELLECTA ACADEMY"; 
const char* WIFI_PASSWORD = "PEDRO12345";

const char* SERVER_IP   = "192.168.1.58";
const int   SERVER_PORT = 5001;

const char* DEVICE_NAME = "emanuel";  
const int   CAM_PORT    = 80;

// Variables globales para no reconstruir Strings en cada ciclo (Evita fragmentación)
String urlRegister;
String urlStatus;

// =================================================================== //
//  CONFIGURACION DEL DISPLAY 7 SEGMENTOS
// =================================================================== //
const int SEG_PINS[7] = {12, 13, 14, 15, 2, 4, 3}; 
const bool COMMON_ANODE = false; 

const byte LETTER_PATTERNS[26] = {
  0b01110111, // A
  0b01111100, // b
  0b00111001, // C
  0b01011110, // d
  0b01111001, // E
  0b01110001, // F
  0b00111101, // G
  0b01110110, // H
  0b00110000, // I
  0b00001110, // J
  0b01110101, // K
  0b00111000, // L
  0b00010101, // m 
  0b01010100, // n
  0b00111111, // O
  0b01110011, // P
  0b01100111, // q
  0b01010000, // r
  0b01101101, // S
  0b01111000, // t
  0b00111110, // U
  0b00011100, // v 
  0b00101010, // W 
  0b01001001, // X 
  0b01101110, // y
  0b01011011  // Z
};

// ------------------------------------------------------------------- //
//  Pinout camara AI-Thinker ESP32-CAM
// ------------------------------------------------------------------- //
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

httpd_handle_t  stream_httpd = NULL;
String lastGesture = "";

// ===================================================================
//  Control del 7 Segmentos
// ===================================================================

void init7Seg() {
  for (int i=0; i<7; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
    digitalWrite(SEG_PINS[i], COMMON_ANODE ? HIGH : LOW);
  }
}

void write7Seg(byte pattern) {
  for (int i=0; i<7; i++) {
    bool bitVal = bitRead(pattern, i);
    if (COMMON_ANODE) bitVal = !bitVal; 
    digitalWrite(SEG_PINS[i], bitVal ? HIGH : LOW);
  }
}

void mostrar_letra(String letraStr) {
  if (letraStr.length() > 0 && letraStr != "ninguno") {
    char c = letraStr.charAt(0);
    c = toupper(c);
    
    if (letraStr.length() == 1 && c >= 'A' && c <= 'Z') {
      byte pattern = LETTER_PATTERNS[c - 'A'];
      write7Seg(pattern);
      return;
    }
  }
}

const byte DIGIT_5 = 0b01101101; 
const byte DIGIT_8 = 0b01111111; 

// ===================================================================
//  Servidor de camara MJPEG
// ===================================================================

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

esp_err_t stream_handler(httpd_req_t* req) {
  camera_fb_t* fb    = NULL;
  esp_err_t    res   = ESP_OK;
  char         part_buf[64];

  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[WARN] No frame");
      res = ESP_FAIL;
      break;
    }

    httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
    httpd_resp_send_chunk(req, part_buf, hlen);
    httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);

    esp_camera_fb_return(fb);
    fb = NULL;

    if (res != ESP_OK) break;
  }
  return res;
}

void start_camera_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port    = CAM_PORT;

  httpd_uri_t stream_uri = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = stream_handler,
    .user_ctx = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("[CAM] Servidor de stream iniciado en /stream");
  } else {
    Serial.println("[ERR] No se pudo iniciar el servidor de stream");
  }
}

bool init_camera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size   = FRAMESIZE_VGA;
  config.jpeg_quality = 12; // Si se sigue congelando, súbelo a 15 o baja a FRAMESIZE_QVGA   
  config.fb_count     = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERR] Camara no iniciada: 0x%x\n", err);
    return false;
  }
  return true;
}

// ===================================================================
//  Utilidades de Red
// ===================================================================

String urlEncode(const String& s) {
  String out = "";
  for (int i = 0; i < (int)s.length(); i++) {
    if (s[i] == ' ')  out += "%20";
    else              out += s[i];
  }
  return out;
}

void buildUrls() {
  urlRegister = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/register";
  urlStatus = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/status/" + urlEncode(String(DEVICE_NAME));
}

bool do_register() {
  HTTPClient http;
  
  DynamicJsonDocument doc(128);
  doc["name"] = DEVICE_NAME;
  doc["ip"]   = WiFi.localIP().toString();
  doc["port"] = CAM_PORT;

  String body;
  serializeJson(doc, body);

  http.begin(urlRegister);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(2000);
  int code = http.POST(body);
  http.end();

  if (code == 200) {
    Serial.printf("[REG] '%s' registrado.\n", DEVICE_NAME);
    return true;
  } else {
    Serial.printf("[WARN] Registro fallido HTTP %d\n", code);
    return false;
  }
}

String fetchGesture() {
  HTTPClient http;
  http.begin(urlStatus);
  http.setTimeout(500);
  int code = http.GET();

  String gesture = "";
  if (code == 200) {
    StaticJsonDocument<128> doc;
    // OPTIMIZACION: Parsear directamente desde el stream HTTP ahorra muchísima memoria RAM
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (!error) {
      gesture = doc["gesture"].as<String>();
    }
  }
  http.end();
  return gesture;
}

// ===================================================================
//  Setup
// ===================================================================

void setup() {
  // Deshabilitar el detector de caídas de voltaje (Brownout)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 

  Serial.begin(115200);
  delay(500);

  init7Seg();
  
  Serial.println("[7SEG] TEST HARDWARE: MOSTRANDO 8 y 5");
  write7Seg(DIGIT_8);
  delay(1000);
  write7Seg(DIGIT_5);
  delay(1500);
  write7Seg(0x00); 

  if (!init_camera()) {
    Serial.println("[ERR] FATAL: camara no disponible.");
    while (true) delay(1000);
  }
  Serial.println("[CAM] Camara inicializada.");

  Serial.printf("Conectando a %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Conectado!");

  // Pre-construir URLs
  buildUrls();

  start_camera_server();
  delay(500);
  do_register();
}

// ===================================================================
//  Loop
// ===================================================================

unsigned long lastRegister = 0;
unsigned long lastPoll     = 0;

void loop() {
  unsigned long now = millis();

  if (now - lastRegister > 5000) {
    lastRegister = now;
    if (WiFi.status() == WL_CONNECTED) {
      do_register();
    } else {
      Serial.println("[WARN] WiFi caido. Reconectando...");
      WiFi.reconnect();
    }
  }

  // OPTIMIZACION: Aumentado de 200ms a 500ms para no asfixiar el módulo WiFi
  if (now - lastPoll > 500) {
    lastPoll = now;
    String gesture = fetchGesture();
    
    if (gesture != "" && gesture != lastGesture) {
      lastGesture = gesture;
      Serial.printf("[LETRA] %s\n", gesture.c_str());
      mostrar_letra(gesture);
    }
  }

  delay(10);
}