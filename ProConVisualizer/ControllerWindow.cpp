#include "ControllerWindow.h"
#include <wx/graphics.h>
#include <wx/dcbuffer.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

wxBEGIN_EVENT_TABLE(ControllerWindow, wxFrame)
    EVT_CLOSE(ControllerWindow::OnClose)
    EVT_PAINT(ControllerWindow::OnPaint)
    EVT_TIMER(wxID_ANY, ControllerWindow::OnTimer)
    EVT_SIZE(ControllerWindow::OnSize)
wxEND_EVENT_TABLE()

ControllerWindow::ControllerWindow(wxWindow* parent, ControllerPanel* sourcePanel)
    : wxFrame(parent, wxID_ANY, "Pro Controller Display", wxDefaultPosition, wxSize(320, 240)),
      m_sourcePanel(sourcePanel), m_timer(this)
{
    SetBackgroundColour(*wxBLACK);
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);
    m_timer.Start(16); // 約60FPS
    
    // ウィンドウを常に最前面に表示（録画用）
    SetWindowStyleFlag(GetWindowStyleFlag() | wxSTAY_ON_TOP);
    
    // ダブルバッファリングを有効にしてチカチラを防ぐ
    SetDoubleBuffered(true);
}

ControllerWindow::~ControllerWindow()
{
}

void ControllerWindow::OnClose(wxCloseEvent& event)
{
    m_timer.Stop();
    Hide(); // ウィンドウを隠すだけで削除はしない
}

void ControllerWindow::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    
    if (!gc) return;
    
    wxSize size = GetClientSize();
    
    // 背景をクリア（メインパネルと同じ方式）
    gc->SetBrush(wxBrush(wxColour(40, 40, 40)));
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->DrawRectangle(0, 0, size.x, size.y);
    
    // ソースパネルからデータを取得して描画
    if (m_sourcePanel && m_sourcePanel->IsDataValid()) {
        // データをコピーしてミューテックスのロック時間を最小化
        ControllerData data;
        {
            std::lock_guard<std::mutex> lock(m_sourcePanel->GetDataMutex());
            data = m_sourcePanel->GetControllerData();
        }
        
        // コントローラーデータを使って描画
        DrawController(gc, data);
    } else {
        // データがない場合の表示（静的な描画のみ）
        static bool lastWasInvalid = false;
        if (!lastWasInvalid) {
            gc->SetBrush(wxBrush(wxColour(255, 255, 255)));
            wxGraphicsFont statusFont = gc->CreateFont(wxFont(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD), wxColour(255, 255, 255));
            gc->SetFont(statusFont);
            
            wxString status = "Waiting for data...";
            double textWidth, textHeight;
            gc->GetTextExtent(status, &textWidth, &textHeight);
            gc->DrawText(status, size.x/2 - textWidth/2, size.y/2 - textHeight/2);
            lastWasInvalid = true;
        }
    }
    
    delete gc;
}

void ControllerWindow::OnTimer(wxTimerEvent& event)
{
    // データが更新された場合のみ再描画
    if (m_sourcePanel && m_sourcePanel->IsDataValid()) {
        Refresh(false); // false = 背景を消去しない
    }
}

void ControllerWindow::OnSize(wxSizeEvent& event)
{
    Refresh();
    event.Skip();
}

void ControllerWindow::DrawController(wxGraphicsContext* gc, const ControllerData& controllerData)
{
    // ペン定義（Visualizerと同じ）
    wxPen whitePen(*wxWHITE, 3, wxPENSTYLE_SOLID);
    wxPen yellowPen(*wxYELLOW, 3, wxPENSTYLE_SOLID);
    wxPen whitePenS(*wxWHITE, 2, wxPENSTYLE_SOLID);
    
    double deadzone = 0.05;
    
    // Lスティック（八角形 + 円形）
    gc->SetPen(whitePen);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    // 八角形(外枠)
    int center_x = 50;
    int center_y = 115;
    int radius = 35;
    
    wxPoint2DDouble points[8];
    for (int i = 0; i < 8; ++i) {
        double angle = i * (2 * M_PI / 8);
        points[i].m_x = center_x + radius * cos(angle);
        points[i].m_y = center_y + radius * sin(angle);
    }
    // 八角形を描画（パスを使用）
    wxGraphicsPath octPath = gc->CreatePath();
    octPath.MoveToPoint(points[0]);
    for (int i = 1; i < 8; ++i) {
        octPath.AddLineToPoint(points[i]);
    }
    octPath.CloseSubpath();
    gc->DrawPath(octPath);
    
    // Lスティック
    radius = 20;
    gc->SetBrush(*wxBLACK);
    gc->SetPen(wxPen(controllerData.buttons & BTN_L_STICK ? *wxRED : *wxWHITE, 3));
    
    double stick_x = (double)(controllerData.stick_lx - 128) / 128.0;
    double stick_y = (double)(controllerData.stick_ly - 128) / 128.0;
    
    // デッドゾーン
    if (hypot(stick_x, stick_y) < deadzone) {
        stick_x = 0;
        stick_y = 0;
    }
    
    stick_x = center_x + stick_x * radius;
    stick_y = center_y - stick_y * radius;
    
    gc->DrawEllipse(stick_x - radius, stick_y - radius, radius * 2, radius * 2);
    
    // Rスティック（八角形 + 円形）
    gc->SetPen(yellowPen);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    // 八角形(外枠)
    center_x = 190;
    center_y = 170;
    radius = 32; // 少し小さめ
    
    for (int i = 0; i < 8; ++i) {
        double angle = i * (2 * M_PI / 8);
        points[i].m_x = center_x + radius * cos(angle);
        points[i].m_y = center_y + radius * sin(angle);
    }
    // 八角形を描画（パスを使用）
    octPath = gc->CreatePath();
    octPath.MoveToPoint(points[0]);
    for (int i = 1; i < 8; ++i) {
        octPath.AddLineToPoint(points[i]);
    }
    octPath.CloseSubpath();
    gc->DrawPath(octPath);
    
    // Rスティック
    radius = 18;
    gc->SetBrush(*wxBLACK);
    gc->SetPen(wxPen(controllerData.buttons & BTN_R_STICK ? *wxRED : *wxYELLOW, 3));
    
    stick_x = (double)(controllerData.stick_rx - 128) / 128.0;
    stick_y = (double)(controllerData.stick_ry - 128) / 128.0;
    
    // デッドゾーン
    if (hypot(stick_x, stick_y) < deadzone) {
        stick_x = 0;
        stick_y = 0;
    }
    
    stick_x = center_x + stick_x * radius;
    stick_y = center_y - stick_y * radius;
    
    gc->DrawEllipse(stick_x - radius, stick_y - radius, radius * 2, radius * 2);
    
    // 十字キー
    DrawDPad(gc, 110, 170, 10, controllerData);
    
    // PLUS, MINUS
    gc->SetPen(whitePenS);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    radius = 10;
    center_x = 150;
    center_y = 90;
    int offset = 36;
    
    gc->SetBrush(controllerData.buttons & BTN_PLUS ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x + offset - radius, center_y - radius, radius * 2, radius * 2);
    
    gc->SetBrush(controllerData.buttons & BTN_MINUS ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x - offset - radius, center_y - radius, radius * 2, radius * 2);
    
    // A, B, X, Yボタン
    gc->SetPen(whitePen);
    
    radius = 12;
    center_x = 240;
    center_y = 120;
    offset = 25;
    
    gc->SetBrush(controllerData.buttons & BTN_A ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x + offset - radius, center_y - radius, radius * 2, radius * 2);
    
    gc->SetBrush(controllerData.buttons & BTN_B ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x - radius, center_y + offset - radius, radius * 2, radius * 2);
    
    gc->SetBrush(controllerData.buttons & BTN_X ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x - radius, center_y - offset - radius, radius * 2, radius * 2);
    
    gc->SetBrush(controllerData.buttons & BTN_Y ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawEllipse(center_x - offset - radius, center_y - radius, radius * 2, radius * 2);
    
    // L, Rボタン
    gc->SetPen(whitePen);
    center_x = 150;
    center_y = 60;
    offset = 20;
    int w = 60;
    int h = 16;
    
    gc->SetBrush(controllerData.buttons & BTN_L ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(center_x - offset - w, center_y - h / 2, w, h, 6);
    
    gc->SetBrush(controllerData.buttons & BTN_R ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(center_x + offset, center_y - h / 2, w, h, 6);
    
    // ZL, ZRボタン
    gc->SetPen(whitePen);
    center_x = 150;
    center_y = 20;
    offset = 20;
    w = 80;
    h = 30;
    
    gc->SetBrush(controllerData.buttons & BTN_ZL ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(center_x - offset - w, center_y - h / 2 + 10, w, h, 6);
    gc->SetBrush(controllerData.buttons & BTN_ZR ? *wxWHITE : *wxTRANSPARENT_BRUSH);
    gc->DrawRoundedRectangle(center_x + offset, center_y - h / 2 + 10, w, h, 6);
}

// CreateHomeBase関数とDrawDPad関数のコピー
std::vector<wxPoint2DDouble> CreateHomeBaseForWindow(int cx, int cy, int size, double angle) {
    std::vector<wxPoint2DDouble> points(5);

    double base[5][2];
    if (angle == 0 || angle == M_PI) {
        base[0][0] =  0, base[0][1] = 0; // 先端
        base[1][0] =  1, base[1][1] = 1; // 右上
        base[2][0] =  1, base[2][1] = 3; // 右下
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
        base[0][0] =  0, base[0][1] =  0; // 先端
        base[1][0] = -1, base[1][1] =  1; // 右上
        base[2][0] = -3, base[2][1] =  1; // 右下
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
        double ix = base[i][0] * size;
        double iy = base[i][1] * size;
        points[i].m_x = cx + ix;
        points[i].m_y = cy + iy;
    }
    return points;
}

void ControllerWindow::DrawDPad(wxGraphicsContext* gc, double cx, double cy, double size, const ControllerData& controllerData)
{
    // 十字キー
    gc->SetPen(wxPen(*wxWHITE, 2));
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    int center_x = (int)cx;
    int center_y = (int)cy;
    int isize = (int)size;
    
    // 十字キーの座標計算
    short core = 2 * isize;
    short arm  = 2 * isize;

    std::vector<wxPoint2DDouble> pts;
    // 上
    pts.push_back(wxPoint2DDouble(center_x - core / 2, center_y - core / 2));
    pts.push_back(wxPoint2DDouble(center_x - core / 2, center_y - core / 2 - arm));
    pts.push_back(wxPoint2DDouble(center_x + core / 2, center_y - core / 2 - arm));
    
    // 右
    pts.push_back(wxPoint2DDouble(center_x + core / 2, center_y - core / 2));
    pts.push_back(wxPoint2DDouble(center_x + core / 2 + arm, center_y - core / 2));
    pts.push_back(wxPoint2DDouble(center_x + core / 2 + arm, center_y + core / 2));
    
    // 下
    pts.push_back(wxPoint2DDouble(center_x + core / 2, center_y + core / 2));
    pts.push_back(wxPoint2DDouble(center_x + core / 2, center_y + core / 2 + arm));
    pts.push_back(wxPoint2DDouble(center_x - core / 2, center_y + core / 2 + arm));
    
    // 左
    pts.push_back(wxPoint2DDouble(center_x - core / 2, center_y + core / 2));
    pts.push_back(wxPoint2DDouble(center_x - core / 2 - arm, center_y + core / 2));
    pts.push_back(wxPoint2DDouble(center_x - core / 2 - arm, center_y - core / 2));

    // 基本の十字形状を描画
    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(pts[0]);
    for (size_t i = 1; i < pts.size(); ++i) {
        path.AddLineToPoint(pts[i]);
    }
    path.CloseSubpath();
    gc->DrawPath(path);

    // 押下時の表示
    // 下
    auto homeBase = CreateHomeBaseForWindow(center_x, center_y, isize, 0);
    if (controllerData.dpad & DPAD_DOWN) {
        gc->SetBrush(*wxWHITE);
        wxGraphicsPath homePath = gc->CreatePath();
        homePath.MoveToPoint(homeBase[0]);
        for (size_t i = 1; i < homeBase.size(); ++i) {
            homePath.AddLineToPoint(homeBase[i]);
        }
        homePath.CloseSubpath();
        gc->DrawPath(homePath);
    }
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    // 左
    homeBase = CreateHomeBaseForWindow(center_x, center_y, isize, M_PI / 2);
    if (controllerData.dpad & DPAD_LEFT) {
        gc->SetBrush(*wxWHITE);
        wxGraphicsPath homePath = gc->CreatePath();
        homePath.MoveToPoint(homeBase[0]);
        for (size_t i = 1; i < homeBase.size(); ++i) {
            homePath.AddLineToPoint(homeBase[i]);
        }
        homePath.CloseSubpath();
        gc->DrawPath(homePath);
    }
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    // 上
    homeBase = CreateHomeBaseForWindow(center_x, center_y, isize, M_PI);
    if (controllerData.dpad & DPAD_UP) {
        gc->SetBrush(*wxWHITE);
        wxGraphicsPath homePath = gc->CreatePath();
        homePath.MoveToPoint(homeBase[0]);
        for (size_t i = 1; i < homeBase.size(); ++i) {
            homePath.AddLineToPoint(homeBase[i]);
        }
        homePath.CloseSubpath();
        gc->DrawPath(homePath);
    }
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    
    // 右
    homeBase = CreateHomeBaseForWindow(center_x, center_y, isize, 3 * M_PI / 2);
    if (controllerData.dpad & DPAD_RIGHT) {
        gc->SetBrush(*wxWHITE);
        wxGraphicsPath homePath = gc->CreatePath();
        homePath.MoveToPoint(homeBase[0]);
        for (size_t i = 1; i < homeBase.size(); ++i) {
            homePath.AddLineToPoint(homeBase[i]);
        }
        homePath.CloseSubpath();
        gc->DrawPath(homePath);
    }
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
}