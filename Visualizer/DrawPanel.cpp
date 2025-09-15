#include "DrawPanel.h"
#include "SerialAnalizer.h"
#include <wx/dcbuffer.h>
#include <wx/dcgraph.h>

wxBEGIN_EVENT_TABLE(DrawPanel, wxPanel)
EVT_PAINT(DrawPanel::OnPaint)
EVT_TIMER(wxID_ANY, DrawPanel::OnTimer)
wxEND_EVENT_TABLE()

DrawPanel::DrawPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY), m_timer(this) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(*wxWHITE);

    m_timer.Start(100); // 100msごとにタイマーイベント
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

    // 青い直線
    wxPen bluePen(*wxBLUE, 2, wxPENSTYLE_SOLID);
    gdc.SetPen(bluePen);
    gdc.DrawLine(50, 50, 200, 100);

    // 円
    gdc.SetBrush(*wxTRANSPARENT_BRUSH); // 塗りつぶしなし，これがないと内部が塗りつぶされる
    gdc.DrawCircle(300, 100, 50); // x, y, radius
    gdc.DrawCircle(300, 100, 30);

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
    double df[] = { -1, -0.7, -0.3, 0, 0.3, 0.7, 1 };
    double dx = df[m_index] * 40;
    double dy = df[m_index] * 8;
    gdc.DrawCircle(500 + dx, 100 + dy, 30);
}

void DrawPanel::OnTimer(wxTimerEvent& event) {
    // インデックス更新
    m_index = (m_index + 1) % 7;
    Refresh(); // 再描画要求
}
