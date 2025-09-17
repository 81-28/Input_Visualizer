#include "MainFrame.h"
#include "SerialUtils.h"

MainFrame::MainFrame(const wxString& title) : wxFrame(NULL, wxID_ANY, title) {
	m_drawPanel = new DrawPanel(this);
	
	wxPanel* topPanel = new wxPanel(this);
	wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);

	m_comChoice = new wxChoice(topPanel, wxID_ANY);
	m_connectButton = new wxButton(topPanel, wxID_ANY, "Connect");

	auto ports = SerialUtils::AvailablePorts();
	for (const auto & p : ports) {
		m_comChoice->Append(wxString::Format("%s (%s)", p.port, p.description));
	}

	topSizer->Add(m_comChoice, 1, wxEXPAND | wxRIGHT);
	topSizer->Add(m_connectButton, 0, wxEXPAND);

	topPanel->SetSizer(topSizer);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
	mainSizer->Add(topPanel, 0, wxEXPAND | wxALL);
	mainSizer->Add(m_drawPanel, 1, wxEXPAND);
	this->SetSizer(mainSizer);

	m_connectButton->Bind(wxEVT_BUTTON, &MainFrame::OnConnect, this);
}

void MainFrame::OnConnect(wxCommandEvent& event) {
	if (m_serial && m_serial->IsOpen()) {
		delete m_serial;
		m_serial = nullptr;
		m_connectButton->SetLabel("Connect");
	}
	else {
		int sel = m_comChoice->GetSelection();
		if (sel == wxNOT_FOUND) {
			wxMessageBox("Please select a COM port.", "Error", wxOK | wxICON_ERROR);
			return;
		}

		wxString portStr = m_comChoice->GetString(sel);
		portStr = portStr.BeforeFirst('('); // Extract port name before '('

		if (TryOpenPort(std::string(portStr.mb_str()))) {
			m_connectButton->SetLabel("Disconnect");
		}
		else {
			wxMessageBox("Wrong port or device not connected", "Error", wxOK | wxICON_ERROR);
		}
	}
}

bool MainFrame::TryOpenPort(const std::string& portName) {
	try {
		m_serial = new SerialAnalizer(portName);
		
		if (!m_serial->ReadOnce(200)) {
			delete m_serial;
			m_serial = nullptr;
			return false;
		}
		return true;
	}
	catch (const std::exception& e) {
		m_serial = nullptr;
		return false;
	}	
}
