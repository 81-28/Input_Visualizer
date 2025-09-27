#pragma once

#include <wx/wx.h>
#include <wx/dcgraph.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <windows.h>

// RP2350.inoからのControllerData構造体
struct ControllerData {
    uint16_t buttons;
    uint8_t dpad;
    uint8_t stick_lx, stick_ly, stick_rx, stick_ry;
    uint8_t crc;
};

// ボタン定義（RP2350.inoと同じ）
#define BTN_A       (1 << 0)
#define BTN_B       (1 << 1)
#define BTN_X       (1 << 2)
#define BTN_Y       (1 << 3)
#define BTN_L       (1 << 4)
#define BTN_R       (1 << 5)
#define BTN_ZL      (1 << 6)
#define BTN_ZR      (1 << 7)
#define BTN_MINUS   (1 << 8)
#define BTN_PLUS    (1 << 9)
#define BTN_L_STICK (1 << 10)
#define BTN_R_STICK (1 << 11)
#define BTN_HOME    (1 << 12)
#define BTN_CAPTURE (1 << 13)

#define DPAD_UP    (1 << 0)
#define DPAD_DOWN  (1 << 1)
#define DPAD_LEFT  (1 << 2)
#define DPAD_RIGHT (1 << 3)

class ControllerPanel : public wxPanel
{
public:
    ControllerPanel(wxWindow* parent);
    ~ControllerPanel();
    
    bool ConnectSerial(const wxString& portName);
    void DisconnectSerial();
    bool IsConnected() const { return connected; }

private:
    void OnPaint(wxPaintEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnSize(wxSizeEvent& event);
    
    void SerialReadThread();
    bool DecodeCOBS(const std::vector<uint8_t>& encoded, std::vector<uint8_t>& decoded);
    uint8_t CalculateCRC8(const uint8_t* data, size_t length);
    
    void DrawController(wxGraphicsContext* gc);
    void DrawButton(wxGraphicsContext* gc, double x, double y, double radius, bool pressed, const wxString& label);
    void DrawStick(wxGraphicsContext* gc, double cx, double cy, double radius, uint8_t x_val, uint8_t y_val);
    void DrawDPad(wxGraphicsContext* gc, double cx, double cy, double size);
    
    wxTimer m_timer;
    
    // シリアル通信関連
    std::atomic<bool> connected{false};
    std::atomic<bool> shouldStop{false};
    std::thread serialThread;
    wxString currentPort;
    
    // Windows API シリアル通信関連
    HANDLE hSerial;
    
    // コントローラーデータ
    std::mutex dataMutex;
    ControllerData controllerData{};
    bool dataValid{false};
    
    wxDECLARE_EVENT_TABLE();
};