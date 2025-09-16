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

std::vector<wxPoint> CreateHomeBase(int cx, int cy, int size, double angle) {
    std::vector<wxPoint> points(5);

    double base[5][2];
	if (angle == 0 || angle == M_PI) {
        base[0][0] = 0, base[0][1] = 0; // 先端
		base[1][0] = 1, base[1][1] = 1; // 右上
		base[2][0] = 1, base[2][1] = 3; // 右下
		base[3][0] = -1, base[3][1] = 3; // 左下
		base[4][0] = -1, base[4][1] = 1; // 左上

        if (angle == M_PI) {
			for (int i = 0; i < 5; ++i) {
				base[i][0] = -base[i][0];
				base[i][1] = -base[i][1];
			}
        }
    }

    if (angle == M_PI / 2 || angle == 3 * M_PI / 2) {
        base[0][0] = 0, base[0][1] = 0; // 先端
        base[1][0] = -1, base[1][1] = 1; // 右上
        base[2][0] = -3, base[2][1] = 1; // 右下
        base[3][0] = -3, base[3][1] = -1; // 左下
        base[4][0] = -1, base[4][1] = -1; // 左上

        if (angle == 3 * M_PI / 2) {
            for (int i = 0; i < 5; ++i) {
                base[i][0] = -base[i][0];
                base[i][1] = -base[i][1];
            }
        }
    }

    for (int i = 0; i < 5; ++i) {
		int ix = base[i][0] * size;
		int iy = base[i][1] * size;
        points[i] = wxPoint(cx + ix, cy + iy);
    }
    return points;
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
	wxPen whitePenS(*wxWHITE,  2, wxPENSTYLE_SOLID);

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
    center_y = 115;
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
	gdc.SetPen(gamepad.L3 ? redPen : whitePen);
  
    stick_x = center_x + (double)gamepad.LX / 2048 * radius;
    stick_y = center_y - (double)gamepad.LY / 2048 * radius;
	gdc.DrawCircle(stick_x, stick_y, radius);


    // Rスティック
	gdc.SetPen(yellowPen);
	gdc.SetBrush(*wxTRANSPARENT_BRUSH);

	// 八角形(外枠)
	center_x = 190;
	center_y = 170;
	radius = 32; // 少し小さめ

	for (int i = 0; i < 8; ++i) {
		double angle = i * (2 * M_PI / 8);
		points[i] = wxPoint(center_x + radius * cos(angle), center_y + radius * sin(angle));
	}
	gdc.DrawPolygon(8, points);

	// スティック
	radius = 18;
	gdc.SetBrush(*wxBLACK);
	gdc.SetPen(gamepad.R3 ? redPen : yellowPen);

	stick_x = center_x + (double)gamepad.RX / 2048 * radius;
	stick_y = center_y - (double)gamepad.RY / 2048 * radius;

	gdc.DrawCircle(stick_x, stick_y, radius);


    // 十字キー
	gdc.SetPen(whitePenS);
	gdc.SetBrush(*wxTRANSPARENT_BRUSH);
	center_x = 110;
	center_y = 170;
	//offset = 0;
    short size = 10;

    {
        // 十字キーの座標計算
		short cx = center_x;
		short cy = center_y;
        short core = 2 * size;
		short arm = 2 * size;

        std::vector<wxPoint> pts;
        // 上
        pts.push_back(wxPoint(cx - core / 2, cy - core / 2));
        pts.push_back(wxPoint(cx - core / 2, cy - core / 2 - arm));
		pts.push_back(wxPoint(cx + core / 2, cy - core / 2 - arm));

		// 右
		pts.push_back(wxPoint(cx + core / 2, cy - core / 2));
		pts.push_back(wxPoint(cx + core / 2 + arm, cy - core / 2));
		pts.push_back(wxPoint(cx + core / 2 + arm, cy + core / 2));

		// 下
		pts.push_back(wxPoint(cx + core / 2, cy + core / 2));
		pts.push_back(wxPoint(cx + core / 2, cy + core / 2 + arm));
		pts.push_back(wxPoint(cx - core / 2, cy + core / 2 + arm));

		// 左
		pts.push_back(wxPoint(cx - core / 2, cy + core / 2));
		pts.push_back(wxPoint(cx - core / 2 - arm, cy + core / 2));
		pts.push_back(wxPoint(cx - core / 2 - arm, cy - core / 2));

		gdc.DrawPolygon(pts.size(), pts.data());
    }

	// 押下時
    {
		// 下
        auto pts = CreateHomeBase(center_x, center_y, size, 0);
		if (gamepad.DPAD_DOWN) {
			gdc.SetBrush(*wxWHITE);
			gdc.DrawPolygon(pts.size(), pts.data());
		}
		gdc.SetBrush(*wxTRANSPARENT_BRUSH);
    }
	{
		// 左
		auto pts = CreateHomeBase(center_x, center_y, size, M_PI / 2);
		if (gamepad.DPAD_LEFT) {
			gdc.SetBrush(*wxWHITE);
			gdc.DrawPolygon(pts.size(), pts.data());
		}
		gdc.SetBrush(*wxTRANSPARENT_BRUSH);
	}
	{	// 上
		auto pts = CreateHomeBase(center_x, center_y, size, M_PI);
		if (gamepad.DPAD_UP) {
			gdc.SetBrush(*wxWHITE);
			gdc.DrawPolygon(pts.size(), pts.data());
		}
		gdc.SetBrush(*wxTRANSPARENT_BRUSH);
	}
	{	// 右
		auto pts = CreateHomeBase(center_x, center_y, size, 3 * M_PI / 2);
		if (gamepad.DPAD_RIGHT) {
			gdc.SetBrush(*wxWHITE);
			gdc.DrawPolygon(pts.size(), pts.data());
		}
		gdc.SetBrush(*wxTRANSPARENT_BRUSH);
	}


    // PLUS, MINUS
	gdc.SetPen(whitePenS);
	gdc.SetBrush(*wxTRANSPARENT_BRUSH);
    radius = 10;
    center_x = 150;
    center_y = 90;
    offset = 36;

	gdc.SetBrush(gamepad.PLUS ? *wxWHITE : *wxTRANSPARENT_BRUSH);
	gdc.DrawCircle(center_x + offset, center_y, radius);

	gdc.SetBrush(gamepad.MINUS ? *wxWHITE : *wxTRANSPARENT_BRUSH);
	gdc.DrawCircle(center_x - offset, center_y, radius);

	// Capture, Home
    // 見た目が悪いので実装しない


	// A, B, X, Yボタン
    gdc.SetPen(whitePen);

    radius = 12;
    center_x = 240;
    center_y = 120;
    offset = 25;

	gdc.SetBrush(gamepad.A ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gdc.DrawCircle(center_x + offset, center_y, radius);

	gdc.SetBrush(gamepad.B ? *wxWHITE : *wxTRANSPARENT_BRUSH);
	gdc.DrawCircle(center_x, center_y + offset, radius);

	gdc.SetBrush(gamepad.X ? *wxWHITE : *wxTRANSPARENT_BRUSH);
	gdc.DrawCircle(center_x, center_y - offset, radius);

	gdc.SetBrush(gamepad.Y ? *wxWHITE : *wxTRANSPARENT_BRUSH);
	gdc.DrawCircle(center_x - offset, center_y, radius);


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
