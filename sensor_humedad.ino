// Definimos los pines
const int pinMotor = 26; // Pin que enviará la señal al transistor
const int pinBoton = 18; // Pin del botón

void setup() {
  Serial.begin(115200);
  
  // Configuramos el pin del motor como salida
  pinMode(pinMotor, OUTPUT);
  
  // Usamos la resistencia interna del ESP32 para el botón
  pinMode(pinBoton, INPUT_PULLUP); 

  // Nos aseguramos de que el motor empiece apagado
  digitalWrite(pinMotor, LOW); 
  Serial.println("Sistema de motor listo.");
}

void loop() {
  // Leemos el estado del botón
  int estadoBoton = digitalRead(pinBoton);

  // Como usamos INPUT_PULLUP, LOW significa que el botón está presionado
  if (estadoBoton == LOW) { 
    digitalWrite(pinMotor, HIGH); // Enciende el motor
    Serial.println("¡Acelerando!");
  } else {
    digitalWrite(pinMotor, LOW);  // Apaga el motor
  }
  
  delay(10); // Pequeña pausa para dar estabilidad a la lectura
}