# SPI Text Communication Test (Minimal)

## 概要
RP2350とATMega32u4(Pro Micro)間でのシンプルなSPIテキスト通信テストです。

## 構成
- **送信側**: RP2350-USB-A
- **受信側**: ATMega32u4 (Pro Micro)
- **通信方式**: SPI (可変速度, Mode 0, MSB First)

## ピン接続

### RP2350 → ATMega32u4
| RP2350 | 信号 | ATMega32u4 | 備考 |
|--------|------|------------|------|
| Pin 2  | SCK  | Pin 15     | クロック |
| Pin 3  | MOSI | Pin 16     | データ |
| Pin 0  | MISO | Pin 14     | (未使用) |
| Pin 1  | CS   | Pin 7      | チップセレクト |

## ファイル構成
```
SPI_Test_Micro/
├── RP2350_SPI_Sender.ino      # 送信側コード
├── ATMega32u4_SPI_Receiver.ino # 受信側コード
└── README.md                   # このファイル
```

## 使用方法

### 1. セットアップ
1. **ATMega32u4_SPI_Receiver.ino** をPro Microにアップロード
2. **RP2350_SPI_Sender.ino** をRP2350にアップロード
3. 上記のピン接続を行う

### 2. テスト実行
1. **ATMega32u4のシリアルモニタを開く** (115200 baud)
2. **RP2350のシリアルモニタを開く** (115200 baud)
3. RP2350側でテキストを入力してEnterキーを押す

### 3. 通信速度の変更
RP2350側で以下のコマンドを入力:
- **`slow`** - 5kHz (超安定)
- **`normal`** - 10kHz (標準)
- **`fast`** - 25kHz (高速)

### 4. 動作確認

#### RP2350側の表示例
```
RP2350 SPI Text Sender
Type text and press Enter
Commands:
  'slow'   - 5kHz
  'normal' - 10kHz
  'fast'   - 25kHz
Current speed: 10000 Hz (normal)

Sent: Hello
```

#### ATMega32u4側の表示例
```
ATMega32u4 SPI Text Receiver
CS Pin: 7
Ready...

Received: Hello
```

## LED表示
**Pro Micro LEDs:**
- **RX LED (Pin 17)**: 受信中に点灯
- **TX LED (Pin 30)**: 受信成功時に0.5秒点灯

## 技術仕様

### SPI設定
- **速度**: 可変 (5kHz/10kHz/25kHz)
- **モード**: SPI_MODE0 (CPOL=0, CPHA=0)
- **ビット順**: MSB First
- **制御**: ソフトウェア制御 (割り込み駆動)

### タイミング
- **セットアップ時間**: 100μs
- **ホールド時間**: 100μs
- **文字間隔**: 10μs
- **ビットタイムアウト**: 100ms

### 速度設定
| 設定 | 速度 | 用途 |
|------|------|------|
| slow | 5kHz | 超安定通信、長距離 |
| normal | 10kHz | 標準使用、バランス |
| fast | 25kHz | 高速通信、短距離 |

## トラブルシューティング

### 文字化けする場合
1. **ピン接続を再確認**
2. **GND接続を確認**
3. **電源供給を確認**

### 受信できない場合
1. **CS信号(Pin 7)の接続を確認**
2. **シリアルモニタのボーレートを確認** (115200)
3. **アップロードされたコードを確認**

### 一部文字が抜ける場合
- 通信速度を下げる (SPI_SPEED を 5000 など)
- タイムアウト時間を延ばす

## 拡張可能性
- **双方向通信**: MISO線を使用した応答機能
- **エラー検出**: CRC/チェックサム追加
- **高速化**: 安定後の速度向上
- **フロー制御**: Ready/Busyシグナル追加