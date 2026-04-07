#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- CONFIGURACIÓN DE HARDWARE ---
LiquidCrystal_I2C lcd(0x3F, 16, 2);

// Pines del Sensor Ultrasonido
const int trigPin = 19;
const int echoPin = 18;

// Pines de Actuadores
const int LED_VERDE = 2;
const int LED_AMARILLO = 4;
const int LED_ROJO = 5;
const int PIN_BUZZER = 13;

// Variables de Control
String nombreEstudiante = "Jean Spiegel";
bool sistemaActivado = true;

// --- FUNCIÓN PARA ACTUALIZAR PANTALLA ---
// La ponemos arriba para que el compilador la reconozca siempre
void actualizarPantalla(int distancia) {
  lcd.setCursor(0, 0);
  lcd.print("ALUMNO: ");
  lcd.print("Jean S.  "); // Nombre corto para que quepa bien

  lcd.setCursor(0, 1);
  if (distancia < 15 && distancia > 1) {
    lcd.print("¡ALERTA!      ");
  } else {
    lcd.print("SIST. OK: ");
    lcd.print(distancia);
    lcd.print("cm ");
  }
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // Pines SDA y SCL del ESP32
  
  lcd.init();
  lcd.backlight();
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  lcd.print("Iniciando...");
  delay(1000);
  lcd.clear();
}

// --- LOOP ---
void loop() {
  long duracion;
  int distancia;

  // Disparo del sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Lectura del eco (con timeout de 30ms)
  duracion = pulseIn(echoPin, HIGH, 30000);
  distancia = duracion * 0.034 / 2;

  // Lógica de Alerta
  if (distancia < 15 && distancia > 1) {
    // ESTADO DE PELIGRO
    digitalWrite(LED_ROJO, HIGH);
    digitalWrite(PIN_BUZZER, HIGH);
    digitalWrite(LED_VERDE, LOW);
  } else {
    // ESTADO SEGURO
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(LED_VERDE, HIGH);
  }

  // Actualizar la pantalla con la distancia medida
  actualizarPantalla(distancia);

  delay(200); // Pequeña pausa para estabilidad
}