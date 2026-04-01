/*
  PROYECTO: Tira RGB Inteligente (8 LEDs) con Control IR - Modos Dinámicos
  HARDWARE: ESP32 + Receptor IR (Pin 15) + Tira WS2812B (Pin 12)
  ACADEMIA: Intellecta Academy
*/

#define DECODE_NEC 
#include <IRremote.hpp>
#include <Adafruit_NeoPixel.h> 

// --- PINES Y CONFIGURACIÓN ---
const int PIN_RECEPTOR = 15; 
const int PIN_LEDS = 12;     
const int NUM_LEDS = 8;      

// Creamos el objeto de la tira LED
Adafruit_NeoPixel tira(NUM_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);

int brilloActual = 100; // Brillo inicial 
int modoActual = 1;     // Empezamos en Rojo (Modo 1)
unsigned long tiempoAnterior = 0;
int hueArcoiris = 0;    // Variable para la animación del arcoíris

void setup() {
  Serial.begin(115200);
  
  // Iniciamos el receptor IR
  IrReceiver.begin(PIN_RECEPTOR, ENABLE_LED_FEEDBACK);
  
  // Iniciamos la tira LED
  tira.begin();
  tira.setBrightness(brilloActual);
  tira.show(); 
  
  Serial.println("--- Sistema RGB Inteligente V2 Listo ---");
}

// Función para pintar toda la tira de un solo color
void pintarColor(int r, int g, int b) {
  for(int i = 0; i < NUM_LEDS; i++) {
    tira.setPixelColor(i, tira.Color(r, g, b));
  }
  tira.show();
}

void loop() {
  // --- 1. SECCIÓN DE LECTURA DEL CONTROL REMOTO ---
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.decodedRawData != 0) {
      uint32_t codigoIR = IrReceiver.decodedIRData.decodedRawData;

      // Botón Play/Pausa: Apagar
      if (codigoIR == 0xBB44FF00) { modoActual = -1; } 
      
      // Controles de Brillo
      else if (codigoIR == 0xF609FF00) { 
        brilloActual = min(255, brilloActual + 25); 
        tira.setBrightness(brilloActual); 
        tira.show(); 
      }
      else if (codigoIR == 0xEA15FF00) { 
        brilloActual = max(0, brilloActual - 25); 
        tira.setBrightness(brilloActual); 
        tira.show(); 
      }
      
      // --- NAVEGACIÓN DE MENÚS (<< y >>) ---
      else if (codigoIR == 0xBC43FF00) { // AVANZAR (>>)
        modoActual++;
        if (modoActual > 9) modoActual = 0; // Si pasa de 9, vuelve a 0
      }
      else if (codigoIR == 0xBF40FF00) { // RETROCEDER (<<)
        modoActual--;
        if (modoActual < 0) modoActual = 9; // Si baja de 0, salta a 9
      }

      // --- SELECCIÓN DIRECTA (0 al 9) ---
      else if (codigoIR == 0xE916FF00) { modoActual = 0; } // Blanco
      else if (codigoIR == 0xF30CFF00) { modoActual = 1; } // Rojo
      else if (codigoIR == 0xE718FF00) { modoActual = 2; } // Verde
      else if (codigoIR == 0xA15EFF00) { modoActual = 3; } // Azul
      else if (codigoIR == 0xF708FF00) { modoActual = 4; } // Amarillo
      else if (codigoIR == 0xE31CFF00) { modoActual = 5; } // Magenta
      else if (codigoIR == 0xA55AFF00) { modoActual = 6; } // Cian
      else if (codigoIR == 0xBD42FF00) { modoActual = 7; } // Naranja
      else if (codigoIR == 0xAD52FF00) { modoActual = 8; } // Arcoíris
      else if (codigoIR == 0xB54AFF00) { modoActual = 9; } // Aleatorio
    }
    IrReceiver.resume();
  }

  // --- 2. SECCIÓN DE EJECUCIÓN CONTINUA DE LUCES ---
  if (modoActual == -1) {
    tira.clear();
    tira.show();
  }
  else if (modoActual == 0) { pintarColor(255, 255, 255); } 
  else if (modoActual == 1) { pintarColor(255, 0, 0); }     
  else if (modoActual == 2) { pintarColor(0, 255, 0); }     
  else if (modoActual == 3) { pintarColor(0, 0, 255); }     
  else if (modoActual == 4) { pintarColor(255, 255, 0); }   
  else if (modoActual == 5) { pintarColor(255, 0, 255); }   
  else if (modoActual == 6) { pintarColor(0, 255, 255); }   
  else if (modoActual == 7) { pintarColor(255, 128, 0); }   
  else if (modoActual == 8) {
    // EFECTO ARCOÍRIS FLUIDO
    for(int i = 0; i < NUM_LEDS; i++) {
      int pixelHue = hueArcoiris + (i * 65536L / NUM_LEDS);
      tira.setPixelColor(i, tira.gamma32(tira.ColorHSV(pixelHue)));
    }
    tira.show();
    hueArcoiris += 256; // Velocidad del arcoíris
  }
  else if (modoActual == 9) {
    // CAMBIO ALEATORIO CADA 1 SEGUNDO (1000 ms)
    if (millis() - tiempoAnterior >= 1000) {
      tiempoAnterior = millis();
      pintarColor(random(256), random(256), random(256));
    }
  }

  // Pequeña pausa de 15ms. Es vital para que la animación del arcoíris 
  // no corra a la velocidad de la luz y el ESP32 no colapse.
  delay(15); 
}