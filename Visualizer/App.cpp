#include "App.h"
#include "MainFrame.h"

//#define DEBUG

wxIMPLEMENT_APP(App);

bool App::OnInit() {

	// コンソール画面を強制的に出す
#ifdef DEBUG
	if (AllocConsole()) {
		FILE* fp;
		freopen_s(&fp, "CONOUT$", "w", stdout);
		freopen_s(&fp, "CONOUT$", "w", stderr);
		std::cout << "Debug console started" << std::endl;
	}
#endif

	MainFrame* mainFrame = new MainFrame("GamePad");
	mainFrame->SetSize(310, 280);
	mainFrame->Centre();
	mainFrame->Show(true);
	return true;
}