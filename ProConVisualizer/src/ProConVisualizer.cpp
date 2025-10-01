#include <wx/wx.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/timer.h>
#include <wx/datetime.h>
#include <wx/filename.h>
#include <wx/dir.h>
#include <iostream>
#include "../include/ControllerPanel.h"
#include "../include/SerialUtils.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

class ProConVisualizerApp : public wxApp
{
public:
    bool OnInit() override;
};

class MainFrame : public wxFrame
{
public:
    MainFrame();

private:
    void OnConnect(wxCommandEvent& event);
    void OnRefresh(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnToggleRecording(wxCommandEvent& event);
    void OnTogglePlayback(wxCommandEvent& event);
    void OnLoadPlayback(wxCommandEvent& event);
    void OnUpdateTimer(wxTimerEvent& event);
    void OnPlaybackCompleted(wxCommandEvent& event);  // 再生完了イベントハンドラー
    void PopulateComPorts();
    void UpdateModeDisplay();
    void ResetPlaybackUI();  // 再生終了時の共通UI処理
    void UpdateConnectionState();  // 接続状態に応じたUI更新
    wxString GenerateAutoFilename();
    
    ControllerPanel* m_controllerPanel;
    wxChoice* m_comChoice;
    wxButton* m_connectButton;
    wxButton* m_refreshButton;
    wxButton* m_recordToggleButton;  // 統合ボタン
    wxButton* m_playbackToggleButton;  // 統合ボタン
    wxButton* m_loadPlaybackButton;
    wxStaticText* m_modeStatusText;
    wxStaticText* m_selectedFileText;  // 選択されたファイル表示
    wxString m_lastSelectedFile;  // 最後に選択したファイル
    wxTimer m_updateTimer;
    
    enum {
        ID_CONNECT = 1000,
        ID_REFRESH = 1001,
        ID_TOGGLE_RECORDING = 1002,
        ID_TOGGLE_PLAYBACK = 1003,
        ID_LOAD_PLAYBACK = 1004,
        ID_UPDATE_TIMER = 1005
    };
    
    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON(ID_CONNECT, MainFrame::OnConnect)
    EVT_BUTTON(ID_REFRESH, MainFrame::OnRefresh)
    EVT_BUTTON(ID_TOGGLE_RECORDING, MainFrame::OnToggleRecording)
    EVT_BUTTON(ID_TOGGLE_PLAYBACK, MainFrame::OnTogglePlayback)
    EVT_BUTTON(ID_LOAD_PLAYBACK, MainFrame::OnLoadPlayback)
    EVT_TIMER(ID_UPDATE_TIMER, MainFrame::OnUpdateTimer)
    EVT_CLOSE(MainFrame::OnClose)
    EVT_COMMAND(wxID_ANY, wxEVT_PLAYBACK_COMPLETED, MainFrame::OnPlaybackCompleted)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP(ProConVisualizerApp);

bool ProConVisualizerApp::OnInit()
{
    // デバッグコンソールを有効化
#ifdef _WIN32
    if (AllocConsole()) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        std::cout << "Debug console started" << std::endl;
    }
#endif

    MainFrame* frame = new MainFrame();
    frame->Show(true);
    return true;
}

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "Pro Controller Visualizer", wxDefaultPosition, wxSize(800, 600)),
      m_updateTimer(this, ID_UPDATE_TIMER)
{
    m_controllerPanel = new ControllerPanel(this);
    
    wxPanel* topPanel = new wxPanel(this);
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);

    // 接続パネル
    wxPanel* connectionPanel = new wxPanel(topPanel);
    wxBoxSizer* connectionSizer = new wxBoxSizer(wxHORIZONTAL);
    
    m_comChoice = new wxChoice(connectionPanel, wxID_ANY);
    m_connectButton = new wxButton(connectionPanel, ID_CONNECT, "Connect");
    m_refreshButton = new wxButton(connectionPanel, ID_REFRESH, "Refresh");

    PopulateComPorts();

    connectionSizer->Add(m_comChoice, 1, wxEXPAND | wxRIGHT, 5);
    connectionSizer->Add(m_connectButton, 0, wxEXPAND | wxRIGHT, 5);
    connectionSizer->Add(m_refreshButton, 0, wxEXPAND);
    connectionPanel->SetSizer(connectionSizer);

    // 記録・再生パネル
    wxPanel* recordPlayPanel = new wxPanel(topPanel);
    wxBoxSizer* recordPlaySizer = new wxBoxSizer(wxVERTICAL);
    
    // 大型ボタン作成（2倍サイズ）
    wxFont buttonFont = wxFont(14, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    
    // 統合記録ボタン（スタート/ストップ）
    m_recordToggleButton = new wxButton(recordPlayPanel, ID_TOGGLE_RECORDING, "Start Recording", 
                                       wxDefaultPosition, wxSize(200, 50));
    m_recordToggleButton->SetFont(buttonFont);
    m_recordToggleButton->SetBackgroundColour(wxColour(100, 200, 100));
    
    // 統合再生ボタン（スタート/ストップ）  
    m_playbackToggleButton = new wxButton(recordPlayPanel, ID_TOGGLE_PLAYBACK, "Start Playback",
                                         wxDefaultPosition, wxSize(200, 50));
    m_playbackToggleButton->SetFont(buttonFont);
    m_playbackToggleButton->SetBackgroundColour(wxColour(100, 150, 200));
    
    // ファイル読み込みボタン
    m_loadPlaybackButton = new wxButton(recordPlayPanel, ID_LOAD_PLAYBACK, "Load Playback File",
                                       wxDefaultPosition, wxSize(200, 40));
    
    // 選択されたファイル表示
    m_selectedFileText = new wxStaticText(recordPlayPanel, wxID_ANY, "No file selected");
    
    // 初期状態では録画・再生ボタンを無効化（接続されていないため）
    m_recordToggleButton->Enable(false);
    m_playbackToggleButton->Enable(false);
    
    // ボタンレイアウト
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(m_recordToggleButton, 0, wxEXPAND | wxRIGHT, 10);
    buttonSizer->Add(m_playbackToggleButton, 0, wxEXPAND);
    
    recordPlaySizer->Add(buttonSizer, 0, wxEXPAND | wxBOTTOM, 10);
    recordPlaySizer->Add(m_loadPlaybackButton, 0, wxEXPAND | wxBOTTOM, 5);
    recordPlaySizer->Add(m_selectedFileText, 0, wxEXPAND);
    recordPlayPanel->SetSizer(recordPlaySizer);

    // モード表示
    m_modeStatusText = new wxStaticText(topPanel, wxID_ANY, "Mode: Passthrough");
    wxFont font = m_modeStatusText->GetFont();
    font.SetWeight(wxFONTWEIGHT_BOLD);
    m_modeStatusText->SetFont(font);

    topSizer->Add(connectionPanel, 0, wxEXPAND | wxALL, 5);
    topSizer->Add(recordPlayPanel, 0, wxEXPAND | wxALL, 5);
    topSizer->Add(m_modeStatusText, 0, wxEXPAND | wxALL, 5);
    topPanel->SetSizer(topSizer);

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(topPanel, 0, wxEXPAND);
    mainSizer->Add(m_controllerPanel, 1, wxEXPAND);
    this->SetSizer(mainSizer);

    Centre();
    
    // 定期更新タイマーを開始（500ms間隔）
    m_updateTimer.Start(500);
}

void MainFrame::PopulateComPorts()
{
    m_comChoice->Clear();
    
    auto ports = SerialUtils::AvailablePorts();
    for (const auto& p : ports) {
        m_comChoice->Append(wxString::Format("%s (%s)", p.port, p.description));
    }
    
    if (m_comChoice->GetCount() > 0) {
        m_comChoice->SetSelection(0);
    }
}

void MainFrame::OnConnect(wxCommandEvent& event)
{
    if (m_controllerPanel->IsConnected()) {
        m_controllerPanel->DisconnectSerial();
        m_connectButton->SetLabel("Connect");
        PopulateComPorts();
        std::cout << "Disconnected from device" << std::endl;
    } else {
        int sel = m_comChoice->GetSelection();
        if (sel == wxNOT_FOUND) {
            std::cout << "ERROR: Please select a COM port." << std::endl;
            return;
        }

        wxString portStr = m_comChoice->GetString(sel);
        portStr = portStr.BeforeFirst('('); // Extract port name before '('
        portStr.Trim(); // Remove any trailing spaces

        if (m_controllerPanel->ConnectSerial(portStr)) {
            m_connectButton->SetLabel("Disconnect");
            std::cout << "Connected to: " << portStr.ToStdString() << std::endl;
        } else {
            std::cout << "ERROR: Wrong port or device not connected: " << portStr.ToStdString() << std::endl;
        }
    }
    
    // 接続状態に応じてボタンの有効/無効を更新
    UpdateConnectionState();
}

void MainFrame::OnRefresh(wxCommandEvent& event)
{
    std::cout << "Refresh button clicked" << std::endl;
    
    wxString selectedPort;
    if (m_comChoice->GetSelection() != wxNOT_FOUND) {
        selectedPort = m_comChoice->GetString(m_comChoice->GetSelection());
    }
    
    PopulateComPorts();
    
    // 以前に選択されていたポートがあれば再選択
    if (!selectedPort.IsEmpty()) {
        int index = m_comChoice->FindString(selectedPort);
        if (index != wxNOT_FOUND) {
            m_comChoice->SetSelection(index);
        }
    }
    
    std::cout << "COM ports refreshed. Found " << m_comChoice->GetCount() << " ports" << std::endl;
}

void MainFrame::OnClose(wxCloseEvent& event)
{
    m_controllerPanel->DisconnectSerial();
    event.Skip();
}

void MainFrame::OnToggleRecording(wxCommandEvent& event)
{
    if (!m_controllerPanel->IsConnected()) {
        std::cout << "ERROR: Not connected to device. Please connect to a COM port first." << std::endl;
        wxMessageBox("Please connect to a COM port first.", "Not Connected", wxOK | wxICON_WARNING);
        return;
    }
    
    if (m_controllerPanel->GetCurrentMode() == MODE_RECORDING) {
        // 記録停止
        m_controllerPanel->StopRecording();
        
        // 自動ファイル名で保存
        wxString filename = GenerateAutoFilename();
        if (m_controllerPanel->SaveRecordingData(filename)) {
            std::cout << "Recording saved: " << filename.ToStdString() << std::endl;
            
            // 保存したファイルを再生用にセット
            m_lastSelectedFile = filename;
            m_selectedFileText->SetLabel("File: " + wxFileName(filename).GetName());
        } else {
            std::cout << "ERROR: Failed to save recording" << std::endl;
        }
        
        m_recordToggleButton->SetLabel("Start Recording");
        m_recordToggleButton->SetBackgroundColour(wxColour(100, 200, 100));
        
        // 接続状態に応じてボタンの有効/無効を更新
        UpdateConnectionState();
    } else {
        // 記録開始
        m_controllerPanel->StartRecording();
        m_recordToggleButton->SetLabel("Stop Recording");
        m_recordToggleButton->SetBackgroundColour(wxColour(200, 100, 100));
        
        std::cout << "Recording started. Waiting for first input to trigger recording..." << std::endl;
        
        // 接続状態を更新（録画中は再生ボタンが無効になる）
        UpdateConnectionState();
    }
    
    UpdateModeDisplay();
}

void MainFrame::OnTogglePlayback(wxCommandEvent& event)
{
    if (!m_controllerPanel->IsConnected()) {
        std::cout << "ERROR: Not connected to device. Please connect to a COM port first." << std::endl;
        wxMessageBox("Please connect to a COM port first.", "Not Connected", wxOK | wxICON_WARNING);
        return;
    }
    
    if (m_controllerPanel->GetCurrentMode() == MODE_PLAYBACK) {
        // 再生停止
        m_controllerPanel->StopPlayback();
        
        // 再生終了時の共通UI処理
        ResetPlaybackUI();
        
        std::cout << "Playback stopped" << std::endl;
    } else {
        // 再生開始
        if (m_lastSelectedFile.IsEmpty()) {
            std::cout << "ERROR: No file selected. Please load a playback file first." << std::endl;
            return;
        }
        
        m_controllerPanel->StartPlayback(m_lastSelectedFile);
        m_playbackToggleButton->SetLabel("Stop Playback");
        m_playbackToggleButton->SetBackgroundColour(wxColour(200, 100, 150));
        
        std::cout << "Playback started: " << m_lastSelectedFile.ToStdString() << std::endl;
        std::cout << "Waiting for first input to trigger playback..." << std::endl;
        
        // 接続状態を更新（再生中は録画ボタンが無効になる）
        UpdateConnectionState();
        
        // 再生開始時のみモード表示を更新
        UpdateModeDisplay();
    }
}

void MainFrame::OnLoadPlayback(wxCommandEvent& event)
{
    wxFileDialog dialog(this, "Load playback file", "", "", "Recording files (*.rec)|*.rec",
                       wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    
    if (dialog.ShowModal() == wxID_OK) {
        m_lastSelectedFile = dialog.GetPath();
        m_selectedFileText->SetLabel("File: " + dialog.GetFilename());
        
        // 接続状態に応じて再生ボタンの有効/無効を更新
        UpdateConnectionState();
        
        std::cout << "Playbook file selected: " << dialog.GetPath().ToStdString() << std::endl;
    }
}

wxString MainFrame::GenerateAutoFilename()
{
    // Recordingsフォルダのパスを生成
    wxString recordingsDir = "Recordings";
    
    // フォルダが存在しない場合は作成
    if (!wxDir::Exists(recordingsDir)) {
        wxMkdir(recordingsDir);
    }
    
    wxDateTime now = wxDateTime::Now();
    wxString filename = wxString::Format("%04d_%02d_%02d_%02d_%02d_%02d.rec",
        now.GetYear(),
        now.GetMonth() + 1,  // wxDateTime uses 0-based months
        now.GetDay(),
        now.GetHour(),
        now.GetMinute(),
        now.GetSecond()
    );
    
    // Recordingsフォルダ内のフルパスを返す
    return wxFileName(recordingsDir, filename).GetFullPath();
}

void MainFrame::UpdateModeDisplay()
{
    OperationMode mode = m_controllerPanel->GetCurrentMode();
    wxString modeText = "Mode: ";
    
    switch (mode) {
        case MODE_PASSTHROUGH:
            modeText += "Passthrough";
            m_modeStatusText->SetForegroundColour(*wxBLACK);
            break;
        case MODE_RECORDING:
            if (m_controllerPanel->IsWaitingForRecordingTrigger()) {
                modeText += "Recording (Waiting for input...)";
            } else {
                modeText += "Recording";
            }
            m_modeStatusText->SetForegroundColour(*wxRED);
            break;
        case MODE_PLAYBACK:
            if (m_controllerPanel->IsWaitingForPlaybackTrigger()) {
                modeText += "Playback (Waiting for trigger...)";
            } else if (m_controllerPanel->IsPlayingBack()) {
                modeText += "Playback (Active)";
            } else {
                modeText += "Playback";
            }
            m_modeStatusText->SetForegroundColour(*wxBLUE);
            break;
    }
    
    m_modeStatusText->SetLabel(modeText);
}

void MainFrame::OnUpdateTimer(wxTimerEvent& event)
{
    UpdateModeDisplay();
}

void MainFrame::OnPlaybackCompleted(wxCommandEvent& event)
{
    std::cout << "DEBUG: Playback completed event received - updating UI" << std::endl;
    
    // 再生終了時の共通UI処理
    ResetPlaybackUI();
    
    std::cout << "DEBUG: UI updated after playback completion" << std::endl;
}

void MainFrame::ResetPlaybackUI()
{
    // 再生ボタンを初期状態に戻す
    m_playbackToggleButton->SetLabel("Start Playback");
    m_playbackToggleButton->SetBackgroundColour(wxColour(100, 150, 200));
    
    // 接続状態に応じてボタンの有効/無効を更新
    UpdateConnectionState();
    
    // モード表示を更新
    UpdateModeDisplay();
}

void MainFrame::UpdateConnectionState()
{
    bool isConnected = m_controllerPanel->IsConnected();
    OperationMode currentMode = m_controllerPanel->GetCurrentMode();
    
    std::cout << "DEBUG: UpdateConnectionState called" << std::endl;
    std::cout << "DEBUG: IsConnected() returns: " << (isConnected ? "true" : "false") << std::endl;
    std::cout << "DEBUG: CurrentMode: " << (int)currentMode << std::endl;
    std::cout << "DEBUG: m_lastSelectedFile: " << (m_lastSelectedFile.IsEmpty() ? "empty" : m_lastSelectedFile.ToStdString()) << std::endl;
    
    // 録画ボタンの状態：接続されていて、再生中でなければ有効
    bool recordEnabled = isConnected && (currentMode != MODE_PLAYBACK);
    m_recordToggleButton->Enable(recordEnabled);
    std::cout << "DEBUG: Record button enabled: " << (recordEnabled ? "true" : "false") << std::endl;
    
    // 再生ボタンの状態：接続されていて、ファイルが選択されていて、録画中でなければ有効
    bool playbackEnabled = isConnected && !m_lastSelectedFile.IsEmpty() && (currentMode != MODE_RECORDING);
    m_playbackToggleButton->Enable(playbackEnabled);
    std::cout << "DEBUG: Playback button enabled: " << (playbackEnabled ? "true" : "false") << std::endl;
    
    std::cout << "DEBUG: Connection state updated - Connected: " << (isConnected ? "Yes" : "No") 
              << ", Record enabled: " << (recordEnabled ? "Yes" : "No")
              << ", Playback enabled: " << (playbackEnabled ? "Yes" : "No") << std::endl;
}