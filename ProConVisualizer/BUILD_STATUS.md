# 修正完了 - ビルド手順

wxWidgets 3.3.1との互換性問題を修正しました。

## 修正内容

1. **wxAutoBufferedPaintDC** → **wxPaintDC** に変更
2. **wxGraphicsContext::SetFont** の引数を **wxGraphicsFont** に変更
3. 必要なライブラリ(**wxmsw32u_adv.lib**等)をプロジェクトに追加
4. 自動wxWidgetsビルド機能を追加

## ビルド手順

### 1. wxWidgetsを先にビルド
```
build_wxwidgets.bat をダブルクリック
```

### 2. アプリケーションをビルド  
```
build.bat をダブルクリック
```

もしくは、`build.bat`だけ実行しても、失敗した場合は自動的にwxWidgetsをビルドして再試行します。

## 完成後の実行ファイル
```
x64\Release\ProConVisualizer.exe
```

これでwxWidgets 3.3.1でビルドできるはずです！