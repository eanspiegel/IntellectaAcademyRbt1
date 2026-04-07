#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Configuración LCD 16x2 I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- PINES CORREGIDOS PARA ESP32 CLÁSICO ---
#define I2C_SDA 21
#define I2C_SCL 22

// Controles (Pines 100% seguros)
const int BTN_RESTART = 14; // Botón Azul
const int BTN_UP = 25;      // Botón Verde Izquierdo
const int BTN_DOWN = 26;    // Botón Verde Derecho

// --- Diseño de Caracteres (Pixel Art para LCD) ---
byte carGraphic[8] = {
  0b00000,
  0b00000,
  0b01110, // Techo
  0b11111, // Cuerpo del auto
  0b01010, // Llantas
  0b00000,
  0b00000,
  0b00000
};

byte rockGraphic[8] = {
  0b00000,
  0b00100,
  0b01110,
  0b11111,
  0b11111,
  0b01110,
  0b00000,
  0b00000
};

// --- Variables del Juego ---
int playerRow = 0; // 0 = Carril Arriba, 1 = Carril Abajo
bool obstacles[2][16] = {false}; // Matriz que guarda dónde hay rocas

unsigned long lastMoveTime = 0;
int gameSpeed = 300; // Milisegundos entre fotogramas (más bajo = más rápido)
int score = 0;
bool isGameOver = false;

// --- Funciones ---

void setup() {
  // Inicializar I2C con los pines correctos
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  
  // Guardar los dibujos en la memoria del LCD
  lcd.createChar(0, carGraphic);
  lcd.createChar(1, rockGraphic);
  
  pinMode(BTN_RESTART, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  
  randomSeed(analogRead(0));
  
  // Pantalla de Inicio
  lcd.setCursor(0, 0);
  lcd.print(" CARRERA ESP32! ");
  lcd.setCursor(0, 1);
  lcd.print("Presiona el Azul");
  
  // Esperar a que el jugador presione el botón para empezar
  while(digitalRead(BTN_RESTART) == HIGH) {
    delay(10); 
  }
  lcd.clear();
}

void drawGame() {
  for(int row = 0; row < 2; row++) {
    lcd.setCursor(0, row);
    for(int col = 0; col < 16; col++) {
      // El auto siempre está en la columna 1
      if (col == 1 && row == playerRow) {
        lcd.write(0); // Dibuja el auto
      } else if (obstacles[row][col]) {
        lcd.write(1); // Dibuja la roca
      } else {
        lcd.print(" "); // Dibuja la calle vacía
      }
    }
  }
}

void loop() {
  if (isGameOver) {
    lcd.setCursor(0, 0);
    lcd.print("CHOQUE! Pts: ");
    lcd.print(score);
    lcd.print("  "); // Limpiar restos de texto
    lcd.setCursor(0, 1);
    lcd.print("Azul p/ Reiniciar");
    
    // Si presiona reiniciar, limpiar todo
    if (digitalRead(BTN_RESTART) == LOW) {
      isGameOver = false;
      score = 0;
      gameSpeed = 300;
      playerRow = 0;
      for(int r=0; r<2; r++) {
        for(int c=0; c<16; c++) obstacles[r][c] = false;
      }
      lcd.clear();
      delay(200); // Anti-rebote
    }
    return;
  }

  // --- Leer Controles del Jugador ---
  if (digitalRead(BTN_UP) == LOW) {
    playerRow = 0;
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    playerRow = 1;
  }

  // --- Motor Físico del Juego ---
  // Si ha pasado suficiente tiempo, mover el mundo un paso a la izquierda
  if (millis() - lastMoveTime > gameSpeed) {
    lastMoveTime = millis();
    
    // 1. Mover todas las rocas una columna hacia la izquierda
    for(int row = 0; row < 2; row++) {
      for(int col = 0; col < 15; col++) {
        obstacles[row][col] = obstacles[row][col+1];
      }
      obstacles[row][15] = false; // Limpiar la última columna
    }

    // 2. Generar nuevas rocas aleatoriamente a la derecha (columna 15)
    if (random(0, 10) > 6) { // 30% de probabilidad de que aparezca una roca
      int randomRow = random(0, 2);
      
      // Regla de oro: No crear paredes imposibles de esquivar
      if (!obstacles[!randomRow][15] && !obstacles[!randomRow][14]) {
         obstacles[randomRow][15] = true;
      }
    }

    // 3. Detectar Colisiones
    // El auto está en la columna 1. Si hay una roca ahí en su carril, explota.
    if (obstacles[playerRow][1]) {
      isGameOver = true;
    } else {
      score += 5; // Sumar puntos por sobrevivir
      
      // Aumentar la dificultad (velocidad) cada 100 puntos
      if (gameSpeed > 60 && score % 100 == 0) {
        gameSpeed -= 20; 
      }
    }

    // 4. Dibujar el nuevo fotograma
    if (!isGameOver) {
      drawGame();
    }
  }
}