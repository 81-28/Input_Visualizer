# Pro Controller Visualizer

このアプリケーションは、RP2350マイコンから送信されるNintendo Switch Pro Controllerの入力データを視覚化します。

## 機能

- プロコンのボタン状態をリアルタイム表示
- 左右スティックの位置表示
- D-Padの状態表示
- シリアル通信によるデータ受信（COBS プロトコル対応）
- CRC8によるデータ整合性チェック

## 必要な環境

- Windows 10/11
- wxWidgets 3.1以上
- CMake 3.16以上
- Visual Studio 2019以上 または MinGW

## ビルド方法

1. wxWidgetsをインストール
2. CMakeでプロジェクトを生成
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## 使用方法

1. RP2350マイコンをPCに接続
2. アプリを起動
3. COMポートを選択して「接続」をクリック
4. プロコンを操作すると、リアルタイムで入力が表示されます

## データ形式

RP2350から送信されるデータは以下の構造です：
- ボタン状態: 16bit
- D-Pad状態: 8bit  
- スティック値: 各軸8bit (左X, 左Y, 右X, 右Y)
- CRC: 8bit

データはCOBS (Consistent Overhead Byte Stuffing) プロトコルでエンコードされています。