/*
  PROYECTO: Carrito Autónomo (Versión ESP32 Clásico)
  OBJETIVO: Evasión de obstáculos usando HC-SR04
  ACADEMIA: Intellecta Academy
*/

// --- PINES DE LOS MOTORES (L298N) ---
const int IN1 = 27; // Izquierda Adelante
const int IN2 = 26; // Izquierda Atrás
const int IN3 = 25; // Derecha Adelante
const int IN4 = 33; // Derecha Atrás

// --- PINES DEL SENSOR (HC-SR04) ---
const int TRIG = 5;
const int ECHO = 18;

// --- DISTANCIA DE PELIGRO ---
const int UMBRAL_DISTANCIA = 20; // Reacciona si ve algo a menos de 20cm

void setup() {
  Serial.begin(115200);
  
  // Configurar motores
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  
  // Configurar sensor
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // Iniciar motores apagados por seguridad
  detener();
  
  Serial.println("--- ESP32 Clásico Iniciado ---");
  Serial.println("Calibrando y arrancando en 3 segundos...");
  delay(3000); 
}

void loop() {
  // 1. Escanear el entorno
  int distancia = obtenerDistancia();
  
  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.println(" cm");

  // 2. Tomar decisión
  if (distancia > UMBRAL_DISTANCIA || distancia == 0) {
    // Camino libre
    avanzar();
  } 
  else {
    // ¡Obstáculo cerca! Rutina de escape
    detener();
    delay(200);
    
    retroceder();
    delay(400); // Retrocede un poco
    
    detener();
    delay(200);
    
    girarDerecha();
    delay(500); // Gira para buscar otra ruta
    
    detener();
    delay(200);
  }
  
  delay(50); // Estabilidad para el sensor
}

// ================= FUNCIONES DEL ROBOT =================

int obtenerDistancia() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  
  long duracion = pulseIn(ECHO, HIGH, 26000); // Timeout de seguridad
  int cm = duracion * 0.034 / 2;
  
  return cm;
}

void avanzar() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void retroceder() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void girarDerecha() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}

void detener() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}