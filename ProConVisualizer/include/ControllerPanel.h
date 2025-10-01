#pragma once

#include <wx/wx.h>
#include <wx/dcgraph.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <windows.h>

// カスタムイベント：再生完了通知
wxDECLARE_EVENT(wxEVT_PLAYBACK_COMPLETED, wxCommandEvent);

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

// 動作モード定義
enum OperationMode {
    MODE_PASSTHROUGH = 0,  // パススルーモード（通常動作）
    MODE_RECORDING = 1,    // 記録モード
    MODE_PLAYBACK = 2      // 再生モード
};

// プロトコル定義（簡素化版）
#define CMD_PLAYBACK_START 0xA0
#define CMD_PLAYBACK_DATA 0xA1
#define CMD_PLAYBACK_STOP 0xA2
#define CMD_PRELOAD_FRAME 0xA3        // プリロード専用コマンド
#define CMD_DATA_PACKET 0xB0        // ATmega32U4からのデータ受信

// タイムスタンプ付きデータ構造
struct TimestampedData {
    uint32_t timestamp;  // ミリ秒単位のタイムスタンプ
    ControllerData data;
};

class ControllerPanel : public wxPanel
{
public:
    ControllerPanel(wxWindow* parent);
    ~ControllerPanel();
    
    bool ConnectSerial(const wxString& portName);
    void DisconnectSerial();
    bool IsConnected() const { return connected; }
    
    // 記録・再生機能
    void StartRecording();
    void StopRecording();
    void StartPlayback(const wxString& filename);
    void StopPlayback();
    bool IsRecording() const { return recording; }
    bool IsPlayingBack() const { return playback_active; }
    bool IsWaitingForRecordingTrigger() const { return recording_waiting_for_trigger; }
    bool IsWaitingForPlaybackTrigger() const { return playback_waiting_for_trigger; }
    OperationMode GetCurrentMode() const { return current_mode; }
    bool SaveRecordingData(const wxString& filename);
    bool LoadPlaybackData(const wxString& filename);

private:
    void OnPaint(wxPaintEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnSize(wxSizeEvent& event);
    
    void SerialReadThread();
    bool DecodeCOBS(const std::vector<uint8_t>& encoded, std::vector<uint8_t>& decoded);
    uint8_t CalculateCRC8(const uint8_t* data, size_t length);
    
    // プロトコル関連
    void SendPlaybackStart();
    void SendPlaybackData(const ControllerData& data);
    void SendPlaybackStop();
    void SendPreloadFrame(const ControllerData& data);  // プリロード専用
    bool EncodeCOBS(const std::vector<uint8_t>& data, std::vector<uint8_t>& encoded);
    void ProcessControllerData(const ControllerData& data);  // PC側でのデータ処理
    
    // 記録・再生内部処理（PC側中心）
    void PlaybackThread();
    void RecordData(const ControllerData& data);
    void CheckRecordingTrigger(const ControllerData& data);
    void CheckPlaybackTrigger(const ControllerData& data);
    bool IsNeutralState(const ControllerData& data);
    
    // 遅延補正関連
    void CalibrateDelay();
    uint32_t MeasureRoundTripDelay();
    void PreloadFirstFrame();
    
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
    
    // 記録・再生関連（PC側で制御）
    std::atomic<bool> recording{false};
    std::atomic<bool> playback_active{false};
    std::atomic<bool> recording_waiting_for_trigger{false};
    std::atomic<bool> playback_waiting_for_trigger{false};
    std::atomic<OperationMode> current_mode{MODE_PASSTHROUGH};
    std::vector<TimestampedData> recorded_data;
    std::vector<TimestampedData> playback_data;
    std::thread playback_thread;
    std::atomic<bool> should_stop_playback{false};
    uint32_t recording_start_time{0};
    uint32_t playback_start_time{0};
    size_t playback_index{0};
    bool has_neutral_state{false};  // ニュートラル状態の検出用
    
    // 遅延補正関連
    uint32_t system_delay_ms{50};  // システム遅延(ミリ秒)
    bool delay_calibrated{false};
    bool first_frame_preloaded{false};
    
    wxDECLARE_EVENT_TABLE();
};