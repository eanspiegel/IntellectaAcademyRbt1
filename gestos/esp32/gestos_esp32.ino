/*
  gestos_esp32.ino
  ================
  Cliente ESP32 para el servidor de deteccion de gestos.

  Flujo:
    1. Conecta al WiFi.
    2. Cada 200 ms hace GET http://<SERVER_IP>:5001/status
    3. Si gesture == "go"   -> LED_GO encendido, llama a accion_go()
    4. Si gesture == "stop" -> LED_STOP encendido, llama a accion_stop()

  Dependencias (instalar desde el Library Manager del IDE):
    - ArduinoJson  (Benoit Blanchon)
    - Adafruit NeoPixel (Adafruit)

  Pines por defecto:
    GPIO 12 -> Tira LED WS2812
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ------------------------------------------------------------------ //
//  CONFIGURACION  --  editar antes de cargar
// ------------------------------------------------------------------ //
const char* WIFI_SSID     = "INTELLECTA ACADEMY";
const char* WIFI_PASSWORD = "PEDRO12345";

// IP del PC donde corre server.py  (ver la salida del servidor)
const char* SERVER_IP   = "192.168.1.58";
const int   SERVER_PORT = 5001;

// Nombre del dispositivo del cual leer los gestos
const char* DEVICE_NAME = "Juan Pablo"; // Nombre del ESP32-CAM al que queremos seguir


// ==== Configuracion de la Tira LED WS2812 ====
#include <Adafruit_NeoPixel.h>
const int PIN_LED_STRIP = 12;  // Pin de datos para la tira WS2812
const int NUM_LEDS      = 16;  // Cantidad de LEDs en la tira

Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);
int currentMode = 0;           // 0: Apagado, 1: Arcoiris, 2: Azul
int currentBrightness = 50;    // Brillo actual (0-255)
long firstPixelHue = 0;        // Para el efecto de arcoiris

// Intervalo de consulta al servidor (ms)
const int POLL_INTERVAL_MS = 200;
// ------------------------------------------------------------------ //

String lastGesture = "";

// ======================================================
//  Acciones personalizables
//  Aqui agrega lo que quieres que haga el ESP32
// ======================================================

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


// ======================================================
//  Funciones de red
// ======================================================

// Utilidad: codificar espacios en URLs (%20)
String urlEncode(const String& s) {
  String out = "";
  for (int i = 0; i < (int)s.length(); i++) {
    if (s[i] == ' ')  out += "%20";
    else              out += s[i];
  }
  return out;
}

String fetchGesture() {
  HTTPClient http;
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) 
               + "/status/" + urlEncode(String(DEVICE_NAME));

  http.begin(url);
  http.setTimeout(500);   // timeout corto para no bloquear
  int code = http.GET();

  String gesture = "";

  if (code == 200) {
    String body = http.getString();

    // Parsear JSON: {"name": "Juan Pablo", "gesture": "spiderman"}
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (!err) {
      gesture = doc["gesture"].as<String>();
    }
  } else {
    Serial.printf("[WARN] HTTP %d\n", code);
  }

  http.end();
  return gesture;
}


// ======================================================
//  Setup
// ======================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  // Configurar Tira LED
  strip.begin();
  strip.setBrightness(currentBrightness);
  strip.clear();
  strip.show();

  // ---- Conexion WiFi ----
  Serial.printf("\nConectando a %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConectado!");
  Serial.printf("IP del ESP32: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("Servidor:     http://%s:%d/status\n", SERVER_IP, SERVER_PORT);
  Serial.println("Iniciando polling ...\n");
}


// ======================================================
//  Loop
// ======================================================

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERR] WiFi desconectado. Reconectando...");
    WiFi.reconnect();
    delay(2000);
    return;
  }

  String gesture = fetchGesture();

  if (gesture != lastGesture && gesture != "") {
    lastGesture = gesture;
    Serial.printf("Gesto: %s\n", gesture.c_str());

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

  // Actualizar animacion de LED (no bloqueante)
  actualizar_efecto_led();
  delay(10); // pequeña pausa para suavidad
}
