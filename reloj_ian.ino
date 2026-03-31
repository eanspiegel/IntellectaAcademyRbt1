/*
  PROYECTO: Reloj de Internet (NTP) con LCD I2C
  HARDWARE: ESP32 + LCD 16x2 I2C
  ACADEMIA: Intellecta Academy
*/

#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include "time.h"

const char* ssid = "INTELLECTA ACADEMY";
const char* password = "PEDRO12345";

LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600; // GMT-5 (Hora de Ecuador)
const int daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200); // FUNDAMENTAL para saber qué pasa en el ESP32
  
  lcd.init();
  lcd.backlight();
  
  lcd.setCursor(0,0);
  lcd.print("Conectando WiFi");
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // Intentamos conectar por 10 segundos máximo (20 intentos de 500ms)
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.print("Fallo el WiFi");
    Serial.println("\nError: No se pudo conectar al WiFi. Revisa SSID/Pass.");
    return; // Detenemos la ejecución si no hay WiFi
  }

  lcd.clear();
  lcd.print("WiFi OK!");
  Serial.println("\nWiFi conectado con éxito.");

  // Solicitamos la hora al servidor NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  lcd.setCursor(0,1);
  lcd.print("Sincronizando...");
  Serial.println("Solicitando hora a pool.ntp.org...");
  
  // Damos un pequeño margen para que el paquete de internet viaje y vuelva
  delay(2000); 
  lcd.clear();
}

void loop() {
  // Si se cayó el WiFi, evitamos que la pantalla se vuelva loca
  if (WiFi.status() != WL_CONNECTED) {
    return; 
  }

  struct tm timeinfo;
  
  if(!getLocalTime(&timeinfo)){
    lcd.setCursor(0,0);
    lcd.print("Error NTP       "); // Espacios para limpiar caracteres basura
    Serial.println("Fallo al obtener la hora. (¿Firewall bloqueando puerto 123?)");
    delay(1000);
    return;
  }

  // MÉTODO A PRUEBA DE BALAS: Creamos "cajas" de memoria para el texto
  char bufferHora[17];
  char bufferFecha[17];

  // Llenamos las cajas con el formato exacto usando sprintf
  sprintf(bufferHora, "Hora: %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  sprintf(bufferFecha, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

  // Imprimimos la caja de texto directamente en el LCD
  lcd.setCursor(0,0);
  lcd.print(bufferHora);
  
  lcd.setCursor(0,1);
  lcd.print(bufferFecha);

  // También lo enviamos al Monitor Serie para doble confirmación
  Serial.println(bufferHora);

  delay(1000); // Actualizamos cada segundo
}