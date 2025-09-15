#pragma once

#include <wx/wx.h>
#include "DrawPanel.h"
#include "SerialAnalizer.h"

class MainFrame : public wxFrame
{
public:
	MainFrame(const wxString& title);
	SerialAnalizer* m_serial;


private:
	DrawPanel* m_drawPanel;
};

