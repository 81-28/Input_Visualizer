cmake_minimum_required(VERSION 3.16)
project(ProConVisualizer)

set(CMAKE_CXX_STANDARD 17)

# wxWidgets を検索
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# 実行ファイルを作成
add_executable(ProConVisualizer
    ProConVisualizer.cpp
    ControllerPanel.cpp
    ControllerPanel.h
)

# wxWidgets をリンク
target_link_libraries(ProConVisualizer ${wxWidgets_LIBRARIES})

# Windows固有の設定
if(WIN32)
    # Windowsでコンソールを非表示にする
    set_target_properties(ProConVisualizer PROPERTIES WIN32_EXECUTABLE TRUE)
    
    # 必要なライブラリをリンク
    target_link_libraries(ProConVisualizer kernel32)
    
    # Visual Studio用の設定
    if(MSVC)
        # UTF-8エンコーディングを使用
        target_compile_options(ProConVisualizer PRIVATE /utf-8)
        
        # デバッグ情報を含める
        target_compile_options(ProConVisualizer PRIVATE $<$<CONFIG:Debug>:/Zi>)
        target_link_options(ProConVisualizer PRIVATE $<$<CONFIG:Debug>:/DEBUG>)
    endif()
endif()

# インクルードディレクトリを設定
target_include_directories(ProConVisualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# コンパイラ固有の警告設定
if(MSVC)
    target_compile_options(ProConVisualizer PRIVATE /W3)
else()
    target_compile_options(ProConVisualizer PRIVATE -Wall -Wextra)
endif()