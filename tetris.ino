#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Configuración LCD 16x2 I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define I2C_SDA 21
#define I2C_SCL 22

// --- PINES CORREGIDOS Y SEGUROS PARA ESP32 ---
const int PIN_ROTATE = 14; 
const int PIN_LEFT = 25;   
const int PIN_RIGHT = 26;  
const int PIN_DOWN = 27;   

uint16_t board[16] = {0}; 

const int shapes[7][4][4] = {
  { {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0} }, // I
  { {1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, // J
  { {0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, // L
  { {0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} }, // O
  { {0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} }, // S
  { {0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, // T
  { {1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} }  // Z
};

int currentPiece[4][4];
int currentX, currentY;

unsigned long lastDropTime = 0;
int dropInterval = 800;
int score = 0;
bool isGameOver = false;
bool needsUpdate = true;

// --- Funciones del Motor Gráfico LCD ---
void renderScreen() {
  uint16_t frame[16];
  
  for(int i=0; i<16; i++) frame[i] = board[i];
  
  if (!isGameOver) {
    for(int i=0; i<4; i++) {
      for(int j=0; j<4; j++) {
        if(currentPiece[i][j]) {
          int fy = currentY + i;
          int fx = currentX + j;
          if(fy >= 0 && fy < 16 && fx >= 0 && fx < 10) {
            frame[fy] |= (1 << fx);
          }
        }
      }
    }
  }

  uint8_t char0[8] = {0}, char1[8] = {0}, char2[8] = {0}, char3[8] = {0};
  
  for(int y=0; y<8; y++) {
    for(int x=0; x<5; x++)  if(frame[y] & (1 << x)) char0[y] |= (1 << (4 - x));
    for(int x=5; x<10; x++) if(frame[y] & (1 << x)) char1[y] |= (1 << (4 - (x - 5)));
  }
  for(int y=8; y<16; y++) {
    for(int x=0; x<5; x++)  if(frame[y] & (1 << x)) char2[y-8] |= (1 << (4 - x));
    for(int x=5; x<10; x++) if(frame[y] & (1 << x)) char3[y-8] |= (1 << (4 - (x - 5)));
  }

  // Enviar memoria al LCD 
  lcd.createChar(0, char0);
  lcd.createChar(1, char1);
  lcd.createChar(2, char2);
  lcd.createChar(3, char3);

  lcd.setCursor(0, 0); lcd.write(0); lcd.write(1);
  lcd.setCursor(0, 1); lcd.write(2); lcd.write(3);

  lcd.setCursor(3, 0); lcd.print(" Puntos:");
  lcd.setCursor(3, 1); 
  if(isGameOver) {
      lcd.print(" GAME OVER!");
  } else {
    lcd.print(" ");
    lcd.print(score);
    lcd.print("      ");
  }
}

// --- Lógica del Juego ---
bool checkCollision(int x, int y, int piece[4][4]) {
  for(int i=0; i<4; i++) {
    for(int j=0; j<4; j++) {
      if(piece[i][j]) {
        int bx = x + j;
        int by = y + i;
        if(bx < 0 || bx >= 10 || by >= 16) return true;
        if(by >= 0 && (board[by] & (1 << bx))) return true;
      }
    }
  }
  return false;
}

void spawnPiece() {
  int shapeIdx = random(0, 7);
  for(int i=0; i<4; i++) {
    for(int j=0; j<4; j++) {
      currentPiece[i][j] = shapes[shapeIdx][i][j];
    }
  }
  currentX = 3;
  currentY = 0;
  if(checkCollision(currentX, currentY, currentPiece)) isGameOver = true;
  needsUpdate = true;
}

void lockPiece() {
  for(int i=0; i<4; i++) {
    for(int j=0; j<4; j++) {
      if(currentPiece[i][j]) {
        board[currentY + i] |= (1 << (currentX + j));
      }
    }
  }
  
  int linesCleared = 0;
  for (int i = 15; i >= 0; i--) {
    if (board[i] == 0x3FF) { 
      linesCleared++;
      for (int k = i; k > 0; k--) board[k] = board[k-1];
      board[0] = 0;
      i++; 
    }
  }
  if(linesCleared > 0) score += linesCleared * 10;
  spawnPiece();
}

void rotatePiece() {
  int nextPiece[4][4];
  for(int i=0; i<4; i++) {
    for(int j=0; j<4; j++) nextPiece[j][3-i] = currentPiece[i][j];
  }
  if(!checkCollision(currentX, currentY, nextPiece)) {
    for(int i=0; i<4; i++) {
      for(int j=0; j<4; j++) currentPiece[i][j] = nextPiece[i][j];
    }
    needsUpdate = true;
  }
}

void setup() {
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Configuración de botones (Recuerda conectarlos a GND del ESP32)
  pinMode(PIN_ROTATE, INPUT_PULLUP);
  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);

  randomSeed(analogRead(0));
  spawnPiece();
}

void loop() {
  if (isGameOver) {
    if (digitalRead(PIN_ROTATE) == LOW) {
      isGameOver = false;
      score = 0;
      for(int i=0; i<16; i++) board[i] = 0;
      lcd.clear();
      spawnPiece();
    }
    if(needsUpdate) { renderScreen(); needsUpdate = false; }
    return;
  }

  if (digitalRead(PIN_LEFT) == LOW) {
    if (!checkCollision(currentX - 1, currentY, currentPiece)) { currentX--; needsUpdate = true; }
    delay(150); 
  }
  if (digitalRead(PIN_RIGHT) == LOW) {
    if (!checkCollision(currentX + 1, currentY, currentPiece)) { currentX++; needsUpdate = true; }
    delay(150);
  }
  if (digitalRead(PIN_ROTATE) == LOW) {
    rotatePiece();
    delay(200); 
  }
  
  int speedDelay = dropInterval;
  if (digitalRead(PIN_DOWN) == LOW) speedDelay = 80; // Suavizado para evitar parpadeos bruscos

  if (millis() - lastDropTime > speedDelay) {
    if (!checkCollision(currentX, currentY + 1, currentPiece)) {
      currentY++;
    } else {
      lockPiece();
    }
    lastDropTime = millis();
    needsUpdate = true;
  }

  if(needsUpdate) {
    renderScreen();
    needsUpdate = false;
  }
}