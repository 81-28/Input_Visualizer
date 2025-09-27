#pragma once

#include <wx/wx.h>
#include <wx/timer.h>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include "ControllerPanel.h"

class ControllerWindow : public wxFrame
{
public:
    ControllerWindow(wxWindow* parent, ControllerPanel* sourcePanel);
    ~ControllerWindow();

private:
    void OnClose(wxCloseEvent& event);
    void OnPaint(wxPaintEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnSize(wxSizeEvent& event);
    
    void DrawController(wxGraphicsContext* gc, const ControllerData& controllerData);
    void DrawDPad(wxGraphicsContext* gc, double cx, double cy, double size, const ControllerData& controllerData);
    
    ControllerPanel* m_sourcePanel;
    wxTimer m_timer;    wxDECLARE_EVENT_TABLE();
};