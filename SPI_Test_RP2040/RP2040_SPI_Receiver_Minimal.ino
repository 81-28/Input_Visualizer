// ================================================================
// RP2040 SPI テキスト受信 (最小構成)
// ================================================================

#define SPI_MOSI_PIN 3
#define SPI_SCK_PIN  2
#define SPI_CS_PIN   5
#define LED_PIN     25

String receivedText = "";
volatile bool dataReady = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("RP2040 SPI Text Receiver");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(SPI_MOSI_PIN, INPUT);
  pinMode(SPI_SCK_PIN, INPUT);
  pinMode(SPI_CS_PIN, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(SPI_CS_PIN), onCSChange, FALLING);
}

void loop() {
  if (dataReady) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("Received: " + receivedText);
    receivedText = "";
    dataReady = false;
    delay(100);
    digitalWrite(LED_PIN, LOW);
  }
}

void onCSChange() {
  receivedText = "";
  
  while (digitalRead(SPI_CS_PIN) == LOW) {
    uint8_t byte_data = receiveByte();
    
    if (digitalRead(SPI_CS_PIN) == LOW) {
      if (byte_data == '\n') break;
      if (byte_data >= 32 && byte_data <= 126) {
        receivedText += (char)byte_data;
      }
    }
  }
  
  if (receivedText.length() > 0) {
    dataReady = true;
  }
}

uint8_t receiveByte() {
  uint8_t byte_data = 0;
  
  for (int bit = 7; bit >= 0; bit--) {
    while (digitalRead(SPI_SCK_PIN) == LOW && digitalRead(SPI_CS_PIN) == LOW);
    if (digitalRead(SPI_CS_PIN) == LOW) {
      if (digitalRead(SPI_MOSI_PIN)) byte_data |= (1 << bit);
      while (digitalRead(SPI_SCK_PIN) == HIGH && digitalRead(SPI_CS_PIN) == LOW);
    }
  }
  
  return byte_data;
}