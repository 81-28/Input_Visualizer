// ================================================================
// シリアル中継・モニタリング用コード (Raspberry Pi Pico)
// ================================================================
// 役割:
// 1. Serial1 (RX1) で RP2350 からのデータを受信する。
// 2. 受信したデータを、そのまま Serial2 (TX2) へ送信する (Leonardo行き)。
// 3. 受信したデータを、そのまま Serial (USB) へ送信する (PCでのモニタリング用)。

void setup() {
  // PCとのUSBシリアル通信 (モニタリング用)
  Serial.begin(115200);

  // RP2350とのシリアル通信 (GP4=TX1, GP5=RX1)
  Serial1.setTX(4);
  Serial1.setRX(5);
  Serial1.begin(115200);

  // Leonardoとのシリアル通信 (GP8=TX2, GP9=RX2)
  Serial2.setTX(8);
  Serial2.setRX(9);
  Serial2.begin(115200);
}

void loop() {
  // Serial1 (RP2350から) にデータがあれば...
  if (Serial1.available()) {
    // 1バイト読み込む
    int data = Serial1.read();
    
    // 読み込んだデータを、LeonardoとPCの両方へ送信する
    Serial2.write(data); // Leonardoへ
    Serial.write(data);  // PCへ
  }
}

