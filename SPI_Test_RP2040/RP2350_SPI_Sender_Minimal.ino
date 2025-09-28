// ================================================================
// RP2350 → RP2040 SPI テキスト送信 (最小構成)
// ================================================================

#include <SPI.h>

#define SPI_CS_PIN 1
#define SPI_SPEED 100000

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("RP2350 SPI Text Sender");
  Serial.println("Type text and press Enter");
  
  // SPI初期化
  SPI.setSCK(2);
  SPI.setTX(3);
  SPI.setRX(0);
  SPI.begin();
  pinMode(SPI_CS_PIN, OUTPUT);
  digitalWrite(SPI_CS_PIN, HIGH);
}

void loop() {
  if (Serial.available()) {
    String text = Serial.readStringUntil('\n');
    text.trim();
    
    if (text.length() > 0) {
      sendText(text);
      Serial.println("Sent: " + text);
    }
  }
}

void sendText(String text) {
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
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