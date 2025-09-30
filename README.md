# Input Visualizer

> Nintendo Switch Pro Controllerの入力をリアルタイムで可視化するシステム

## 📋 目次

- [概要](#概要)
- [システム構成](#システム構成)
- [主な機能](#主な機能)
- [ハードウェア配線](#ハードウェア配線)
- [セットアップガイド](#セットアップガイド)
- [使い方](#使い方)
- [ファイル構成](#ファイル構成)
- [通信プロトコル](#通信プロトコル)
- [トラブルシューティング](#トラブルシューティング)

## 概要

Nintendo Switch Pro Controller（プロコン）からの入力データを受信し、PC上でリアルタイムに可視化するシステムです。マイコン間のSPI通信とPC向けGUIアプリケーションで構成されており、**パススルー機能**により、Switchでの使用と同時にPC上での入力監視が可能です。

### 🎯 このシステムの特徴

- ✅ **リアルタイム可視化**: 約60FPSでの入力表示
- ✅ **パススルー機能**: Switchでの使用と同時監視
- ✅ **オフライン監視**: LCD表示でPC接続なしでも状態確認
- ✅ **デッドゾーン補正**: ハードウェアレベルでの精密な補正
- ✅ **エラーハンドリング**: 自動切断検知・復旧機能

## システム構成

```
Pro Controller → RP2350-USB-A → ATMega32U4 → Nintendo Switch
                      ↓             ↓
                 LCD Display    Serial → PC GUI
```

**データフロー:**
- USB HID: Pro Controller → RP2350-USB-A
- SPI (10kHz): RP2350-USB-A → ATMega32U4  
- USB HID: ATMega32U4 → Nintendo Switch
- Serial (115200bps): ATMega32U4 → PC
- I2C: RP2350-USB-A → LCD Display

### ハードウェアコンポーネント

| コンポーネント | 役割 | 詳細 |
|---------------|------|------|
| **RP2350-USB-A** | USB Host制御・データ処理 | Pro Controller接続、キャリブレーション処理、内蔵NeoPixel LED |
| **ATMega32U4** | パススルー・PC通信 | HID出力、シリアル通信、COBS/CRC処理 |
| **1602 LCD (I2C)** | オフライン状態表示 | 接続状態・経過時間の表示 |

## 主な機能

### 🎮 コントローラー機能
- Pro Controller全ボタンの状態表示
- 左右アナログスティックの位置可視化（デッドゾーン補正付き）
- 十字キー（D-Pad）の状態表示
- リアルタイム更新（約60FPS）

### 🔗 接続・通信機能
- シリアルポート自動検出
- **パススルー機能**: Nintendo Switchへの遅延なし入力転送
- 自動切断検知・復旧
- CRC8によるデータ整合性チェック

### 📊 監視・表示機能
- LCD表示による接続状態確認
- LED表示による視覚的フィードバック
- 接続時間の計測・表示
- シリアルログ出力

## ハードウェア配線

### 📌 配線概要

システムは以下の3つの通信路で構成されています：

1. **USB接続**: Pro Controller ↔ RP2350-USB-A
2. **SPI接続**: RP2350-USB-A ↔ ATMega32U4
3. **I2C接続**: RP2350-USB-A ↔ LCD Display

### 🔌 SPI接続: RP2350-USB-A ↔ ATMega32U4

#### ピン配置表

| RP2350-USB-A Pin | ATMega32U4 Pin | 信号名 | 機能 |
|------------|----------------|--------|------|
| `Pin 1` | `Pin 7 (SS)` | **CS** | チップセレクト |
| `Pin 2` | `Pin 15 (SCK)` | **SCK** | シリアルクロック |
| `Pin 3` | `Pin 16 (MOSI)` | **MOSI** | マスター出力/スレーブ入力 |
| `Pin 0` | `Pin 14 (MISO)` | **MISO** | マスター入力/スレーブ出力 *(未使用)* |
| `GND` | `GND` | **GND** | グラウンド |

#### 接続図
```
┌─────────────────┐           ┌────────────────┐
│  RP2350-USB-A   │           │  ATMega32U4    │
│                 │           │                │
│ Pin 1 (CS)      ├───────────┤ Pin 7 (SS)     │
│ Pin 2 (SCK)     ├───────────┤ Pin 15 (SCK)   │
│ Pin 3 (MOSI)    ├───────────┤ Pin 16 (MOSI)  │
│ Pin 0 (MISO)    ├─────────┬─┤ Pin 14 (MISO)  │ ※未使用
│ GND             ├─────────┼─┤ GND            │
└─────────────────┘         │ └────────────────┘
                            │
                        (Future expansion)
```

> ⚠️ **重要**: SPI通信速度は **10kHz** に設定。MISOラインは将来の拡張用として予約済み。

### 🔌 I2C接続: RP2350-USB-A ↔ LCD Display

#### ピン配置表

| RP2350-USB-A Pin | LCD Pin | 信号名 | 機能 |
|------------|---------|--------|------|
| `Pin 4` | `SDA` | **SDA** | I2Cデータライン |
| `Pin 5` | `SCL` | **SCL** | I2Cクロックライン |
| `5V` | `VCC` | **VCC** | 電源（5V） |
| `GND` | `GND` | **GND** | グラウンド |

#### 接続図
```
┌─────────────────┐           ┌──────────────────┐
│  RP2350-USB-A   │           │ 1602 LCD (I2C)   │
│                 │           │  Address: 0x27   │
│ Pin 4 (SDA)     ├───────────┤ SDA              │
│ Pin 5 (SCL)     ├───────────┤ SCL              │
│ 5V              ├───────────┤ VCC              │
│ GND             ├───────────┤ GND              │
└─────────────────┘           └──────────────────┘
```

### 🔌 シリアル接続: ATMega32U4 ↔ PC

#### ピン配置表

| ATMega32U4 Pin | USB-Serial変換器 | 信号名 | 機能 |
|----------------|------------------|--------|------|
| `Pin 0 (TX)` | `RX` | **TX** | 送信（ATMega32U4 → PC） |
| `Pin 1 (RX)` | `TX` | **RX** | 受信（PC → ATMega32U4） |
| `GND` | `GND` | **GND** | グラウンド |

#### 接続図
```
┌────────────────┐    ┌──────────────────┐    ┌─────────┐
│  ATMega32U4    │    │ USB-Serial変換器  │    │   PC    │
│                │    │                  │    │         │
│ Pin 0 (TX)     ├────┤ RX               │    │         │
│ Pin 1 (RX)     ├────┤ TX               ├────┤ USB Port│
│ GND            ├────┤ GND              │    │         │
└────────────────┘    └──────────────────┘    └─────────┘
```

> 📝 **通信設定**: ボーレート **115200bps**、COBS エンコーディング使用

### 💡 LED状態表示（内蔵NeoPixel）

※ RP2350-USB-AにはNeoPixel LEDが内蔵されています

| LED色 | 状態 | 説明 |
|-------|------|------|
| 🔵 **青色** | 待機中 | Pro Controller接続待ち |
| 🟡 **黄色** | 接続中 | Pro Controller初期化中 |
| 🟢 **緑色** | 接続完了 | 正常動作中 |
| 🔴 **赤色** | 切断・エラー | 接続切断またはエラー状態 |

### 📺 LCD表示フォーマット

```
┌─────────────────┐
│    0:15:32.451  │ ← 経過時間（右詰め、h:mm:ss.fff形式）
│Connected        │ ← 接続状態メッセージ
└─────────────────┘
```

#### 状態メッセージ一覧

| メッセージ | 状態 |
|------------|------|
| `"Starting..."` | システム起動中 |
| `"Waiting ProCon"` | Pro Controller待機中 |
| `"Connecting..."` | 接続・初期化中 |
| `"Connected"` | 接続完了 |
| `"Disconnected"` | 切断状態 |
| `"X h XX m"` | 接続時間表示 |

## セットアップガイド

### 🛠️ 必要な環境

#### ハードウェア
- [x] RP2350-USB-A マイコンボード（内蔵NeoPixel LED付き）
- [x] ATMega32U4 マイコンボード（Arduino Leonardo等）
- [x] 1602 LCD (I2C対応)
- [x] Nintendo Switch Pro Controller
- [x] USB-Serial変換器（PC接続用）
- [x] ジャンパーワイヤー

#### ソフトウェア
- [x] **Arduino IDE** 2.0以降
- [x] **Visual Studio 2022**以降（PC側GUI用）
- [x] **wxWidgets**ライブラリ
- [x] 必要なArduinoライブラリ：
  - `Adafruit_TinyUSB`
  - `LiquidCrystal_I2C`
  - `Adafruit_NeoPixel`

### 📥 インストール手順

#### 1. マイコンファームウェアの書き込み

```bash
# Arduino IDEで以下のファイルを書き込み
├── RP2350.ino      → RP2350-USB-Aマイコンに書き込み
└── ATMega32U4.ino  → ATMega32U4マイコンに書き込み
```

#### 2. ハードウェア配線

上記の[ハードウェア配線](#ハードウェア配線)セクションを参照して配線を行ってください。

#### 3. PC側アプリケーションのビルド

```bash
# build.batを実行してビルド
cd ProConVisualizer
build.bat
```

### ⚡ クイックスタート

#### 基本動作確認

1. **Pro ControllerをRP2350-USB-AにUSB接続**
   ※ 重要: RP2350-USB-Aの電源投入**前**にPro Controllerを接続する必要があります

2. **ファームウェア書き込み後、RP2350-USB-Aに電源投入**
   - 🔵 内蔵LED: 青色点灯（待機状態）
   - 📺 LCD: "Waiting ProCon"表示

3. **Pro Controllerが認識されると**
   - 🟡 内蔵LED: 黄色（接続中）→ 🟢 緑色（接続完了）
   - 📺 LCD: "Connected"表示

## 使い方

#### フル機能モード（PC接続あり）

```
Pro Controller
      ↓ (USB HID)
RP2350-USB-A
      ↓ (SPI)
ATMega32U4
   ↓       ↓
 (Serial) (USB HID)
   ↓       ↓  
 PC GUI   Nintendo Switch
```

**用途**: PC上でのリアルタイム可視化 + Switch同時使用

**重要**: Pro ControllerはRP2350-USB-Aの電源投入**前**に接続してください

1. 全ハードウェアを配線
2. Pro ControllerをRP2350-USB-Aに接続
3. RP2350-USB-Aに電源投入
4. PC上で`ProConVisualizer.exe`を起動
5. 適切なCOMポートを選択して接続
6. リアルタイム可視化開始

### � GUI操作方法

#### ProConVisualizer アプリケーション

**メイン画面構成**:
- 🎮 **コントローラー表示**: ボタン・スティック状態
- 📈 **リアルタイムグラフ**: スティック座標の軌跡
- 🔗 **接続パネル**: COMポート選択・接続状態
- ⚙️ **設定パネル**: 更新頻度・表示オプション

**操作方法**:
1. **接続**: COMポート選択 → "Connect"ボタン
2. **切断**: "Disconnect"ボタン
3. **設定変更**: 設定メニューから各種パラメータ調整

### 🔄 状態遷移フロー

```
[System Start]
      ↓
[Waiting for Pro Controller]
      ↓
[Pro Controller Connected?] → No ──┐
      ↓ Yes                         │
[Connecting...]                     │
      ↓                             │
[Initialization]                    │
      ↓                             │
[Connected] ←──────────────────────┘
      ↓
[Connection Lost?] → No ──┐
      ↓ Yes                │
[Disconnected]            │
      ↓                    │
[Auto Recovery]           │
      ↓                    │
[Back to Waiting] ←───────┘
```

### 🔧 トラブルシューティング

#### よくある問題と解決方法

| 問題 | 症状 | 解決方法 |
|------|------|----------|
| **接続できない** | 🔴 赤色LED点灯 | 1. USBケーブル確認<br>2. Pro Controller電源確認<br>3. デバイス再起動 |
| **データが来ない** | GUI上で入力反応なし | 1. COMポート設定確認<br>2. シリアル配線確認<br>3. ボーレート確認（115200bps） |
| **スティック値異常** | ドリフトや感度異常 | 1. キャリブレーション定数確認<br>2. デッドゾーン設定調整 |
| **頻繁な切断** | 接続が不安定 | 1. 電源供給確認<br>2. USB接続確認<br>3. ノイズ対策 |

#### デバッグ方法

1. **シリアルモニタ確認**
   ```
   Arduino IDE → Tools → Serial Monitor (115200bps)
   ```

2. **LCD表示確認**
   - 経過時間が正常に更新されているか
   - 状態メッセージが適切か

3. **LED状態確認**
   - 色の変化が期待通りか
   - 異常時の色表示

### ⚙️ 設定のカスタマイズ

#### スティックキャリブレーション

```cpp
// RP2350.ino内の設定
constexpr int STICK_LX_MIN = 300;     // 左スティック X軸最小値
constexpr int STICK_LX_NEUTRAL = 1841; // 左スティック X軸ニュートラル
constexpr int STICK_LX_MAX = 3450;    // 左スティック X軸最大値
// ... 他の軸も同様
```

#### デッドゾーン調整

```cpp
// デッドゾーン半径の調整
constexpr int STICK_DEADZONE_RADIUS = 150; // 0-4095の範囲で指定
```

#### 接続監視設定

```cpp
// 接続ログの出力間隔（分）
constexpr unsigned long CONNECTION_LOG_INTERVAL_MINUTES = 15;
```

## ファイル構成

### 📂 プロジェクト構造

```
Input_Visualizer/
├── 📄 README.md                    # このファイル
├── 📄 LICENSE                      # ライセンス情報
├── 🔧 RP2350.ino                   # RP2350用ファームウェア
├── 🔧 ATMega32U4.ino               # ATMega32U4用ファームウェア
├── 📁 ProConVisualizer/            # Windows GUIアプリケーション
│   ├── 📄 ProConVisualizer.sln     # Visual Studioソリューション
│   ├── 📄 ProConVisualizer.vcxproj # プロジェクトファイル
│   ├── 📄 build.bat                # ビルドスクリプト
│   ├── 📁 include/                 # ヘッダファイル
│   │   ├── ControllerPanel.h       # コントローラー描画パネル
│   │   └── SerialUtils.h           # シリアル通信ユーティリティ
│   ├── 📁 src/                     # ソースファイル
│   │   ├── ProConVisualizer.cpp    # メインアプリケーション
│   │   ├── ControllerPanel.cpp     # UI描画・入力処理
│   │   └── SerialUtils.cpp         # シリアル通信・プロトコル処理
│   └── 📁 x64/Release/             # ビルド出力
│       └── ProConVisualizer.exe    # 実行ファイル
```

### 🗂️ ファイル詳細

#### マイコンファームウェア

| ファイル | 対象 | 説明 |
|----------|------|------|
| `RP2350.ino` | RP2350-USB-A | メインファームウェア・USB Host制御・SPI送信 |
| `ATMega32U4.ino` | ATMega32U4 | SPI受信・HID送信・シリアル通信 |

#### PC側アプリケーション

| ファイル | 機能 |
|----------|------|
| `ProConVisualizer.cpp` | メインアプリケーション・UI管理 |
| `ControllerPanel.cpp` | コントローラー描画・リアルタイム表示 |
| `SerialUtils.cpp` | シリアル通信・COBS/CRC処理 |

## 通信プロトコル

### 📡 プロトコル階層

```
┌─────────────────────────────┐
│   Application Layer         │ ← Controller Data
│   (Controller Data)         │
├─────────────────────────────┤
│   Transport Layer           │ ← CRC8 Error Detection  
│   (CRC8 Error Detection)    │
├─────────────────────────────┤
│   Data Link Layer           │ ← COBS Encoding
│   (COBS Encoding)           │
├─────────────────────────────┤
│   Physical Layer            │ ← SPI/Serial/USB
│   (SPI/Serial/USB)          │
└─────────────────────────────┘
```

### 🔗 SPI通信: RP2350-USB-A ↔ ATMega32U4

#### データ構造

```cpp
struct ControllerData {
    uint16_t buttons;           // ボタン状態（16bit）
    uint8_t dpad;              // D-Pad状態（8bit）
    uint8_t stick_lx;          // 左スティック X軸（0-255）
    uint8_t stick_ly;          // 左スティック Y軸（0-255）
    uint8_t stick_rx;          // 右スティック X軸（0-255）
    uint8_t stick_ry;          // 右スティック Y軸（0-255）
    uint8_t crc;               // CRC8チェックサム
};
// 総サイズ: 8バイト
```

#### 通信仕様

| 項目 | 値 |
|------|------|
| **通信速度** | 10kHz |
| **データ長** | 8バイト |
| **エラー検出** | CRC8 |
| **通信方向** | 単方向（RP2350-USB-A → ATMega32U4） |

### 📻 シリアル通信: ATMega32U4 ↔ PC

#### COBSエンコーディング

```
Raw Data:     [01][02][00][03][04]
COBS Encoded: [03][01][02][02][03][04][00]
              └─┘       └─┘           └─┘
           Distance  Distance    Delimiter
```

**特徴**:
- ゼロバイトをデリミタとして使用
- 任意のバイナリデータを安全に送信
- フレーム同期の確実性

#### 通信仕様

| 項目 | 値 |
|------|------|
| **ボーレート** | 115200bps |
| **データビット** | 8bit |
| **パリティ** | なし |
| **ストップビット** | 1bit |
| **フロー制御** | なし |
| **エンコーディング** | COBS |
| **エラー検出** | CRC8 |

### 🔌 USB HID通信

#### Pro Controller → RP2350-USB-A

| 項目 | 値 |
|------|------|
| **VID** | 0x057E（Nintendo） |
| **PID** | 0x2009（Switch Pro Controller） |
| **レポートサイズ** | 64バイト |
| **更新頻度** | 約60Hz |

#### ATMega32U4 → Nintendo Switch

| 項目 | 値 |
|------|------|
| **デバイスクラス** | HID |
| **サブクラス** | Boot Interface |
| **プロトコル** | Keyboard/Mouse |
| **レポートサイズ** | 8バイト |

## トラブルシューティング

### 🔍 診断フローチャート

```
[問題発生]
    ↓
[LED状態確認]
    ├─ 🔵 青色 → [Pro Controller接続確認]
    │              ├─ [USBケーブル確認]
    │              └─ [Pro Controller電源確認]
    ├─ 🟡 黄色 → [接続処理中・待機]
    ├─ 🟢 緑色 → [正常動作中]
    │              ↓
    │          [PC側アプリ動作確認]
    │              ├─ 動作せず → [シリアル接続確認]
    │              └─ 動作中 → [入力データ確認]
    └─ 🔴 赤色 → [エラー状態]
                   ├─ [電源供給確認]
                   ├─ [配線確認]  
                   └─ [デバイスリスタート]
```

### 🛠️ 一般的な問題と解決策

#### 接続関係

| 症状 | 原因 | 解決方法 |
|------|------|----------|
| LED が点灯しない | 電源供給不足 | • USB電源確認<br>• 電源容量確認 |
| 🔴 赤色LED点灯 | Pro Controller未接続 | • USBケーブル交換<br>• Controller電源ON |
| 接続後即切断 | 通信エラー | • ケーブル品質確認<br>• ノイズ対策 |

#### PC通信関係

| 症状 | 原因 | 解決方法 |
|------|------|----------|
| COMポート認識されない | ドライバー未インストール | • USB-Serialドライバー再インストール |
| データ受信されない | 配線・設定エラー | • シリアル配線確認<br>• ボーレート確認 |
| データ化け | ノイズ・同期エラー | • グラウンド強化<br>• ケーブル交換 |

#### 動作関係

| 症状 | 原因 | 解決方法 |
|------|------|----------|
| スティック値異常 | キャリブレーション | • 設定値見直し<br>• デッドゾーン調整 |
| ボタン反応しない | マッピングエラー | • ボタン定義確認<br>• ファームウェア更新 |
| 遅延が大きい | 処理負荷過大 | • 更新頻度調整<br>• 不要処理削除 |
