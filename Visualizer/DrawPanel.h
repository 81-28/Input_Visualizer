#pragma once
#include <wx/wx.h>
#include <wx/dcgraph.h>
#include "SerialAnalizer.h"

class DrawPanel : public wxPanel
{
public:
	DrawPanel(wxWindow* parent);

private:
	void ClearBackground(wxGCDC& gdc);
	void OnPaint(wxPaintEvent& event);
	void OnTimer(wxTimerEvent& event);

	wxTimer m_timer;

	wxDECLARE_EVENT_TABLE();
};

