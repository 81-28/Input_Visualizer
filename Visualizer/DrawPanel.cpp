#include "DrawPanel.h"
#include <wx/dcbuffer.h>
#include <wx/dcgraph.h>
#include <iostream>
#include "MainFrame.h"

wxBEGIN_EVENT_TABLE(DrawPanel, wxPanel)
EVT_PAINT(DrawPanel::OnPaint)
EVT_TIMER(wxID_ANY, DrawPanel::OnTimer)
wxEND_EVENT_TABLE()


DrawPanel::DrawPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY), m_timer(this) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(*wxBLACK);

    m_timer.Start(8); // 8msごとにタイマーイベント
}

void DrawPanel::ClearBackground(wxGCDC& gdc) {
    gdc.SetBrush(wxBrush(GetBackgroundColour()));
    gdc.SetPen(*wxTRANSPARENT_PEN);
    gdc.DrawRectangle(GetClientRect());
}

void DrawPanel::OnPaint(wxPaintEvent& event) {
    wxAutoBufferedPaintDC dc(this);
    wxGCDC gdc(dc);
    ClearBackground(gdc);

    // ペン定義
	wxPen redPen  (*wxRED,   2, wxPENSTYLE_SOLID);
	wxPen bluePen (*wxBLUE,  2, wxPENSTYLE_SOLID);
	wxPen greenPen(*wxGREEN, 2, wxPENSTYLE_SOLID);
	wxPen blackPen(*wxBLACK, 2, wxPENSTYLE_SOLID);
	wxPen whitePen(*wxWHITE, 2, wxPENSTYLE_SOLID);

    SwitchPro::GamePad gamepad = dynamic_cast<MainFrame*>(GetParent())->m_serial->GetGamePad();


    gdc.SetPen(whitePen);
	gdc.SetBrush(*wxTRANSPARENT_BRUSH);

	uint8_t button_radius = 12;

    // X, B
	gdc.DrawCircle(400, 300, button_radius);
	gdc.DrawCircle(400, 350, button_radius);

	// A, Y
	gdc.DrawCircle(425, 325, button_radius);
	gdc.DrawCircle(375, 325, button_radius);

    if (gamepad.X) {
        gdc.SetBrush(*wxWHITE);
        gdc.DrawCircle(400, 300, button_radius);
    }
	if (gamepad.B) {
		gdc.SetBrush(*wxWHITE);
		gdc.DrawCircle(400, 350, button_radius);
	}
	if (gamepad.A) {
		gdc.SetBrush(*wxWHITE);
		gdc.DrawCircle(425, 325, button_radius);
	}
	if (gamepad.Y) {
		gdc.SetBrush(*wxWHITE);
		gdc.DrawCircle(375, 325, button_radius);
	}

   

    // 正八角形
    wxPen anyColorPen(wxColour(255, 100, 100), 2);
    gdc.SetPen(anyColorPen);

    // 八角形の頂点を計算
    // 中心(500, 100)，半径50
    wxPoint points[8];
    for (int i = 0; i < 8; i++) {
        double angle = i * (2 * M_PI / 8);
        points[i] = wxPoint(500 + 50 * cos(angle), 100 + 50 * sin(angle));
    }
    gdc.DrawPolygon(8, points);

    // 内部を塗りつぶし
    gdc.SetBrush(wxBrush(*wxWHITE));
    gdc.DrawCircle(500, 100, 30);

	//std::cout << (double)gamepad.LX / 2048 << ", " << (double)gamepad.LY / 2048 << " | " << std::endl;
	std::cout << gamepad.RX << ", " << gamepad.RY << " | " << std::endl;
}

void DrawPanel::OnTimer(wxTimerEvent& event) {
    Refresh(); // 再描画要求
}
