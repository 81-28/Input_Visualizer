# 簡単セットアップガイド

## 1. wxWidgetsのダウンロード

1. https://www.wxwidgets.org/downloads/ にアクセス
2. 「Windows Binaries」の「Source Archives」から最新版をダウンロード（例：wxWidgets-3.2.4.zip）
3. `C:\wxWidgets` に解凍

## 2. wxWidgetsのビルド

`build_wxwidgets.bat` をダブルクリック

## 3. アプリケーションのビルド

`build.bat` をダブルクリック

## 4. 実行

`x64\Release\ProConVisualizer.exe` が作成されます

---

## トラブルシューティング

### wxWidgetsが見つからない
- `C:\wxWidgets\include\wx\wx.h` が存在することを確認
- パスが違う場合は `ProConVisualizer.vcxproj` を編集

### ビルドエラー
- Visual Studio 2019/2022がインストールされていることを確認
- C++開発ツールがインストールされていることを確認

### 文字化け
修正済み（UTF-8対応）