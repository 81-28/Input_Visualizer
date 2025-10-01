#include "../include/ControllerPanel.h"
#include <wx/graphics.h>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

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
}

ControllerPanel::~ControllerPanel()
{
    DisconnectSerial();
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
        return true;
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
    }
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
                    if (DecodeCOBS(packetBuffer, decoded) && decoded.size() == sizeof(ControllerData)) {
                        ControllerData newData;
                        memcpy(&newData, decoded.data(), sizeof(ControllerData));
                        
                        // CRCチェック（RP2350.inoと同じ方式）
                        uint8_t receivedCRC = newData.crc;
                        newData.crc = 0;  // CRCフィールドを0にしてから計算
                        uint8_t calculatedCRC = CalculateCRC8((uint8_t*)&newData, sizeof(ControllerData));
                        if (calculatedCRC == receivedCRC) {
                            newData.crc = receivedCRC;  // CRC値を復元
                            std::lock_guard<std::mutex> lock(dataMutex);
                            controllerData = newData;
                            dataValid = true;
                            
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