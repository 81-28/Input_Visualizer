# Pro Controller Input Visualizer & Recorder

Nintendo Switch Pro Controllerの入力データをリアルタイムで表示し、記録・再生機能を提供するアプリケーションです。

## 機能

### 🎮 リアルタイム表示
- プロコンのボタン状態をリアルタイム表示
- 左右アナログスティックの位置表示
- D-Padの状態表示

### 📹 記録・再生機能
- **入力記録**: プロコンの全ての入力を高精度タイムスタンプ付きで記録
- **入力再生**: 記録したデータを再生してコントローラー操作を完全再現
- **自動ファイル管理**: `Recordings/YYYY_MM_DD_HH_MM_SS.rec`形式で自動保存
- **トリガー検出**: ニュートラル状態からの最初の入力で記録・再生を自動開始
- **高精度タイミング**: QueryPerformanceCounterによるマイクロ秒精度の再生制御

### 🔧 高度な機能
- **接続状態管理**: COMポート接続時のみ記録・再生ボタンが有効
- **統合UI**: スタート/ストップを統一したトグルボタン
- **リアルタイム状態表示**: 現在のモード（パススルー/記録/再生）を表示
- **プリバッファリング**: 再生時の最初の3フレームを事前送信して遅延を最小化
- **自動完了処理**: 再生完了時の自動UI更新とモード切り替え

## システム構成

### ハードウェア構成
```
Pro Controller → RP2350 (SPI) → ATmega32U4 (USB HID) → Nintendo Switch
                               ↕ (Serial)
                              PC Application
```

### ソフトウェア構成
- **PC側**: wxWidgets C++アプリケーション
- **ATmega32U4**: Arduino スケッチ（USB HID変換 + シリアル通信）
- **RP2350**: Pro Controller読み取り（SPIマスター）

## システム要件

- Windows 10/11
- wxWidgets 3.3.1
- Visual Studio 2019/2022

## セットアップ

1. **wxWidgets のインストール**
   - `C:\wxWidgets-3.3.1` または `C:\wxWidgets` にインストール

2. **プロジェクトのビルド**
   ```batch
   build.bat
   ```

3. **実行**
   ```batch
   x64\Release\ProConVisualizer.exe
   ```

## 使用方法

### 基本操作
1. ATmega32U4デバイスをUSBで接続
2. アプリケーションを起動
3. COMポートを選択して「Connect」
4. プロコンの入力がリアルタイムで表示されます

### 記録・再生
1. **記録**: 
   - 「Start Recording」をクリック
   - プロコンをニュートラル状態にしてから操作開始
   - 「Stop Recording」で終了、自動的に`Recordings`フォルダに保存

2. **再生**:
   - 「Load Playback File」で記録ファイルを選択
   - 「Start Playback」をクリック
   - プロコンで何か操作すると再生開始

## 実装状況

### ✅ 完成済み機能
- リアルタイム入力表示
- 高精度入力記録（タイムスタンプ付き）
- 入力再生システム
- トリガー検出による自動開始
- ファイル自動管理（タイムスタンプ付きファイル名）
- 接続状態に基づくUI制御
- 統合されたトグルボタンUI
- 再生完了時の自動状態復帰
- プリバッファリングシステム

### 🔧 現在の技術仕様
- **記録精度**: Windows `GetTickCount()` （約15-16ms精度）
- **再生精度**: `QueryPerformanceCounter` （マイクロ秒精度）
- **通信プロトコル**: COBS + CRC8
- **Arduino処理頻度**: 
  - 通常時: 333Hz (`delay(3)`)
  - 再生時: 1000Hz (`delay(1)`)
- **データ形式**: バイナリ（.recファイル）

## 既知の問題点

### ⚠️ タイミング精度の問題
1. **記録・再生間の時間解像度ミスマッチ**
   - 記録: `GetTickCount()` （15-16ms精度）
   - 再生: `QueryPerformanceCounter` （マイクロ秒精度）
   - 結果: 精密な再生ができない

2. **Arduino処理頻度の不一致**
   - 記録時: `delay(3)` → 333Hz（3ms間隔）
   - 再生時: `delay(1)` → 1000Hz（1ms間隔）
   - 結果: サンプリング頻度が異なるため等倍再生ができない

3. **PC側データ送信頻度の差**
   - 記録時: 毎フレーム送信
   - 再生時: 1/3頻度の送信
   - 結果: データ流れが不一致

4. **システム遅延の蓄積**
   - シリアル通信遅延
   - USB HID処理遅延
   - プリバッファ遅延
   - 結果: 累積遅延によるタイミングずれ

### 🎯 完全再現に向けた課題
- **根本原因**: 記録システムと再生システムの非対称性
- **目標**: 1:1の完全等倍再生の実現
- **必要な対策**: 
  - 記録・再生でのタイミング制御統一
  - 高精度タイムスタンプの一貫使用
  - Arduino側処理頻度の統一
  - システム遅延の測定と補正

## プロジェクト構成

```
ProConVisualizer/
├── src/                 # ソースファイル
│   ├── ProConVisualizer.cpp    # メインUI
│   ├── ControllerPanel.cpp     # 描画・記録・再生制御
│   └── SerialUtils.cpp         # シリアル通信
├── include/             # ヘッダーファイル
│   ├── ControllerPanel.h
│   └── SerialUtils.h
├── build.bat            # ビルドスクリプト
├── x64/Release/         # 実行ファイル
└── Recordings/          # 記録データ保存フォルダ（自動作成）
```

## 対応コントローラー入力

- A/B/X/Y ボタン
- L/R, ZL/ZR ボタン  
- PLUS/MINUS ボタン
- 左右スティック（押し込み含む）
- D-Pad（上下左右）
- HOME/CAPTURE ボタン

## 技術詳細

- **通信プロトコル**: COBS (Consistent Overhead Byte Stuffing)
- **データ整合性**: CRC8チェックサム
- **フレームレート**: 60FPS表示
- **グラフィック**: wxGraphicsContext による高品質描画
- **ファイル形式**: バイナリ（TimestampedData構造体の配列）
- **高精度タイマー**: Windows QueryPerformanceCounter