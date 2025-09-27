// ================================================================
// シリアル中継・モニタリング用コード (Raspberry Pi Pico)
// ================================================================
// 役割:
// 1. Serial1 (RX1) で RP2350 からのデータを受信する。
// 2. 受信したデータを、そのまま Serial2 (TX2) へ送信する (Leonardo行き)。
// 3. 受信したデータを、そのまま Serial (USB) へ送信する (PCでのモニタリング用)。
#define LED_PIN 25

void setup() {
  pinMode(LED_PIN, OUTPUT);
  // PCとのUSBシリアル通信 (モニタリング用)
  Serial.begin(115200);
  // RP2350とのシリアル通信 (GP0=TX1, GP1=RX1)
  Serial1.begin(115200);
  // Leonardoとのシリアル通信 (GP8=TX2, GP9=RX2)
  Serial2.begin(115200);
}

void loop() {
  // Serial1 (RP2350から) にデータがあれば...
  if (Serial1.available()) {
    // 1バイト読み込む
    int data = Serial1.read();
    // Serial.print(data, HEX);
    // Serial.print("\n");
    
    // 読み込んだデータを、LeonardoとPCの両方へ送信する
    Serial2.write(data); // Leonardoへ
    Serial.write(data);  // PCへ
  }
}

