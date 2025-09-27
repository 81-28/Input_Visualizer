# Pro Controller Visualizer

RP2350マイコンから送信されるNintendo Switch Pro Controllerの入力データをリアルタイムで表示するアプリケーションです。

## 機能

- 🎮 プロコンのボタン状態をリアルタイム表示
- 🕹️ 左右アナログスティックの位置表示
- ➕ D-Padの状態表示  
- 📡 シリアル通信（COBS + CRC8）による安定したデータ受信
- 🖥️ 既存のVisualizerと統一されたUI デザイン

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

1. RP2350デバイスをUSBで接続
2. アプリケーションを起動
3. COMポートを選択して「Connect」
4. プロコンの入力がリアルタイムで表示されます

## プロジェクト構成

```
ProConVisualizer/
├── src/                 # ソースファイル
│   ├── ProConVisualizer.cpp
│   ├── ControllerPanel.cpp
│   └── SerialUtils.cpp
├── include/             # ヘッダーファイル
│   ├── ControllerPanel.h
│   └── SerialUtils.h
├── build.bat            # ビルドスクリプト
└── x64/Release/         # 実行ファイル
```

## 対応コントローラー入力

- A/B/X/Y ボタン
- L/R, ZL/ZR ボタン  
- PLUS/MINUS ボタン
- 左右スティック（押し込み含む）
- D-Pad（上下左右）

## 技術詳細

- **通信プロトコル**: COBS (Consistent Overhead Byte Stuffing)
- **データ整合性**: CRC8チェックサム
- **フレームレート**: 60FPS
- **グラフィック**: wxGraphicsContext による高品質描画