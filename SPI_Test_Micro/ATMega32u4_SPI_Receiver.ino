// ================================================================
// ATMega32u4 SPI Text Receiver (Minimal Configuration)
// ================================================================
// Tested settings: Pin 7 CS, 100ms timeout, Software control

#define SS_PIN   7    // CS signal from RP2350 Pin 1
#define MISO_PIN 14   // SPI MISO (unused)
#define SCK_PIN  15   // SPI SCK  <- RP2350 Pin 2
#define MOSI_PIN 16   // SPI MOSI <- RP2350 Pin 3

// Pro Micro LEDs
#define LED_RX 17     // RX LED (on during reception)
#define LED_TX 30     // TX LED (success indicator)

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("ATMega32u4 SPI Text Receiver");
  
  // LED setup
  pinMode(LED_RX, OUTPUT);
  pinMode(LED_TX, OUTPUT);
  digitalWrite(LED_RX, HIGH);  // OFF
  digitalWrite(LED_TX, HIGH);  // OFF
  
  // SPI pin setup
  pinMode(MISO_PIN, OUTPUT);     // Not used but set as output
  pinMode(MOSI_PIN, INPUT);      // Data input
  pinMode(SCK_PIN, INPUT);       // Clock input
  pinMode(SS_PIN, INPUT_PULLUP); // CS with pullup
  
  // Disable hardware SPI (use software control)
  SPCR = 0;
  
  // CS interrupt
  attachInterrupt(digitalPinToInterrupt(SS_PIN), onCSFalling, FALLING);
  
  Serial.print("CS Pin: ");
  Serial.println(SS_PIN);
  Serial.println("Ready...");
}

void loop() {
  // 割り込み駆動
  delay(100);
}

void onCSFalling() {
  digitalWrite(LED_RX, LOW);
  String receivedText = "";
  
  while (digitalRead(SS_PIN) == LOW) {
    uint8_t byte_data = receiveByte();
    
    if (digitalRead(SS_PIN) == LOW) {
      if (byte_data == '\n') break;
      if (byte_data >= 32 && byte_data <= 126) {
        receivedText += (char)byte_data;
      }
    }
  }
  
  digitalWrite(LED_RX, HIGH);
  
  if (receivedText.length() > 0) {
    digitalWrite(LED_TX, LOW);
    Serial.println("Received: " + receivedText);
    delay(100);
    digitalWrite(LED_TX, HIGH);
  }
}

uint8_t receiveByte() {
  uint8_t byte_data = 0;
  
  // 各ビットを受信 (MSB first) - 成功設定と同じ方式
  for (int bit = 7; bit >= 0; bit--) {
    // より長いタイムアウト (成功設定と同じ)
    unsigned long timeout = micros() + 100000; // 100ms per bit
    
    // クロック立ち上がり待ち
    while (digitalRead(SCK_PIN) == LOW && digitalRead(SS_PIN) == LOW && micros() < timeout) {
      delayMicroseconds(1);
    }
    
    if (digitalRead(SS_PIN) == LOW && micros() < timeout) {
      // より確実なサンプリング (成功設定と同じ)
      delayMicroseconds(10);  // クロックの中央でサンプリング
      
      if (digitalRead(MOSI_PIN)) {
        byte_data |= (1 << bit);
      }
      
      // クロック立ち下がり待ち
      timeout = micros() + 100000;
      while (digitalRead(SCK_PIN) == HIGH && digitalRead(SS_PIN) == LOW && micros() < timeout) {
        delayMicroseconds(1);
      }
    } else {
      // タイムアウト時は現在の値を返す
      Serial.print("TIMEOUT at bit ");
      Serial.println(bit);
      break;
    }
  }
  
  return byte_data;
}