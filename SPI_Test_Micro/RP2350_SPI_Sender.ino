// ================================================================
// RP2350 → ATMega32u4 SPI テキスト送信 (最小構成)
// ================================================================

#include <SPI.h>

#define SPI_CS_PIN 1

// 通信速度設定 (変更可能)
#define SPI_SPEED_SLOW   5000    // 5kHz  (超安定)
#define SPI_SPEED_NORMAL 10000   // 10kHz (標準)
#define SPI_SPEED_FAST   15000   // 15kHz (高速)
// 20kHzはタイムアウトする

int currentSpeed = SPI_SPEED_NORMAL;  // デフォルト速度

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("RP2350 SPI Text Sender");
  Serial.println("Type text and press Enter");
  Serial.println("Commands:");
  Serial.println("  'slow'   - 5kHz");
  Serial.println("  'normal' - 10kHz");
  Serial.println("  'fast'   - 15kHz");
  
  // SPI初期化
  SPI.setSCK(2);
  SPI.setTX(3);
  SPI.setRX(0);
  SPI.begin();
  pinMode(SPI_CS_PIN, OUTPUT);
  digitalWrite(SPI_CS_PIN, HIGH);
  
  showCurrentSpeed();
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() > 0) {
      // 速度変更コマンド
      if (input == "slow") {
        currentSpeed = SPI_SPEED_SLOW;
        showCurrentSpeed();
      } else if (input == "normal") {
        currentSpeed = SPI_SPEED_NORMAL;
        showCurrentSpeed();
      } else if (input == "fast") {
        currentSpeed = SPI_SPEED_FAST;
        showCurrentSpeed();
      } else {
        // テキスト送信
        sendText(input);
        Serial.println("Sent: " + input);
      }
    }
  }
}

void sendText(String text) {
  SPI.beginTransaction(SPISettings(currentSpeed, MSBFIRST, SPI_MODE0));
  digitalWrite(SPI_CS_PIN, LOW);
  delayMicroseconds(100);
  
  for (int i = 0; i < text.length(); i++) {
    SPI.transfer(text.charAt(i));
    delayMicroseconds(10);
  }
  SPI.transfer('\n');
  
  delayMicroseconds(100);
  digitalWrite(SPI_CS_PIN, HIGH);
  SPI.endTransaction();
}

void showCurrentSpeed() {
  Serial.print("Current speed: ");
  Serial.print(currentSpeed);
  Serial.print(" Hz (");
  if (currentSpeed == SPI_SPEED_SLOW) Serial.print("slow");
  else if (currentSpeed == SPI_SPEED_NORMAL) Serial.print("normal");
  else if (currentSpeed == SPI_SPEED_FAST) Serial.print("fast");
  Serial.println(")");
}