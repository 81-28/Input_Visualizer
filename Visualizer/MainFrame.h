#pragma once

#include <wx/wx.h>
#include "DrawPanel.h"
#include "SerialAnalizer.h"

class MainFrame : public wxFrame
{
public:
	MainFrame(const wxString& title);
	SerialAnalizer* m_serial = nullptr;


private:
	void OnConnect(wxCommandEvent& event);

	DrawPanel* m_drawPanel;
	wxChoice* m_comChoice;
	wxButton* m_connectButton;
};

