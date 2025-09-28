# RP2350 → RP2040 SPI テキスト送信 (最小構成)

## ファイル
- `RP2350_SPI_Sender_Minimal.ino` - RP2350-USB-A用送信コード
- `RP2040_SPI_Receiver_Minimal.ino` - RP2040用受信コード

## 配線
```
RP2350-USB-A → RP2040
Pin 2 (SCK)  → GP2
Pin 3 (MOSI) → GP3  
Pin 1 (CS)   → GP5
GND          → GND
```

## 使用方法
1. 両方のコードをアップロード
2. 配線を接続
3. RP2350のシリアルモニタで文字列入力
4. RP2040のシリアルモニタで受信確認

## 動作
- **RP2350**: 入力テキストをSPI送信
- **RP2040**: SPI受信してシリアル表示 + LED点滅

## 特徴
- 最小限のコード (各30行程度)
- デバッグ機能なし
- シンプルな動作のみ