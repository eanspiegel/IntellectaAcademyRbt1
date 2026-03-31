/*
  PROYECTO: Sintetizador Digital V2 (Con Diagnóstico Serial)
  HARDWARE: ESP32 + 7 Botones + Amplificador PAM8403
  ACADEMIA: Intellecta Academy
*/

// --- PINES DE LOS BOTONES ---
const int pines[7] = { 4, 17, 23, 32, 33, 16, 13};

// --- FRECUENCIAS Y NOMBRES ---
const int notas[7] = {262, 294, 330, 349, 392, 440, 494};
const String nombres[7] = {"DO", "RE", "MI", "FA", "SOL", "LA", "SI"};

const int PIN_AUDIO = 25; 
const int CANAL_PWM = 0; 

void setup() {
  Serial.begin(115200);
  
  ledcSetup(CANAL_PWM, 2000, 8); 
  ledcAttachPin(PIN_AUDIO, CANAL_PWM); 

  for (int i = 0; i < 7; i++) {
    pinMode(pines[i], INPUT_PULLUP);
  }

  Serial.println("-----------------------------------");
  Serial.println("🎹 PIANO INICIADO - MODO DIAGNÓSTICO");
  Serial.println("-----------------------------------");
}

void loop() {
  bool hayTeclaPresionada = false; 

  for (int i = 0; i < 7; i++) {
    if (digitalRead(pines[i]) == LOW) { 
      
      ledcWriteTone(CANAL_PWM, notas[i]); 
      
      // --- REPORTE SERIAL ---
      Serial.print("✅ Botón en PIN ");
      Serial.print(pines[i]);
      Serial.print(" detectado | Intentando tocar: ");
      Serial.println(nombres[i]);
      
      hayTeclaPresionada = true; 
      break; 
    }
  }

  if (!hayTeclaPresionada) {
    ledcWriteTone(CANAL_PWM, 0); 
  }

  delay(50); // Pausa un poquito más larga para poder leer el texto en pantalla
}