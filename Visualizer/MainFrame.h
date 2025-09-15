#pragma once

#include <wx/wx.h>
#include "DrawPanel.h"
#include "SerialAnalizer.h"

class MainFrame : public wxFrame
{
public:
	MainFrame(const wxString& title);

private:
	DrawPanel* m_drawPanel;
	SerialAnalizer* m_serial;
};

