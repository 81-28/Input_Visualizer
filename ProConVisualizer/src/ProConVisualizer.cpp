#include <wx/wx.h>
#include <wx/choice.h>
#include <wx/sizer.h>
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
    void PopulateComPorts();
    
    ControllerPanel* m_controllerPanel;
    wxChoice* m_comChoice;
    wxButton* m_connectButton;
    wxButton* m_refreshButton;
    
    enum {
        ID_CONNECT = 1000,
        ID_REFRESH = 1001
    };
    
    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON(ID_CONNECT, MainFrame::OnConnect)
    EVT_BUTTON(ID_REFRESH, MainFrame::OnRefresh)
    EVT_CLOSE(MainFrame::OnClose)
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
    : wxFrame(nullptr, wxID_ANY, "Pro Controller Visualizer", wxDefaultPosition, wxSize(600, 500))
{
    m_controllerPanel = new ControllerPanel(this);
    
    wxPanel* topPanel = new wxPanel(this);
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);

    m_comChoice = new wxChoice(topPanel, wxID_ANY);
    m_connectButton = new wxButton(topPanel, ID_CONNECT, "Connect");

    PopulateComPorts();

    topSizer->Add(m_comChoice, 1, wxEXPAND | wxRIGHT, 5);
    topSizer->Add(m_connectButton, 0, wxEXPAND);

    topPanel->SetSizer(topSizer);

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(topPanel, 0, wxEXPAND | wxALL, 5);
    mainSizer->Add(m_controllerPanel, 1, wxEXPAND);
    this->SetSizer(mainSizer);

    Centre();
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