#include "ControllerPanel.h"
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
        wxLogError("Serial connection error: %s", e.what());
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
        wxLogError("Failed to open serial port: %s", currentPort);
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
                                wxLogMessage("First valid data received: buttons=0x%04X, dpad=0x%02X", 
                                           newData.buttons, newData.dpad);
                                firstDataReceived = true;
                            }
                        } else {
                            // 詳細なデバッグ情報
                            static int errorCount = 0;
                            if (errorCount++ < 5) { // 最初の5回のエラーのみ表示
                                wxString dataHex;
                                for (size_t i = 0; i < decoded.size(); i++) {
                                    dataHex += wxString::Format("%02X ", decoded[i]);
                                }
                                wxLogMessage("CRC mismatch #%d: expected=0x%02X, calculated=0x%02X, size=%zu, data=[%s]", 
                                           errorCount, receivedCRC, calculatedCRC, decoded.size(), dataHex);
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
    wxSize size = GetSize();
    double centerX = size.x / 2.0;
    double centerY = size.y / 2.0;
    
    std::lock_guard<std::mutex> lock(dataMutex);
    
    if (!dataValid) {
        gc->SetBrush(wxBrush(wxColour(255, 255, 255)));
        wxGraphicsFont statusFont = gc->CreateFont(wxFont(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD), wxColour(255, 255, 255));
        gc->SetFont(statusFont);
        
        wxString status = connected ? "Waiting for data..." : "Not connected";
        double textWidth, textHeight;
        gc->GetTextExtent(status, &textWidth, &textHeight);
        gc->DrawText(status, centerX - textWidth/2, centerY - textHeight/2);
        return;
    }
    
    // 左スティック
    DrawStick(gc, centerX - 120, centerY + 40, 30, controllerData.stick_lx, controllerData.stick_ly);
    
    // 右スティック  
    DrawStick(gc, centerX + 120, centerY + 40, 30, controllerData.stick_rx, controllerData.stick_ry);
    
    // D-Pad
    DrawDPad(gc, centerX - 80, centerY - 20, 25, controllerData.dpad);
    
    // 右側ボタン (A, B, X, Y)
    DrawButton(gc, centerX + 80, centerY - 20, 15, controllerData.buttons & BTN_A, "A");
    DrawButton(gc, centerX + 80 + 25, centerY - 20 - 25, 15, controllerData.buttons & BTN_X, "X");
    DrawButton(gc, centerX + 80 - 25, centerY - 20 - 25, 15, controllerData.buttons & BTN_Y, "Y");
    DrawButton(gc, centerX + 80, centerY - 20 - 50, 15, controllerData.buttons & BTN_B, "B");
    
    // 肩ボタン
    DrawButton(gc, centerX - 100, centerY - 80, 12, controllerData.buttons & BTN_L, "L");
    DrawButton(gc, centerX + 100, centerY - 80, 12, controllerData.buttons & BTN_R, "R");
    DrawButton(gc, centerX - 120, centerY - 80, 12, controllerData.buttons & BTN_ZL, "ZL");
    DrawButton(gc, centerX + 120, centerY - 80, 12, controllerData.buttons & BTN_ZR, "ZR");
    
    // 中央ボタン
    DrawButton(gc, centerX - 30, centerY - 50, 10, controllerData.buttons & BTN_MINUS, "-");
    DrawButton(gc, centerX + 30, centerY - 50, 10, controllerData.buttons & BTN_PLUS, "+");
    DrawButton(gc, centerX, centerY - 50, 10, controllerData.buttons & BTN_HOME, "H");
    DrawButton(gc, centerX, centerY - 30, 8, controllerData.buttons & BTN_CAPTURE, "C");
    
    // スティッククリック
    if (controllerData.buttons & BTN_L_STICK) {
        gc->SetBrush(wxBrush(wxColour(255, 0, 0, 100)));
        gc->DrawEllipse(centerX - 150, centerY + 10, 60, 60);
    }
    if (controllerData.buttons & BTN_R_STICK) {
        gc->SetBrush(wxBrush(wxColour(255, 0, 0, 100)));
        gc->DrawEllipse(centerX + 90, centerY + 10, 60, 60);
    }
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
    
    // スティック位置を計算 (128が中央、Y軸は反転)
    double stickX = cx + (x_val - 128) * radius / 128.0;
    double stickY = cy - (y_val - 128) * radius / 128.0;  // Y軸反転
    
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

void ControllerPanel::DrawDPad(wxGraphicsContext* gc, double cx, double cy, double size, uint8_t dpad_state)
{
    double halfSize = size / 2;
    
    // 上
    wxColour upColor = (dpad_state & DPAD_UP) ? wxColour(255, 100, 100) : wxColour(100, 100, 100);
    gc->SetBrush(wxBrush(upColor));
    gc->SetPen(wxPen(wxColour(200, 200, 200), 1));
    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(cx, cy - size);
    path.AddLineToPoint(cx - halfSize/2, cy - halfSize);
    path.AddLineToPoint(cx + halfSize/2, cy - halfSize);
    path.CloseSubpath();
    gc->FillPath(path);
    
    // 下
    wxColour downColor = (dpad_state & DPAD_DOWN) ? wxColour(255, 100, 100) : wxColour(100, 100, 100);
    gc->SetBrush(wxBrush(downColor));
    path = gc->CreatePath();
    path.MoveToPoint(cx, cy + size);
    path.AddLineToPoint(cx - halfSize/2, cy + halfSize);
    path.AddLineToPoint(cx + halfSize/2, cy + halfSize);
    path.CloseSubpath();
    gc->FillPath(path);
    
    // 左
    wxColour leftColor = (dpad_state & DPAD_LEFT) ? wxColour(255, 100, 100) : wxColour(100, 100, 100);
    gc->SetBrush(wxBrush(leftColor));
    path = gc->CreatePath();
    path.MoveToPoint(cx - size, cy);
    path.AddLineToPoint(cx - halfSize, cy - halfSize/2);
    path.AddLineToPoint(cx - halfSize, cy + halfSize/2);
    path.CloseSubpath();
    gc->FillPath(path);
    
    // 右
    wxColour rightColor = (dpad_state & DPAD_RIGHT) ? wxColour(255, 100, 100) : wxColour(100, 100, 100);
    gc->SetBrush(wxBrush(rightColor));
    path = gc->CreatePath();
    path.MoveToPoint(cx + size, cy);
    path.AddLineToPoint(cx + halfSize, cy - halfSize/2);
    path.AddLineToPoint(cx + halfSize, cy + halfSize/2);
    path.CloseSubpath();
    gc->FillPath(path);
}