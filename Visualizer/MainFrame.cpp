#include "MainFrame.h"

MainFrame::MainFrame(const wxString& title) : wxFrame(NULL, wxID_ANY, title) {
	m_drawPanel = new DrawPanel(this);
	m_serial = new SerialAnalizer("COM9");

	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(m_drawPanel, 1, wxEXPAND);
	this->SetSizer(sizer);
}
