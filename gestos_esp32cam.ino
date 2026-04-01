/*
  gestos_esp32cam.ino
  ====================
  Sketch para ESP32-CAM (AI-Thinker o compatible).

  Funciones:
    1. Inicia el servidor de camara MJPEG en GET /stream
    2. Se registra en el servidor de gestos (server.py) via POST /register
    3. Hace heartbeat cada 5 s para mantener la conexion activa
    4. Hace polling de GET /status para leer el gesto detectado
    5. Ejecuta acciones segun el gesto (GPIO, motor, LED, etc.)

  Dependencias (Library Manager del Arduino IDE):
    - ArduinoJson  (Benoit Blanchon)
    (La camara y el servidor MJPEG usan las librerias del ESP32 integradas)

  Board: "AI Thinker ESP32-CAM"
  Flashear con FTDI o adaptador USB-Serial (GPIO0 a GND al grabar)
*/

#include "esp_camera.h"
#include "esp_http_server.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =================================================================== //
//  CONFIGURACION  --  editar antes de cargar
// =================================================================== //

const char* WIFI_SSID     = "INTELLECTA ACADEMY"; 
const char* WIFI_PASSWORD = "PEDRO12345";

// IP del PC donde corre server.py  (gestos/server.py)
const char* SERVER_IP   = "192.168.1.58";
const int   SERVER_PORT = 5001;

// Nombre único de este dispositivo  *** CAMBIAR EN CADA ESP32 ***
const char* DEVICE_NAME = "Gian";  
// Puerto del stream de esta camara
const int   CAM_PORT    = 80;

// ==== Configuracion de la Tira LED WS2812 ====
#include <Adafruit_NeoPixel.h>
const int PIN_LED_STRIP = 12;  // Pin de datos para la tira WS2812
const int NUM_LEDS      = 16;  // Cantidad de LEDs en la tira

Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);
int currentMode = 0;           // 0: Apagado, 1: Arcoiris, 2: Azul
int currentBrightness = 50;    // Brillo actual (0-255)
long firstPixelHue = 0;        // Para el efecto de arcoiris

// =================================================================== //


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


// ------------------------------------------------------------------- //
//  Variables globales
// ------------------------------------------------------------------- //
String lastGesture  = "";
httpd_handle_t  stream_httpd = NULL;


// ===================================================================
//  Acciones personalizables (Efectos Tira LED)
// ===================================================================

void actualizar_efecto_led() {
  if (currentMode == 0) {
    // ✊ Apagar
    strip.clear();
    strip.show();
  } else if (currentMode == 2) {
    // ✌️ Azul
    strip.fill(strip.Color(0, 0, 255));
    strip.show();
  } else if (currentMode == 1) {
    // ✋ Arcoiris (animado paso a paso para no bloquear)
    strip.rainbow(firstPixelHue);
    strip.show();
    firstPixelHue += 256;
    if (firstPixelHue >= 65536) {
      firstPixelHue = 0;
    }
  } else if (currentMode == 3) {
    // LIKE (THIAGO): Azul , celeste, verde, amarillo y naranja
    uint32_t c[] = {strip.Color(0,0,255), strip.Color(0,191,255), strip.Color(0,255,0), strip.Color(255,255,0), strip.Color(255,165,0)};
    for(int i=0; i<NUM_LEDS; i++) strip.setPixelColor(i, c[i % 5]);
    strip.show();
  } else if (currentMode == 4) {
    // SPIDER MAN (JP): azul y rojo
    uint32_t c[] = {strip.Color(0,0,255), strip.Color(255,0,0)};
    for(int i=0; i<NUM_LEDS; i++) strip.setPixelColor(i, c[i % 2]);
    strip.show();
  } else if (currentMode == 5) {
    // IAN: rojo amarillo y azul
    uint32_t c[] = {strip.Color(255,0,0), strip.Color(255,255,0), strip.Color(0,0,255)};
    for(int i=0; i<NUM_LEDS; i++) strip.setPixelColor(i, c[i % 3]);
    strip.show();
  } else if (currentMode == 6) {
    // DAVIET: azul, rojo, azul claro, rojo claro, naranja y negro
    uint32_t c[] = {strip.Color(0,0,255), strip.Color(255,0,0), strip.Color(135,206,250), strip.Color(255,102,102), strip.Color(255,165,0), strip.Color(0,0,0)};
    for(int i=0; i<NUM_LEDS; i++) strip.setPixelColor(i, c[i % 6]);
    strip.show();
  } else if (currentMode == 7) {
    // CESAR: Rojo naranja, azul, amarillo y verde
    uint32_t c[] = {strip.Color(255,0,0), strip.Color(255,165,0), strip.Color(0,0,255), strip.Color(255,255,0), strip.Color(0,255,0)};
    for(int i=0; i<NUM_LEDS; i++) strip.setPixelColor(i, c[i % 5]);
    strip.show();
  }
}

void accion_rainbow() {
  currentMode = 1;
  Serial.println("[ACCION] Arcoiris");
}

void accion_off() {
  currentMode = 0;
  strip.clear();
  strip.show();
  Serial.println("[ACCION] Apagar");
}

void accion_blue() {
  currentMode = 2;
  Serial.println("[ACCION] Azul");
}

void accion_like() {
  currentMode = 3;
  Serial.println("[ACCION] LIKE (Thiago)");
}

void accion_spiderman() {
  currentMode = 4;
  Serial.println("[ACCION] SPIDER MAN (JP)");
}

void accion_ian() {
  currentMode = 5;
  Serial.println("[ACCION] IAN");
}

void accion_daviet() {
  currentMode = 6;
  Serial.println("[ACCION] DAVIET");
}

void accion_cesar() {
  currentMode = 7;
  Serial.println("[ACCION] CESAR");
}


// ===================================================================
//  Servidor de camara MJPEG
// ===================================================================

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

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


// ===================================================================
//  Inicializacion de la camara
// ===================================================================

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

  // Resolucion: QVGA (320x240) para mayor fluidez en la red
  config.frame_size   = FRAMESIZE_VGA;
  config.jpeg_quality = 12;   // 0=maxima calidad, 63=minima
  config.fb_count     = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERR] Camara no iniciada: 0x%x\n", err);
    return false;
  }
  return true;
}


// ===================================================================
//  Registro en el servidor de gestos
// ===================================================================

bool do_register() {
  HTTPClient http;
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/register";

  DynamicJsonDocument doc(128);
  doc["name"] = DEVICE_NAME;             // <-- nombre único del dispositivo
  doc["ip"]   = WiFi.localIP().toString();
  doc["port"] = CAM_PORT;

  String body;
  serializeJson(doc, body);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(2000);
  int code = http.POST(body);
  http.end();

  if (code == 200) {
    Serial.printf("[REG] '%s' registrado en el servidor.\n", DEVICE_NAME);
    return true;
  } else {
    Serial.printf("[WARN] Registro fallido HTTP %d\n", code);
    return false;
  }
}


// ===================================================================
//  Utilidad: codificar espacios en URLs (%20)
// ===================================================================

String urlEncode(const String& s) {
  String out = "";
  for (int i = 0; i < (int)s.length(); i++) {
    if (s[i] == ' ')  out += "%20";
    else              out += s[i];
  }
  return out;
}


// ===================================================================
//  Polling de gestos
// ===================================================================

String fetchGesture() {
  HTTPClient http;
  // Consulta el gesto de ESTE dispositivo por su nombre (URL-encoded)
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT)
               + "/status/" + urlEncode(String(DEVICE_NAME));

  http.begin(url);
  http.setTimeout(500);
  int code = http.GET();

  String gesture = "";
  if (code == 200) {
    String body = http.getString();
    StaticJsonDocument<128> doc;
    if (!deserializeJson(doc, body)) {
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
  Serial.begin(115200);
  delay(500);

  // Configurar Tira LED
  strip.begin();
  strip.setBrightness(currentBrightness);
  strip.clear();
  strip.show();

  // Camara
  if (!init_camera()) {
    Serial.println("[ERR] FATAL: camara no disponible.");
    while (true) delay(1000);
  }
  Serial.println("[CAM] Camara inicializada.");

  // WiFi
  Serial.printf("Conectando a %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());

  // Servidor de stream
  start_camera_server();
  Serial.printf("[CAM] Stream en: http://%s/stream\n",
                WiFi.localIP().toString().c_str());

  // Registrar en el servidor de gestos
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

  // Heartbeat: re-registrar cada 5 s
  if (now - lastRegister > 5000) {
    lastRegister = now;
    if (WiFi.status() == WL_CONNECTED) {
      do_register();
    } else {
      Serial.println("[WARN] WiFi caido. Reconectando...");
      WiFi.reconnect();
    }
  }

  // Polling de gestos cada 200 ms
  if (now - lastPoll > 200) {
    lastPoll = now;
    String gesture = fetchGesture();
    if (gesture != "" && gesture != lastGesture) {
      lastGesture = gesture;
      Serial.printf("[GESTO] %s\n", gesture.c_str());
      if      (gesture == "rainbow")        accion_rainbow();
      else if (gesture == "off")            accion_off();
      else if (gesture == "blue")           accion_blue();
      else if (gesture == "like")           accion_like();
      else if (gesture == "spiderman")      accion_spiderman();
      else if (gesture == "ian")            accion_ian();
      else if (gesture == "daviet")         accion_daviet();
      else if (gesture == "cesar")          accion_cesar();
      // "ninguno" mantiene el efecto actual
    }
  }

  // Actualizar animacion de LED (no bloqueante)
  actualizar_efecto_led();
  delay(10); // pequeña pausa para suavidad
}
