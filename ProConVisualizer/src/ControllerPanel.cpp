#include "../include/ControllerPanel.h"
#include <wx/graphics.h>
#include <iostream>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

// カスタムイベント定義
wxDEFINE_EVENT(wxEVT_PLAYBACK_COMPLETED, wxCommandEvent);

wxBEGIN_EVENT_TABLE(ControllerPanel, wxPanel)
    EVT_PAINT(ControllerPanel::OnPaint)
    EVT_TIMER(wxID_ANY, ControllerPanel::OnTimer)
    EVT_SIZE(ControllerPanel::OnSize)
wxEND_EVENT_TABLE()

ControllerPanel::ControllerPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY), m_timer(this), hSerial(INVALID_HANDLE_VALUE)
{
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
    m_timer.Start(16); // 約60FPS
    
    // Windows高精度タイマーの初期化
#ifdef _WIN32
    timeBeginPeriod(1); // 1msタイマー解像度を要求
#endif
    
    // 記録・再生機能の初期化
    recording = false;
    playback_active = false;
    playback_waiting_for_trigger = false;
    current_mode = MODE_PASSTHROUGH;
    should_stop_playback = false;
    recording_start_time = 0;
    playback_start_time = 0;
    playback_index = 0;
    recording_waiting_for_trigger = false;
    has_neutral_state = false;
    
    // 遅延補正の初期化
    system_delay_ms = 50;  // 初期値は50ms
    delay_calibrated = false;
    first_frame_preloaded = false;
}

ControllerPanel::~ControllerPanel()
{
    DisconnectSerial();
    
    // Windows高精度タイマーのクリーンアップ
#ifdef _WIN32
    timeEndPeriod(1);
#endif
}

bool ControllerPanel::ConnectSerial(const wxString& portName)
{
    if (connected) {
        DisconnectSerial();
    }
    
    currentPort = portName;
    shouldStop = false;
    
    try {
        serialThread = std::thread(&ControllerPanel::SerialReadThread, this);
        
        // 接続完了を最大2秒待機
        int retryCount = 0;
        while (!connected && retryCount < 200) {  // 200 * 10ms = 2秒
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            retryCount++;
        }
        
        std::cout << "DEBUG: ConnectSerial - connection status after wait: " << (connected ? "true" : "false") << std::endl;
        return connected;
    }
    catch (const std::exception& e) {
        std::cout << "ERROR: Serial connection error: " << e.what() << std::endl;
        return false;
    }
}

void ControllerPanel::DisconnectSerial()
{
    if (connected || serialThread.joinable()) {
        shouldStop = true;
        connected = false;
        
        if (serialThread.joinable()) {
            serialThread.join();
        }
        
        if (hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
        }
    }
    
    // 再生スレッドの停止
    if (playback_thread.joinable()) {
        should_stop_playback = true;
        playback_thread.join();
    }
    
    // 記録・再生状態のリセット
    recording = false;
    playback_active = false;
    playback_waiting_for_trigger = false;
    current_mode = MODE_PASSTHROUGH;
    recording_waiting_for_trigger = false;
}

void ControllerPanel::SerialReadThread()
{
    // Windows APIでシリアルポートを開く
    wxString portPath = wxString::Format("\\\\.\\%s", currentPort);
    hSerial = CreateFileA(portPath.c_str(),
                         GENERIC_READ | GENERIC_WRITE,
                         0,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);
    
    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cout << "ERROR: Failed to open serial port: " << currentPort.ToStdString() << std::endl;
        return;
    }
    
    // シリアルポート設定
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
        return;
    }
    
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
    dcbSerialParams.fRtsControl = RTS_CONTROL_ENABLE;
    
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
        return;
    }
    
    // タイムアウト設定（より高速化）
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 1;  // バイト間のタイムアウトを短く
    timeouts.ReadTotalTimeoutConstant = 1;  // 総タイムアウトを短く
    timeouts.ReadTotalTimeoutMultiplier = 0;  // バイト毎の追加タイムアウトなし
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
        return;
    }
    
    connected = true;
    std::vector<uint8_t> packetBuffer;
    uint8_t readBuffer[256];  // 一度に複数バイト読み取り用
    
    while (!shouldStop && connected && hSerial != INVALID_HANDLE_VALUE) {
        DWORD bytesRead = 0;
        
        // 利用可能なデータを一度に読み取り
        if (!ReadFile(hSerial, readBuffer, sizeof(readBuffer), &bytesRead, NULL)) {
            Sleep(1);
            continue;
        }
        
        if (bytesRead == 0) {
            Sleep(1);
            continue;
        }
        
        // 読み取ったデータを処理
        for (DWORD i = 0; i < bytesRead; i++) {
            uint8_t byte = readBuffer[i];
            
            if (byte == 0x00) { // COBSパケット終了
                if (packetBuffer.size() > 1) {
                    std::vector<uint8_t> decoded;
                    if (DecodeCOBS(packetBuffer, decoded)) {
                        // ATmega32U4からのデータパケット受信チェック
                        if (decoded.size() == sizeof(ControllerData) + 1 && decoded[0] == CMD_DATA_PACKET) {
                            // コントローラーデータ部分を抽出
                            ControllerData newData;
                            memcpy(&newData, decoded.data() + 1, sizeof(ControllerData));
                            
                            // CRCチェック（RP2350.inoと同じ方式）
                            uint8_t receivedCRC = newData.crc;
                            newData.crc = 0;  // CRCフィールドを0にしてから計算
                            uint8_t calculatedCRC = CalculateCRC8((uint8_t*)&newData, sizeof(ControllerData));
                            if (calculatedCRC == receivedCRC) {
                                newData.crc = receivedCRC;  // CRC値を復元
                                std::lock_guard<std::mutex> lock(dataMutex);
                                controllerData = newData;
                                dataValid = true;
                                
                                // PC側での統合処理
                                ProcessControllerData(newData);
                                
                                // デバッグ出力（初回のみ）
                                static bool firstDataReceived = false;
                                if (!firstDataReceived) {
                                    std::cout << "First valid data received: buttons=0x" << std::hex << newData.buttons 
                                             << ", dpad=0x" << std::hex << (int)newData.dpad << std::dec << std::endl;
                                    firstDataReceived = true;
                                }
                            } else {
                                // 詳細なデバッグ情報
                                static int errorCount = 0;
                                if (errorCount++ < 5) { // 最初の5回のエラーのみ表示
                                    std::string dataHex;
                                    for (size_t i = 0; i < decoded.size(); i++) {
                                        char hex[4];
                                        sprintf_s(hex, "%02X ", decoded[i]);
                                        dataHex += hex;
                                    }
                                    std::cout << "CRC mismatch #" << errorCount << ": expected=0x" << std::hex << (int)receivedCRC 
                                             << ", calculated=0x" << std::hex << (int)calculatedCRC << std::dec
                                             << ", size=" << decoded.size() << ", data=[" << dataHex << "]" << std::endl;
                                }
                            }
                        }
                    }
                }
                packetBuffer.clear();
            } else {
                packetBuffer.push_back(byte);
            }
        }
    }
    
    // クリーンアップ
    if (hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
    }
    connected = false;
}

bool ControllerPanel::DecodeCOBS(const std::vector<uint8_t>& encoded, std::vector<uint8_t>& decoded)
{
    if (encoded.empty()) return false;
    
    decoded.clear();
    size_t read_index = 0;
    
    while (read_index < encoded.size()) {
        uint8_t code = encoded[read_index++];
        
        for (uint8_t i = 1; i < code && read_index < encoded.size(); i++) {
            decoded.push_back(encoded[read_index++]);
        }
        
        if (code < 0xFF && read_index <= encoded.size()) {
            decoded.push_back(0x00);
        }
    }
    
    // 最後の0は除去
    if (!decoded.empty() && decoded.back() == 0x00) {
        decoded.pop_back();
    }
    
    return true;
}

uint8_t ControllerPanel::CalculateCRC8(const uint8_t* data, size_t length)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void ControllerPanel::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    
    if (!gc) return;
    
    // 背景クリア
    gc->SetBrush(wxBrush(wxColour(40, 40, 40)));
    wxSize size = GetSize();
    gc->DrawRectangle(0, 0, size.x, size.y);
    
    DrawController(gc);
    
    delete gc;
}

void ControllerPanel::OnTimer(wxTimerEvent& event)
{
    Refresh();
}

void ControllerPanel::OnSize(wxSizeEvent& event)
{
    Refresh();
    event.Skip();
}

void ControllerPanel::DrawController(wxGraphicsContext* gc)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    
    if (!dataValid) {
        gc->SetBrush(wxBrush(wxColour(255, 255, 255)));
        wxGraphicsFont statusFont = gc->CreateFont(wxFont(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD), wxColour(255, 255, 255));
        gc->SetFont(statusFont);
        
        wxString status = connected ? "Waiting for data..." : "Not connected";
        double textWidth, textHeight;
        gc->GetTextExtent(status, &textWidth, &textHeight);
        gc->DrawText(status, 150, 100);
        return;
    }
    
    // ペン定義（Visualizerと同じ）
    wxPen redPen(*wxRED, 3, wxPENSTYLE_SOLID);
    wxPen yellowPen(*wxYELLOW, 3, wxPENSTYLE_SOLID);
    wxPen whitePen(*wxWHITE, 3, wxPENSTYLE_SOLID);
    wxPen whitePenS(*wxWHITE, 2, wxPENSTYLE_SOLID);
    
    // Lスティック（八角形 + 円形）
    gc->SetPen(wxPen(*wxWHITE, 3));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    // 八角形(外枠)
    int center_x = 50;
    int center_y = 115;
    int radius = 35;
    
    wxPoint2DDouble points[8];
    for (int i = 0; i < 8; ++i) {
        double angle = i * (2 * M_PI / 8);
        points[i].m_x = center_x + radius * cos(angle);
        points[i].m_y = center_y + radius * sin(angle);
    }
    // 八角形を描画（パスを使用）
    wxGraphicsPath octPath = gc->CreatePath();
    octPath.MoveToPoint(points[0]);
    for (int i = 1; i < 8; ++i) {
        octPath.AddLineToPoint(points[i]);
    }
    octPath.CloseSubpath();
    gc->DrawPath(octPath);
    
    // Lスティック
    radius = 20;
    gc->SetBrush(*wxBLACK);
    gc->SetPen(wxPen(controllerData.buttons & BTN_L_STICK ? *wxRED : *wxWHITE, 3));
    
    double stick_x = (double)(controllerData.stick_lx - 128) / 128.0;
    double stick_y = (double)(controllerData.stick_ly - 128) / 128.0;
    
    stick_x = center_x + stick_x * radius;
    stick_y = center_y + stick_y * radius; // Y軸反転はRP2350.ino側で処理済み
    
    gc->DrawEllipse(stick_x - radius, stick_y - radius, radius * 2, radius * 2);
    
    // Rスティック（八角形 + 円形）
    gc->SetPen(wxPen(*wxYELLOW, 3));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    // 八角形(外枠)
    center_x = 190;
    center_y = 170;
    radius = 32; // 少し小さめ
    
    for (int i = 0; i < 8; ++i) {
        double angle = i * (2 * M_PI / 8);
        points[i].m_x = center_x + radius * cos(angle);
        points[i].m_y = center_y + radius * sin(angle);
    }
    // 八角形を描画（パスを使用）
    octPath = gc->CreatePath();
    octPath.MoveToPoint(points[0]);
    for (int i = 1; i < 8; ++i) {
        octPath.AddLineToPoint(points[i]);
    }
    octPath.CloseSubpath();
    gc->DrawPath(octPath);
    
    // Rスティック
    radius = 18;
    gc->SetBrush(*wxBLACK);
    gc->SetPen(wxPen(controllerData.buttons & BTN_R_STICK ? *wxRED : *wxYELLOW, 3));
    
    stick_x = (double)(controllerData.stick_rx - 128) / 128.0;
    stick_y = (double)(controllerData.stick_ry - 128) / 128.0;
    
    stick_x = center_x + stick_x * radius;
    stick_y = center_y + stick_y * radius; // Y軸反転はRP2350.ino側で処理済み
    
    gc->DrawEllipse(stick_x - radius, stick_y - radius, radius * 2, radius * 2);
    
    // 十字キー
    DrawDPad(gc, 110, 170, 10);
    
    // PLUS, MINUS
    gc->SetPen(wxPen(*wxWHITE, 2));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    radius = 10;
    center_x = 150;
    center_y = 90;
    int offset = 36;
    
    gc->SetBrush(controllerData.buttons & BTN_PLUS ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x + offset - radius, center_y - radius, radius * 2, radius * 2);
    
    gc->SetBrush(controllerData.buttons & BTN_MINUS ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x - offset - radius, center_y - radius, radius * 2, radius * 2);
    
    // A, B, X, Yボタン
    gc->SetPen(wxPen(*wxWHITE, 3));
    
    radius = 12;
    center_x = 240;
    center_y = 120;
    offset = 25;
    
    gc->SetBrush(controllerData.buttons & BTN_A ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x + offset - radius, center_y - radius, radius * 2, radius * 2);
    
    gc->SetBrush(controllerData.buttons & BTN_B ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x - radius, center_y + offset - radius, radius * 2, radius * 2);
    
    gc->SetBrush(controllerData.buttons & BTN_X ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x - radius, center_y - offset - radius, radius * 2, radius * 2);
    
    gc->SetBrush(controllerData.buttons & BTN_Y ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x - offset - radius, center_y - radius, radius * 2, radius * 2);
    
    // L, Rボタン
    gc->SetPen(wxPen(*wxWHITE, 3));
    center_x = 150;
    center_y = 60;
    offset = 20;
    int w = 60;
    int h = 16;
    
    gc->SetBrush(controllerData.buttons & BTN_L ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(center_x - offset - w, center_y - h / 2, w, h, 6);
    
    gc->SetBrush(controllerData.buttons & BTN_R ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(center_x + offset, center_y - h / 2, w, h, 6);
    
    // ZL, ZRボタン
    gc->SetPen(wxPen(*wxWHITE, 3));
    center_x = 150;
    center_y = 20;
    offset = 20;
    w = 80;
    h = 30;
    
    gc->SetBrush(controllerData.buttons & BTN_ZL ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(center_x - offset - w, center_y - h / 2 + 10, w, h, 6);
    gc->SetBrush(controllerData.buttons & BTN_ZR ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(center_x + offset, center_y - h / 2 + 10, w, h, 6);
}

void ControllerPanel::DrawButton(wxGraphicsContext* gc, double x, double y, double radius, bool pressed, const wxString& label)
{
    wxColour buttonColor = pressed ? wxColour(255, 100, 100) : wxColour(100, 100, 100);
    gc->SetBrush(wxBrush(buttonColor));
    gc->SetPen(wxPen(wxColour(200, 200, 200), 2));
    
    gc->DrawEllipse(x - radius, y - radius, radius * 2, radius * 2);
    
    gc->SetBrush(wxBrush(wxColour(255, 255, 255)));
    wxGraphicsFont buttonFont = gc->CreateFont(wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD), wxColour(255, 255, 255));
    gc->SetFont(buttonFont);
    
    double textWidth, textHeight;
    gc->GetTextExtent(label, &textWidth, &textHeight);
    gc->DrawText(label, x - textWidth/2, y - textHeight/2);
}

void ControllerPanel::DrawStick(wxGraphicsContext* gc, double cx, double cy, double radius, uint8_t x_val, uint8_t y_val)
{
    // スティック範囲を描画
    gc->SetBrush(wxBrush(wxColour(60, 60, 60)));
    gc->SetPen(wxPen(wxColour(150, 150, 150), 2));
    gc->DrawEllipse(cx - radius, cy - radius, radius * 2, radius * 2);
    
    // スティック位置を計算 (128が中央、Y軸反転はRP2350.ino側で処理済み)
    double stickX = cx + (x_val - 128) * radius / 128.0;
    double stickY = cy + (y_val - 128) * radius / 128.0;  // Y軸反転済み
    
    // スティック位置を制限
    double dx = stickX - cx;
    double dy = stickY - cy;
    double distance = sqrt(dx*dx + dy*dy);
    if (distance > radius) {
        stickX = cx + dx * radius / distance;
        stickY = cy + dy * radius / distance;
    }
    
    // スティックを描画
    gc->SetBrush(wxBrush(wxColour(200, 200, 255)));
    gc->SetPen(wxPen(wxColour(255, 255, 255), 2));
    gc->DrawEllipse(stickX - 5, stickY - 5, 10, 10);
}

std::vector<wxPoint2DDouble> CreateHomeBase(int cx, int cy, int size, double angle) {
    std::vector<wxPoint2DDouble> points(5);

    double base[5][2];
    if (angle == 0 || angle == M_PI) {
        base[0][0] =  0, base[0][1] = 0; // 先端
        base[1][0] =  1, base[1][1] = 1; // 右上
        base[2][0] =  1, base[2][1] = 3; // 右下
        base[3][0] = -1, base[3][1] = 3; // 左下
        base[4][0] = -1, base[4][1] = 1; // 左上

        if (angle == M_PI) {
            for (int i = 0; i < 5; ++i) {
                base[i][0] = -base[i][0];
                base[i][1] = -base[i][1];
            }
        }
    }

    if (angle == M_PI / 2 || angle == 3 * M_PI / 2) {
        base[0][0] =  0, base[0][1] =  0; // 先端
        base[1][0] = -1, base[1][1] =  1; // 右上
        base[2][0] = -3, base[2][1] =  1; // 右下
        base[3][0] = -3, base[3][1] = -1; // 左下
        base[4][0] = -1, base[4][1] = -1; // 左上

        if (angle == 3 * M_PI / 2) {
            for (int i = 0; i < 5; ++i) {
                base[i][0] = -base[i][0];
                base[i][1] = -base[i][1];
            }
        }
    }

    for (int i = 0; i < 5; ++i) {
        double ix = base[i][0] * size;
        double iy = base[i][1] * size;
        points[i].m_x = cx + ix;
        points[i].m_y = cy + iy;
    }
    return points;
}

void ControllerPanel::DrawDPad(wxGraphicsContext* gc, double cx, double cy, double size)
{
    // 十字キー
    gc->SetPen(wxPen(*wxWHITE, 2));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    int center_x = (int)cx;
    int center_y = (int)cy;
    int isize = (int)size;
    
    // 十字キーの座標計算
    short core = 2 * isize;
    short arm  = 2 * isize;

    std::vector<wxPoint2DDouble> pts;
    // 上
    pts.push_back(wxPoint2DDouble(center_x - core / 2, center_y - core / 2));
    pts.push_back(wxPoint2DDouble(center_x - core / 2, center_y - core / 2 - arm));
    pts.push_back(wxPoint2DDouble(center_x + core / 2, center_y - core / 2 - arm));
    
    // 右
    pts.push_back(wxPoint2DDouble(center_x + core / 2, center_y - core / 2));
    pts.push_back(wxPoint2DDouble(center_x + core / 2 + arm, center_y - core / 2));
    pts.push_back(wxPoint2DDouble(center_x + core / 2 + arm, center_y + core / 2));
    
    // 下
    pts.push_back(wxPoint2DDouble(center_x + core / 2, center_y + core / 2));
    pts.push_back(wxPoint2DDouble(center_x + core / 2, center_y + core / 2 + arm));
    pts.push_back(wxPoint2DDouble(center_x - core / 2, center_y + core / 2 + arm));
    
    // 左
    pts.push_back(wxPoint2DDouble(center_x - core / 2, center_y + core / 2));
    pts.push_back(wxPoint2DDouble(center_x - core / 2 - arm, center_y + core / 2));
    pts.push_back(wxPoint2DDouble(center_x - core / 2 - arm, center_y - core / 2));

    // 基本の十字形状を描画
    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(pts[0]);
    for (size_t i = 1; i < pts.size(); ++i) {
        path.AddLineToPoint(pts[i]);
    }
    path.CloseSubpath();
    gc->DrawPath(path);

    // 押下時の表示
    // 下
    auto homeBase = CreateHomeBase(center_x, center_y, isize, 0);
    if (controllerData.dpad & DPAD_DOWN) {
        gc->SetBrush(*wxWHITE);
        wxGraphicsPath homePath = gc->CreatePath();
        homePath.MoveToPoint(homeBase[0]);
        for (size_t i = 1; i < homeBase.size(); ++i) {
            homePath.AddLineToPoint(homeBase[i]);
        }
        homePath.CloseSubpath();
        gc->DrawPath(homePath);
    }
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    // 左
    homeBase = CreateHomeBase(center_x, center_y, isize, M_PI / 2);
    if (controllerData.dpad & DPAD_LEFT) {
        gc->SetBrush(*wxWHITE);
        wxGraphicsPath homePath = gc->CreatePath();
        homePath.MoveToPoint(homeBase[0]);
        for (size_t i = 1; i < homeBase.size(); ++i) {
            homePath.AddLineToPoint(homeBase[i]);
        }
        homePath.CloseSubpath();
        gc->DrawPath(homePath);
    }
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    // 上
    homeBase = CreateHomeBase(center_x, center_y, isize, M_PI);
    if (controllerData.dpad & DPAD_UP) {
        gc->SetBrush(*wxWHITE);
        wxGraphicsPath homePath = gc->CreatePath();
        homePath.MoveToPoint(homeBase[0]);
        for (size_t i = 1; i < homeBase.size(); ++i) {
            homePath.AddLineToPoint(homeBase[i]);
        }
        homePath.CloseSubpath();
        gc->DrawPath(homePath);
    }
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    // 右
    homeBase = CreateHomeBase(center_x, center_y, isize, 3 * M_PI / 2);
    if (controllerData.dpad & DPAD_RIGHT) {
        gc->SetBrush(*wxWHITE);
        wxGraphicsPath homePath = gc->CreatePath();
        homePath.MoveToPoint(homeBase[0]);
        for (size_t i = 1; i < homeBase.size(); ++i) {
            homePath.AddLineToPoint(homeBase[i]);
        }
        homePath.CloseSubpath();
        gc->DrawPath(homePath);
    }
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
}

// 記録・再生機能の実装

void ControllerPanel::StartRecording()
{
    if (!connected) return;
    
    std::cout << "DEBUG: StartRecording called - waiting for first input" << std::endl;
    
    recorded_data.clear();
    recording = true;
    recording_waiting_for_trigger = true;
    has_neutral_state = false;  // ニュートラル状態検出フラグをリセット
    current_mode = MODE_RECORDING;
    
    // 遅延補正を無効化：等倍速再生のため
    system_delay_ms = 0;  // 遅延補正なし
    delay_calibrated = true;
    
    std::cout << "DEBUG: Recording mode activated (no delay compensation for realtime playback)" << std::endl;
}

void ControllerPanel::StopRecording()
{
    std::cout << "DEBUG: StopRecording called" << std::endl;
    
    recording = false;
    recording_waiting_for_trigger = false;
    has_neutral_state = false;
    current_mode = MODE_PASSTHROUGH;
    
    // ATmega32U4にはモード変更を送信しない（PC側で制御）
    
    std::cout << "DEBUG: Recording stopped. " << recorded_data.size() << " frames recorded" << std::endl;
}

void ControllerPanel::StartPlayback(const wxString& filename)
{
    if (!connected) return;
    
    std::cout << "DEBUG: StartPlayback called: " << filename.ToStdString() << std::endl;
    
    // 前回の再生スレッドが残っていたら停止
    if (playback_thread.joinable()) {
        should_stop_playback = true;
        playback_thread.join();
    }
    
    if (!LoadPlaybackData(filename)) {
        std::cout << "ERROR: Failed to load playback data" << std::endl;
        return;
    }
    
    // 状態を完全に初期化
    playback_waiting_for_trigger = true;
    playback_active = false;
    should_stop_playback = false;
    playback_index = 0;
    playback_start_time = 0;  // タイミング制御の初期化
    has_neutral_state = false;  // ニュートラル状態検出フラグをリセット
    first_frame_preloaded = false;  // プリロード状態をリセット
    current_mode = MODE_PLAYBACK;
    
    // 遅延補正を無効化：等倍速再生のため
    system_delay_ms = 0;  // 遅延補正なし
    delay_calibrated = true;
    
    std::cout << "DEBUG: Playback ready - waiting for first input to trigger playback" << std::endl;
    
    // 再生スレッドを開始（トリガー待ち状態で）
    playback_thread = std::thread(&ControllerPanel::PlaybackThread, this);
}

void ControllerPanel::StopPlayback()
{
    std::cout << "DEBUG: StopPlayback called" << std::endl;
    
    should_stop_playback = true;
    
    if (playback_thread.joinable()) {
        playback_thread.join();
    }
    
    // 状態を完全にリセット
    playback_active = false;
    playback_waiting_for_trigger = false;
    should_stop_playback = false;
    playback_index = 0;
    playback_start_time = 0;  // タイミング制御のリセット
    first_frame_preloaded = false;  // プリロード状態をリセット
    has_neutral_state = false;
    current_mode = MODE_PASSTHROUGH;
    
    // ATmega32U4に再生停止を通知
    SendPlaybackStop();
}

bool ControllerPanel::SaveRecordingData(const wxString& filename)
{
    if (recorded_data.empty()) {
        std::cout << "ERROR: No recording data to save" << std::endl;
        return false;
    }
    
    std::cout << "DEBUG: Saving " << recorded_data.size() << " frames to " << filename.ToStdString() << std::endl;
    
    FILE* file = nullptr;
    _wfopen_s(&file, filename.wc_str(), L"wb");
    if (!file) {
        std::cout << "ERROR: Cannot open file for writing" << std::endl;
        return false;
    }
    
    // ヘッダー情報を保存
    uint32_t version = 1;
    uint32_t count = static_cast<uint32_t>(recorded_data.size());
    
    fwrite(&version, sizeof(version), 1, file);
    fwrite(&count, sizeof(count), 1, file);
    
    // データを保存
    for (const auto& data : recorded_data) {
        fwrite(&data, sizeof(TimestampedData), 1, file);
    }
    
    fclose(file);
    
    // 最初と最後のタイムスタンプをデバッグ出力
    if (!recorded_data.empty()) {
        std::cout << "DEBUG: Recording saved successfully. First: " << recorded_data[0].timestamp 
                  << "ms, Last: " << recorded_data.back().timestamp << "ms, Duration: " 
                  << (recorded_data.back().timestamp - recorded_data[0].timestamp) << "ms" << std::endl;
    }
    return true;
}

bool ControllerPanel::LoadPlaybackData(const wxString& filename)
{
    std::cout << "DEBUG: Loading playback data from " << filename.ToStdString() << std::endl;
    
    FILE* file = nullptr;
    _wfopen_s(&file, filename.wc_str(), L"rb");
    if (!file) {
        std::cout << "ERROR: Cannot open file for reading" << std::endl;
        return false;
    }
    
    // ヘッダー情報を読み込み
    uint32_t version, count;
    if (fread(&version, sizeof(version), 1, file) != 1 ||
        fread(&count, sizeof(count), 1, file) != 1) {
        std::cout << "ERROR: Failed to read file header" << std::endl;
        fclose(file);
        return false;
    }
    
    if (version != 1) {
        std::cout << "ERROR: Unsupported file version: " << version << std::endl;
        fclose(file);
        return false;
    }
    
    // データを読み込み
    playback_data.clear();
    playback_data.resize(count);
    
    size_t read_count = fread(playback_data.data(), sizeof(TimestampedData), count, file);
    fclose(file);
    
    if (read_count != count) {
        std::cout << "ERROR: Failed to read all data. Expected: " << count << ", Read: " << read_count << std::endl;
        return false;
    }
    
    // 読み込んだデータの最初と最後のタイムスタンプをデバッグ出力
    if (!playback_data.empty()) {
        std::cout << "DEBUG: Loaded " << playback_data.size() << " frames. First: " << playback_data[0].timestamp 
                  << "ms, Last: " << playback_data.back().timestamp << "ms, Duration: " 
                  << (playback_data.back().timestamp - playback_data[0].timestamp) << "ms" << std::endl;
        
        // 最初の数フレームのタイムスタンプを確認
        for (size_t i = 0; i < std::min((size_t)5, playback_data.size()); i++) {
            std::cout << "DEBUG: Frame " << i << " at " << playback_data[i].timestamp << "ms" << std::endl;
        }
    }
    
    return true;
}

// 削除された関数: SendModeChangeCommand（PC側制御により不要）

void ControllerPanel::SendPlaybackData(const ControllerData& data)
{
    if (hSerial == INVALID_HANDLE_VALUE) return;
    
    // 高精度パフォーマンス：スタティックバッファでメモリ確保を最小化
    static std::vector<uint8_t> packet;
    static std::vector<uint8_t> encoded;
    static bool initialized = false;
    
    if (!initialized) {
        packet.reserve(sizeof(ControllerData) + 1);
        encoded.reserve(sizeof(ControllerData) + 10); // COBSオーバーヘッド考慮
        initialized = true;
    }
    
    packet.clear();
    packet.push_back(CMD_PLAYBACK_DATA);
    
    // ControllerDataを直接コピー（最適化）
    const uint8_t* dataPtr = reinterpret_cast<const uint8_t*>(&data);
    packet.insert(packet.end(), dataPtr, dataPtr + sizeof(ControllerData));
    
    encoded.clear();
    if (EncodeCOBS(packet, encoded)) {
        encoded.push_back(0x00); // COBS terminator
        
        DWORD bytesWritten;
        // 同期送信で精度を保証
        if (!WriteFile(hSerial, encoded.data(), static_cast<DWORD>(encoded.size()), &bytesWritten, nullptr)) {
            // 送信エラーのログを最小化（パフォーマンス優先）
            static int error_count = 0;
            if (++error_count < 3) {
                std::cout << "DEBUG: Serial write error " << error_count << std::endl;
            }
        }
    }
}

bool ControllerPanel::EncodeCOBS(const std::vector<uint8_t>& data, std::vector<uint8_t>& encoded)
{
    encoded.clear();
    encoded.reserve(data.size() + data.size() / 254 + 1);
    
    size_t read_index = 0;
    
    while (read_index < data.size()) {
        size_t code_index = encoded.size();
        encoded.push_back(0); // プレースホルダー
        
        uint8_t code = 1;
        while (read_index < data.size() && data[read_index] != 0 && code < 0xFF) {
            encoded.push_back(data[read_index++]);
            code++;
        }
        
        encoded[code_index] = code;
        
        if (read_index < data.size() && data[read_index] == 0) {
            read_index++; // ゼロバイトをスキップ
        }
    }
    
    return true;
}

void ControllerPanel::RecordData(const ControllerData& data)
{
    if (!recording || recording_waiting_for_trigger) return;
    
    uint32_t current_time = GetTickCount();
    
    TimestampedData timestamped;
    timestamped.timestamp = current_time - recording_start_time;
    timestamped.data = data;
    
    recorded_data.push_back(timestamped);
    
    // 最初の10フレームのタイムスタンプをデバッグ出力
    if (recorded_data.size() <= 10) {
        std::cout << "DEBUG: Record frame " << recorded_data.size() << " at " << timestamped.timestamp << "ms" << std::endl;
    }
}

void ControllerPanel::PlaybackThread()
{
    if (playback_data.empty()) {
        std::cout << "ERROR: No playback data" << std::endl;
        return;
    }
    
    std::cout << "DEBUG: Playback thread started - waiting for trigger" << std::endl;
    
    // トリガー待ち（高速化）
    while (playback_waiting_for_trigger && !should_stop_playback) {
        Sleep(2); // 5msから2msに短縮
    }
    
    if (should_stop_playback) {
        std::cout << "DEBUG: Playback stopped before trigger" << std::endl;
        return;
    }
    
    std::cout << "DEBUG: Playback thread - starting high-precision timing control" << std::endl;
    
    // 高精度タイマーを使用（QueryPerformanceCounter）
    LARGE_INTEGER frequency, start_time, current_time_precise;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start_time);
    
    uint32_t frame_count = 0;
    uint32_t last_debug_time = 0;
    uint32_t missed_frames = 0;
    
    // プリバッファリング：最初の数フレームを事前送信
    int prebuffer_count = std::min(3, (int)playback_data.size());
    for (int i = 0; i < prebuffer_count && !should_stop_playback; i++) {
        SendPlaybackData(playback_data[i].data);
        playback_index++;
        frame_count++;
    }
    
    if (playback_index > 0) {
        std::cout << "DEBUG: Prebuffered " << prebuffer_count << " frames" << std::endl;
    }
    
    while (!should_stop_playback && playback_index < playback_data.size()) {
        // 高精度時刻取得
        QueryPerformanceCounter(&current_time_precise);
        uint32_t elapsed_ms = (uint32_t)(((current_time_precise.QuadPart - start_time.QuadPart) * 1000) / frequency.QuadPart);
        
        uint32_t target_time = playback_data[playback_index].timestamp;
        
        // フレームが時間通りに来たかチェック
        if (elapsed_ms >= target_time) {
            // 遅延チェック：30ms以上遅れた場合は警告
            int32_t delay = elapsed_ms - target_time;
            if (delay > 30) {
                if (missed_frames < 5) {
                    std::cout << "DEBUG: Late frame " << playback_index << " (delay: " << delay << "ms)" << std::endl;
                }
                missed_frames++;
            }
            
            // フレーム送信
            SendPlaybackData(playback_data[playback_index].data);
            playback_index++;
            frame_count++;
            
            // 最初の10フレームのタイミングをデバッグ出力
            if (playback_index <= 10) {
                std::cout << "DEBUG: Frame " << playback_index-1 << " sent at " << elapsed_ms << "ms (target: " << target_time << "ms, precision: " << delay << "ms)" << std::endl;
            }
        } else {
            // 次のフレームまでの待機時間を計算
            uint32_t wait_time = target_time - elapsed_ms;
            
            if (wait_time > 10) {
                // 10ms以上の場合はSleepで待機
                Sleep(wait_time - 5); // 5ms手前で起きて精度を上げる
            } else if (wait_time > 2) {
                // 2-10msの場合は短いSleep
                Sleep(1);
            }
            // 2ms以下の場合はビジーウェイト（精度優先）
        }
        
        // 1秒ごとに統計情報を表示
        if (elapsed_ms - last_debug_time >= 1000) {
            double actual_fps = frame_count * 1000.0 / elapsed_ms;
            std::cout << "DEBUG: Playback stats - " << frame_count << " frames, " << actual_fps << " fps, " << missed_frames << " late frames" << std::endl;
            last_debug_time = elapsed_ms;
        }
    }
    
    if (playback_index >= playback_data.size()) {
        std::cout << "DEBUG: Playback completed" << std::endl;
        // ATmega32U4に再生停止を通知
        SendPlaybackStop();
        
        // 再生完了時の状態リセット
        playback_active = false;
        playback_waiting_for_trigger = false;
        should_stop_playback = false;
        playback_index = 0;
        has_neutral_state = false;
        current_mode = MODE_PASSTHROUGH;
        
        // 親ウィンドウに再生完了を通知（UIスレッドで実行される）
        wxCommandEvent event(wxEVT_PLAYBACK_COMPLETED);
        wxPostEvent(GetParent(), event);
    } else {
        std::cout << "DEBUG: Playback stopped by user" << std::endl;
        SendPlaybackStop();
    }
}

// PC側でのデータ処理（メイン処理）
void ControllerPanel::ProcessControllerData(const ControllerData& data)
{
    // 1. 記録モード時の処理
    if (recording) {
        if (recording_waiting_for_trigger) {
            CheckRecordingTrigger(data);
        } else {
            RecordData(data);
        }
    }
    
    // 2. 再生モード時のトリガー検出
    if (playback_waiting_for_trigger) {
        CheckPlaybackTrigger(data);
    }
}

// 記録トリガー検出
void ControllerPanel::CheckRecordingTrigger(const ControllerData& data)
{
    // ニュートラル状態を初めて検出したらフラグを立てる
    if (!has_neutral_state && IsNeutralState(data)) {
        has_neutral_state = true;
        std::cout << "DEBUG: Recording - Neutral state detected, waiting for input..." << std::endl;
        return;
    }
    
    // ニュートラル状態検出後、最初の入力で記録開始
    if (has_neutral_state && !IsNeutralState(data)) {
        std::cout << "DEBUG: Recording - First input detected, starting recording..." << std::endl;
        recording_waiting_for_trigger = false;
        
        // 正常な時刻で記録開始（遅延補正なし）
        recording_start_time = GetTickCount();
        recorded_data.clear();
        
        // トリガーとなったフレームから記録開始（タイムスタンプ0から）
        TimestampedData firstFrame;
        firstFrame.timestamp = 0;  // 最初のフレームは0msから開始
        firstFrame.data = data;
        recorded_data.push_back(firstFrame);
        
        std::cout << "DEBUG: Recording - First frame recorded at timestamp 0ms" << std::endl;
    }
}

// 再生トリガー検出（簡素化版）
void ControllerPanel::CheckPlaybackTrigger(const ControllerData& data)
{
    if (!playback_waiting_for_trigger) return;
    
    // ニュートラル状態を初めて検出したらフラグを立てる
    if (!has_neutral_state && IsNeutralState(data)) {
        has_neutral_state = true;
        std::cout << "DEBUG: Playback - Neutral state detected, preloading first frame..." << std::endl;
        
        // ニュートラル状態検出時に最初のフレームをプリロード
        PreloadFirstFrame();
        return;
    }
    
    // ニュートラル状態検出後、最初の入力で再生開始
    if (has_neutral_state && !IsNeutralState(data)) {
        std::cout << "DEBUG: Playback - First input detected, starting immediate playback..." << std::endl;
        playback_waiting_for_trigger = false;
        playback_active = true;
        
        // 再生モードを開始
        SendPlaybackStart();
        
        // 即座再生開始（遅延補正なし）
        playback_start_time = GetTickCount();
        
        // 最初のフレームがプリロード済みの場合はスキップ
        if (first_frame_preloaded && !playback_data.empty()) {
            playback_index = 1;  // 2番目のフレームから開始
            std::cout << "DEBUG: Playback - First frame already preloaded, starting from index 1" << std::endl;
        } else {
            playback_index = 0;
        }
    }
}

// ニュートラル状態判定
bool ControllerPanel::IsNeutralState(const ControllerData& data)
{
    return data.buttons == 0 && 
           data.dpad == 0 && 
           data.stick_lx == 128 && 
           data.stick_ly == 128 && 
           data.stick_rx == 128 && 
           data.stick_ry == 128;
}

// 簡素化されたプロトコル関数
void ControllerPanel::SendPlaybackStart()
{
    std::vector<uint8_t> command = { CMD_PLAYBACK_START };
    std::vector<uint8_t> encoded;
    if (EncodeCOBS(command, encoded)) {
        WriteFile(hSerial, encoded.data(), encoded.size(), NULL, NULL);
        uint8_t terminator = 0x00;
        WriteFile(hSerial, &terminator, 1, NULL, NULL);
        std::cout << "DEBUG: Sent playback start command" << std::endl;
    }
}

void ControllerPanel::SendPlaybackStop()
{
    std::vector<uint8_t> command = { CMD_PLAYBACK_STOP };
    std::vector<uint8_t> encoded;
    if (EncodeCOBS(command, encoded)) {
        WriteFile(hSerial, encoded.data(), encoded.size(), NULL, NULL);
        uint8_t terminator = 0x00;
        WriteFile(hSerial, &terminator, 1, NULL, NULL);
        std::cout << "DEBUG: Sent playback stop command" << std::endl;
    }
}

void ControllerPanel::SendPreloadFrame(const ControllerData& data)
{
    if (hSerial == INVALID_HANDLE_VALUE) return;
    
    std::vector<uint8_t> packet;
    packet.push_back(CMD_PRELOAD_FRAME);
    
    // ControllerDataをバイト配列として追加
    const uint8_t* dataPtr = reinterpret_cast<const uint8_t*>(&data);
    packet.insert(packet.end(), dataPtr, dataPtr + sizeof(ControllerData));
    
    std::vector<uint8_t> encoded;
    if (EncodeCOBS(packet, encoded)) {
        encoded.push_back(0x00); // COBS terminator
        
        DWORD bytesWritten;
        WriteFile(hSerial, encoded.data(), static_cast<DWORD>(encoded.size()), &bytesWritten, nullptr);
        std::cout << "DEBUG: Sent preload frame command" << std::endl;
    }
}

// 遅延補正機能（簡素化）
void ControllerPanel::CalibrateDelay()
{
    // 簡素化：固定値を使用して処理速度を向上
    system_delay_ms = 30;  // 固定遅延値
    delay_calibrated = true;
    std::cout << "DEBUG: Using fixed system delay: " << system_delay_ms << "ms (optimized)" << std::endl;
}

uint32_t ControllerPanel::MeasureRoundTripDelay()
{
    // 簡素化：固定値を返す
    return 60;  // 固定往復遅延（30ms x 2）
}

void ControllerPanel::PreloadFirstFrame()
{
    if (playback_data.empty() || first_frame_preloaded) return;
    
    // プリロード専用コマンドで送信（モード切り替えなし）
    if (!playback_data.empty()) {
        SendPreloadFrame(playback_data[0].data);
        first_frame_preloaded = true;
        std::cout << "DEBUG: First frame preloaded to ATmega32U4 (mode unchanged)" << std::endl;
    }
}