/*
  PROYECTO: Carrito Autónomo Evasor de Obstáculos
  HARDWARE: ESP32 + L298N + 2 Motores DC + Sensor HC-SR04
*/

// --- PINES DEL L298N (MOTORES) ---
const int IN1 = 27; // Motor Izquierdo Adelante
const int IN2 = 26; // Motor Izquierdo Atrás
const int IN3 = 25; // Motor Derecho Adelante
const int IN4 = 33; // Motor Derecho Atrás

// --- PINES DEL SENSOR ULTRASÓNICO ---
const int TRIG = 5;
const int ECHO = 18;

// --- CONFIGURACIÓN DE SEGURIDAD ---
const int DISTANCIA_MINIMA = 15; // Centímetros para reaccionar al choque

void setup() {
  Serial.begin(115200);

  // Configurar pines de los motores como salida
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Configurar pines del sensor
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // Asegurarnos de que los motores empiecen apagados por seguridad
  detenerMotores();
  
  Serial.println("--- Carrito Autónomo Iniciado ---");
  delay(3000); // Te da 3 segundos para ponerlo en el piso antes de que arranque
}

void loop() {
  // 1. OBSERVAR: Medimos a qué distancia está la pared
  int distancia = medirDistancia();
  
  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.println(" cm");

  // 2. PENSAR Y ACTUAR: Tomamos decisiones según la distancia
  if (distancia > DISTANCIA_MINIMA && distancia > 0) {
    // Si la distancia es mayor a 15cm y es una lectura válida, avanzamos
    avanzar();
  } 
  else {
    // ¡Obstáculo detectado a menos de 15cm!
    detenerMotores();
    delay(300); // Pausa rápida para no dañar los motores por el cambio brusco
    
    retroceder();
    delay(400); // Da reversa por casi medio segundo
    
    girarDerecha();
    delay(500); // Gira como un tanque por medio segundo (ajusta esto si gira mucho o poco)
    
    detenerMotores();
    delay(200); // Pausa antes de volver a escanear
  }

  delay(50); // Pequeña pausa para no saturar al sensor
}

// ================= FUNCIONES DE MOVIMIENTO =================

void avanzar() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void retroceder() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void girarDerecha() {
  // Para girar rápido como un tanque: la llanta izquierda avanza y la derecha retrocede
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void detenerMotores() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ================= FUNCIÓN DEL SENSOR =================

int medirDistancia() {
  // Limpiamos el sonido residual
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  
  // Disparamos un pulso de sonido por 10 microsegundos
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  
  // Medimos cuánto tiempo tarda en regresar el eco
  // El "30000" es un tiempo límite para que no se quede trabado si no hay pared
  long duracion = pulseIn(ECHO, HIGH, 30000); 
  
  // Convertimos el tiempo a centímetros (magia física)
  int distanciaCalculada = duracion * 0.034 / 2;
  
  return distanciaCalculada;
}