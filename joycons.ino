#include <BleGamepad.h>

BleGamepad bleGamepad("Intellecta_ian", "Cesar", 100);

void setup() {
  Serial.begin(115200);
  
  // Configuración de los 4 botones (INPUT_PULLUP para no usar resistencias)
  pinMode(13, INPUT_PULLUP); // Botón A
  pinMode(12, INPUT_PULLUP); // Botón B
  pinMode(14, INPUT_PULLUP); // Botón X
  pinMode(27, INPUT_PULLUP); // Botón Y
  
  bleGamepad.begin();
  Serial.println(">>> Control Intellecta 4 Botones - ¡LISTO! <<<");
}

void loop() {
  int xRaw = analogRead(32);
  int yRaw = analogRead(33);

  // Lectura de botones (0 = Presionado, 1 = Suelto)
  int b1 = digitalRead(13);
  int b2 = digitalRead(12);
  int b3 = digitalRead(14);
  int b4 = digitalRead(27);

  if (bleGamepad.isConnected()) {
    // Mapeo de Joystick
    int xFinal = map(xRaw, 0, 4095, -32767, 32767);
    int yFinal = map(yRaw, 0, 4095, 32767, -32767);
    bleGamepad.setAxes(xFinal, yFinal);

    // Lógica de Botones (Se presionan cuando el pin detecta 0)
    !b1 ? bleGamepad.press(BUTTON_1) : bleGamepad.release(BUTTON_1);
    !b2 ? bleGamepad.press(BUTTON_2) : bleGamepad.release(BUTTON_2);
    !b3 ? bleGamepad.press(BUTTON_3) : bleGamepad.release(BUTTON_3);
    !b4 ? bleGamepad.press(BUTTON_4) : bleGamepad.release(BUTTON_4);

    bleGamepad.sendReport();
  }

  // --- MONITOR SERIAL PARA EL PROFE ---
  Serial.print("Stick [X:"); Serial.print(xRaw);
  Serial.print(" Y:"); Serial.print(yRaw);
  Serial.print("] | Botones [A:"); Serial.print(!b1);
  Serial.print(" B:"); Serial.print(!b2);
  Serial.print(" X:"); Serial.print(!b3);
  Serial.print(" Y:"); Serial.print(!b4);
  Serial.println("]");

  delay(50);
}