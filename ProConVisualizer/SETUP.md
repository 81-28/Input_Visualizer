# Pro Controller Visualizer - 簡単セットアップガイド

CMakeが無くてもビルドできるように、Visual Studioプロジェクトファイルを用意しました。

## 必要なもの

1. **Visual Studio 2019 以上** (Community版で十分)
2. **wxWidgets 3.1以上**

## wxWidgets のセットアップ

### 1. wxWidgets のダウンロード
- https://www.wxwidgets.org/downloads/ から最新版をダウンロード
- `C:\wxWidgets` に解凍

### 2. wxWidgets のビルド
PowerShellまたはコマンドプロンプトで以下を実行:

```cmd
cd C:\wxWidgets\build\msw
```

**Visual Studio 2019の場合:**
```cmd
msbuild wx_vc16.sln /p:Configuration=Release /p:Platform=x64
msbuild wx_vc16.sln /p:Configuration=Debug /p:Platform=x64
```

**Visual Studio 2022の場合:**
```cmd
msbuild wx_vc17.sln /p:Configuration=Release /p:Platform=x64  
msbuild wx_vc17.sln /p:Configuration=Debug /p:Platform=x64
```

## Pro Controller Visualizer のビルド

1. `build.bat` をダブルクリック
2. または Visual Studio で `ProConVisualizer.sln` を開いてビルド

## トラブルシューティング

### wxWidgets が見つからない場合
`ProConVisualizer.vcxproj` を編集して、wxWidgets のパスを変更:
```xml
<AdditionalIncludeDirectories>C:\wxWidgets\include;C:\wxWidgets\include\msvc;</AdditionalIncludeDirectories>
<AdditionalLibraryDirectories>C:\wxWidgets\lib\vc_x64_lib;</AdditionalLibraryDirectories>
```

### 32bit版でビルドしたい場合
`build.bat` の最後の行を変更:
```cmd
"%MSBUILD%" ProConVisualizer.sln /p:Configuration=Release /p:Platform=Win32
```

## 使用方法

1. RP2350マイコンをPCに接続
2. 生成された `x64\Release\ProConVisualizer.exe` を実行
3. COMポートを選択して「接続」
4. プロコンを操作して動作確認