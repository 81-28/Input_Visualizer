# Input Visualizer

Nintendo Switch Pro Controllerの入力をリアルタイムで可視化するシステムです。

## 概要

このプロジェクトは、Nintendo Switch Pro Controller（プロコン）からの入力データを受信し、PC上でリアルタイムに可視化するシステムです。マイコン間でのSPI通信とPC向けのGUIアプリケーションで構成されています。

## システム構成

```
Pro Controller → RP2350 → ATMega32U4 → PC (Windows GUI)
                  (USB)    (SPI)      (Serial)
                                ↓
                            Nintendo Switch
                             (USB HID)
```

### ハードウェア
- **RP2350**: Pro ControllerからUSB HIDデータを受信
- **ATMega32U4**: RP2350からSPI経由でデータを受信し、Nintendo SwitchへのHID出力とPCへのシリアル通信を同時実行

### ソフトウェア
- **ProConVisualizer**: Windows GUI アプリケーション（wxWidgets使用）
- コントローラーの状態をリアルタイムで視覚的に表示

## 主な機能

- Pro Controllerの全ボタン状態表示
- 左右アナログスティックの位置可視化
- 十字キー（D-Pad）の状態表示
- リアルタイム更新（約60FPS）
- シリアルポート自動検出
- **パススルー機能**: Nintendo Switchへの入力も同時出力（遅延なし）

## ファイル構成

```
├── RP2350.ino              # RP2350用ファームウェア
├── ATMega32U4.ino          # ATMega32U4用ファームウェア
├── ProConVisualizer/       # Windows GUIアプリケーション
│   ├── src/
│   │   ├── ProConVisualizer.cpp    # メインアプリケーション
│   │   ├── ControllerPanel.cpp     # コントローラー描画パネル
│   │   └── SerialUtils.cpp         # シリアル通信ユーティリティ
│   └── include/            # ヘッダファイル
└── SPI_Test_*/            # SPI通信テスト用サンプル
```

## 通信プロトコル

- **SPI通信**: RP2350 ↔ ATMega32U4間でコントローラーデータを転送
- **シリアル通信**: ATMega32U4 ↔ PC間でCOBS エンコーディングを使用
- **CRC8**: データ整合性チェック

## ビルド・実行

### マイコン側
1. Arduino IDEでそれぞれの`.ino`ファイルを対応するマイコンに書き込み

### PC側（Windows）
1. Visual Studio 2022以降が必要
2. wxWidgetsライブラリが必要
3. `ProConVisualizer.sln`をVisual Studioで開いてビルド

### 実行
1. Pro ControllerをRP2350に接続
2. PC上で`ProConVisualizer.exe`を実行
3. 適切なCOMポートを選択して接続

## 特徴

- **パススルー機能**: Pro ControllerをSwitchで使いながら、同時にPC上で入力を可視化
- **リアルタイム表示**: 低遅延での入力可視化
- **デッドゾーン処理**: RP2350側でハードウェアレベルのデッドゾーン補正
- **クロスプラットフォーム設計**: wxWidgetsベースのGUI
