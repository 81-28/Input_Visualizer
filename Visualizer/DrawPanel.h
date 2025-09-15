#pragma once
#include <wx/wx.h>
#include <wx/dcgraph.h>

class DrawPanel : public wxPanel
{
public:
	DrawPanel(wxWindow* parent);

private:
	void ClearBackground(wxGCDC& gdc);
	void OnPaint(wxPaintEvent& event);
	void OnTimer(wxTimerEvent& event);

	wxTimer m_timer;
	int m_index = 0;

	wxDECLARE_EVENT_TABLE();
};

