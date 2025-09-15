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
	wxPen redPen   (*wxRED,    3, wxPENSTYLE_SOLID);
	wxPen bluePen  (*wxBLUE,   3, wxPENSTYLE_SOLID);
	wxPen greenPen (*wxGREEN,  3, wxPENSTYLE_SOLID);
	wxPen yellowPen(*wxYELLOW, 3, wxPENSTYLE_SOLID);
	wxPen blackPen (*wxBLACK,  3, wxPENSTYLE_SOLID);
	wxPen whitePen (*wxWHITE,  3, wxPENSTYLE_SOLID);

    SwitchPro::GamePad gamepad = dynamic_cast<MainFrame*>(GetParent())->m_serial->GetGamePad();

    short radius;
    short center_x;
    short center_y;
    short offset;
    double stick_x, stick_y;

	// Lスティック
    gdc.SetPen(whitePen);
	gdc.SetBrush(*wxTRANSPARENT_BRUSH);

    // 八角形(外枠)
    center_x = 50;
    center_y = 95;
    radius = 35;

    wxPoint points[8];
    for (int i = 0; i < 8; ++i) {
        double angle = i * (2 * M_PI / 8);
        points[i] = wxPoint(center_x + radius * cos(angle), center_y + radius * sin(angle));
    }
    gdc.DrawPolygon(8, points);

    // スティック
    radius = 20;
	gdc.SetBrush(*wxBLACK);
  
    stick_x = center_x + (double)gamepad.LX / 2048 * radius;
    stick_y = center_y - (double)gamepad.LY / 2048 * radius;
	gdc.DrawCircle(stick_x, stick_y, radius);

    // 押し込み
    if (gamepad.L3) {
        gdc.SetPen(redPen);
        gdc.DrawCircle(stick_x, stick_y, radius);
    }


    // Rスティック
	gdc.SetPen(yellowPen);
	gdc.SetBrush(*wxTRANSPARENT_BRUSH);

	// 八角形(外枠)
	center_x = 190;
	center_y = 150;
	radius = 32; // 少し小さめ

	for (int i = 0; i < 8; ++i) {
		double angle = i * (2 * M_PI / 8);
		points[i] = wxPoint(center_x + radius * cos(angle), center_y + radius * sin(angle));
	}
	gdc.DrawPolygon(8, points);

	// スティック
	radius = 18;
	gdc.SetBrush(*wxBLACK);

	stick_x = center_x + (double)gamepad.RX / 2048 * radius;
	stick_y = center_y - (double)gamepad.RY / 2048 * radius;

	gdc.DrawCircle(stick_x, stick_y, radius);

	// 押し込み
	if (gamepad.R3) {
		gdc.SetPen(redPen);
		gdc.DrawCircle(stick_x, stick_y, radius);
	}


	// A, B, X, Yボタン
    gdc.SetPen(whitePen);
    gdc.SetBrush(*wxTRANSPARENT_BRUSH);

    radius = 12;
    center_x = 240;
    center_y = 100;
    offset = 25;
  
    // 押されてない
    gdc.DrawCircle(center_x + offset, center_y, radius); // A
	gdc.DrawCircle(center_x, center_y + offset, radius); // B
	gdc.DrawCircle(center_x, center_y - offset, radius); // X
	gdc.DrawCircle(center_x - offset, center_y, radius); // Y

	// 押されてる
    if (gamepad.A) {
        gdc.SetBrush(*wxWHITE);
        gdc.DrawCircle(center_x + offset, center_y, radius);
    }
    if (gamepad.B) {
        gdc.SetBrush(*wxWHITE);
        gdc.DrawCircle(center_x, center_y + offset, radius);
    }
    if (gamepad.X) {
        gdc.SetBrush(*wxWHITE);
        gdc.DrawCircle(center_x, center_y - offset, radius);
    }
    if (gamepad.Y) {
        gdc.SetBrush(*wxWHITE);
        gdc.DrawCircle(center_x - offset, center_y, radius);
    }



   

    // 正八角形
    wxPen anyColorPen(wxColour(255, 100, 100), 2);
    gdc.SetPen(anyColorPen);



    // 内部を塗りつぶし
    gdc.SetBrush(wxBrush(*wxWHITE));
    gdc.DrawCircle(500, 100, 30);

	//std::cout << (double)gamepad.LX / 2048 << ", " << (double)gamepad.LY / 2048 << " | " << std::endl;
}

void DrawPanel::OnTimer(wxTimerEvent& event) {
    Refresh(); // 再描画要求
}
