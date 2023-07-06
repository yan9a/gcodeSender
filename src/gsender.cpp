//File: gsender.cpp
//Description: gcode sender 
//Copyright (c) 2021 Yan Naing Aye

#include "wx/wx.h"
#include <wx/spinctrl.h>
#include <string>
#include <vector>
#include <cstdint>
#include <wx/numdlg.h>
#include "ce/ceUtil.h"
using namespace std;
using namespace ce;
#define N_LOG_LIST_LINES 30

#define ID_BTNSTART 100
#define ID_GTIMER 102
#define ID_MY_WXSERIAL 103
#define ID_SELPORT_MNU 104
#define ID_OPEN_MNU 105
#define ID_CLOSE_MNU 106
#define ID_CLEAR_MNU 107
#define ID_LOG_LST 108
#define ID_TXTRX 109
#define ID_BTN_SEQUENCE 110
#define SEQUENCE_DIR "./"

// ----------------------------------------------------------------------------
class MyFrame;
class MyApp : public wxApp
{
public:
	virtual bool OnInit();
	MyFrame* _fra;
	ceWxSerial* _com;
	void OnSerialEvent(wxThreadEvent& event);
};

// Define a new frame type: this is going to be our main frame
class MyFrame : public wxFrame
{
public:
	MyFrame(MyApp* app, const wxString& title);
	// event handlers (these functions should _not_ be virtual)
	void OnQuit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	void OnOpen(wxCommandEvent& event);
	void OnClose(wxCommandEvent& event);
	void SelPort(wxCommandEvent& event);
	void OnClear(wxCommandEvent& event);
	void OnSend(wxCommandEvent& event);
	void OnTimer(wxTimerEvent& event);
	void ProcessChar(char ch);
	void ClearText(wxCommandEvent& event);
	void OnSequence(wxCommandEvent& WXUNUSED(event));
	void Write(std::string mes);
    // ctor(s)
private:
	MyApp* _app;
	wxButton *btnStart;
	wxTimer _timer;
	wxTextCtrl *txtRx;
	wxButton* btnSequence;
	wxFileDialog* _dlgSeqFile;
	Json::Value _seq;
};

IMPLEMENT_APP(MyApp)

// 'Main program' equivalent: the program execution "starts" here
bool MyApp::OnInit()
{
    // call the base class initialization method, currently it only parses a
    // few common command-line options but it could be do more in the future
    if ( !wxApp::OnInit() )
        return false;

    // create the main application window
    _fra = new MyFrame(this, wxT("Gcode sender"));

	string device;
#if defined(CE_WINDOWS)
	device = "\\\\.\\COM5";
#else
	device = "/dev/ttyUSB0";
#endif
	_com = new ceWxSerial(this, ID_MY_WXSERIAL, 10, device, 115200, 8, 'N', 1);
	Connect(ID_MY_WXSERIAL, wxEVT_THREAD, wxThreadEventHandler(MyApp::OnSerialEvent));

    _fra->Show(true);

    return true;
}

void MyApp::OnSerialEvent(wxThreadEvent& event)
{
	std::vector<char> v = event.GetPayload<std::vector<char>>();
	char ch;
	for (int i = 0; i < v.size(); i++) {
		ch = v[i];
		_fra->ProcessChar(ch);
	}
	// wxMessageBox(wxT("Serial event"));
}

// ----------------------------------------------------------------------------
// main frame
// ----------------------------------------------------------------------------

// frame constructor
MyFrame::MyFrame(MyApp* app, const wxString& title)
       : wxFrame(NULL, wxID_ANY, title,wxDefaultPosition,wxSize(800, 600), wxDEFAULT_FRAME_STYLE ^ wxRESIZE_BORDER),_timer(this, ID_GTIMER), _app(app)
{
    // set the frame icon
    SetIcon(wxICON(sample));

#if wxUSE_MENUS
    // create a menu bar
    wxMenu *fileMenu = new wxMenu;
    wxMenu *helpMenu = new wxMenu;

    helpMenu->Append(wxID_ABOUT, wxT("&About\tF1"), wxT("Show about dialog"));
    fileMenu->Append(ID_OPEN_MNU, wxT("&Open\tAlt-O"), wxT("Open serial port"));
	fileMenu->Append(ID_CLOSE_MNU, wxT("&Close\tAlt-C"), wxT("Close serial port"));
	fileMenu->Append(ID_CLEAR_MNU, wxT("Clea&r\tAlt-R"), wxT("Clear text"));
	fileMenu->Append(ID_SELPORT_MNU, wxT("&Serial Port\tAlt-S"), wxT("Select serial port"));
	fileMenu->Append(wxID_EXIT, wxT("E&xit\tAlt-X"), wxT("Quit this program"));

    // now append the freshly created menu to the menu bar...
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, wxT("&File"));
    menuBar->Append(helpMenu, wxT("&Help"));

    // ... and attach this menu bar to the frame
    SetMenuBar(menuBar);
#endif // wxUSE_MENUS

#if wxUSE_STATUSBAR
    // create a status bar just for fun (by default with 1 pane only)
    CreateStatusBar(2);
    SetStatusText(wxT("GCode Sender"));
#endif // wxUSE_STATUSBAR
	btnStart = new wxButton(this, ID_BTNSTART,wxT("Start"), wxPoint(5, 5), wxSize(100, 25));
	txtRx = new wxTextCtrl(this, ID_TXTRX, wxT(""), wxPoint(5, 35), wxSize(765, 475), wxTE_MULTILINE);

	Connect(ID_BTNSTART, wxEVT_COMMAND_BUTTON_CLICKED,wxCommandEventHandler(MyFrame::OnSend));
	Connect(wxID_ABOUT,wxEVT_COMMAND_MENU_SELECTED,wxCommandEventHandler(MyFrame::OnAbout));
	Connect(wxID_EXIT,wxEVT_COMMAND_MENU_SELECTED,wxCommandEventHandler(MyFrame::OnQuit));
	Connect(ID_OPEN_MNU,wxEVT_COMMAND_MENU_SELECTED,wxCommandEventHandler(MyFrame::OnOpen));
	Connect(ID_CLOSE_MNU,wxEVT_COMMAND_MENU_SELECTED,wxCommandEventHandler(MyFrame::OnClose));
	Connect(ID_SELPORT_MNU, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::SelPort));
	Connect(ID_GTIMER,wxEVT_TIMER, wxTimerEventHandler(MyFrame::OnTimer));
	Connect(ID_CLEAR_MNU, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::ClearText));

	// For sequence of movements
	this->btnSequence = new wxButton(this, ID_BTN_SEQUENCE, wxT("Load"), wxPoint(170, 5), wxSize(100, 25));
	Connect(ID_BTN_SEQUENCE, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::OnSequence));

	_dlgSeqFile = new wxFileDialog(this, wxT("Open GCode sequence file"), (SEQUENCE_DIR), "sequence.json", "JSON files (*.json)|*.json");
	this->Centre(wxBOTH);

	_timer.Start(250);
}


// event handlers
void MyFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
    // true is to force the frame to close
    Close(true);
}

void MyFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    wxMessageBox(wxString::Format(
                                  wxT("GCode Sender \n ")
                                  wxT("Author: Yan Naing Aye \n ")
                                  wxT("Web: http://cool-emerald.blogspot.com/2021/04/drawbot-xy-plotter-with-arduino-grbl.html \n")
								  wxT("Repo: https://github.com/yan9a/gcodeSender")
                                  ),
        wxT("About GCode Sender"),
        wxOK | wxICON_INFORMATION,
        this);
}

void MyFrame::OnOpen(wxCommandEvent& WXUNUSED(event))
{
	if(_app->_com->Open()) txtRx->AppendText(wxString::Format(wxT("Error opening port %s.\n"),_app->_com->GetPort()));
	else txtRx->AppendText(wxString::Format(wxT("Port %s is opened.\n"), _app->_com->GetPort()));
}

void MyFrame::OnClose(wxCommandEvent& WXUNUSED(event))
{
	_app->_com->Close();
	txtRx->AppendText(wxString::Format(wxT("Port %s is closed.\n"), _app->_com->GetPort()));
}

void MyFrame::SelPort(wxCommandEvent& WXUNUSED(event))
{
	if (_app->_com->IsOpened()) {
		txtRx->AppendText(wxString::Format(wxT("Close Port %s first.\n"), _app->_com->GetPort()));
	}
	else {
        wxString cdev=wxString::Format(wxT("%s"), _app->_com->GetPort());
		wxString device = wxGetTextFromUser(wxT("Enter the port"), wxT("Set Port"), cdev);
		string str = device.ToStdString();
		if (str.length() > 0) {
			_app->_com->SetPortName(str);
		}

        txtRx->AppendText(wxString::Format(wxT("Port: %s\n"), _app->_com->GetPort()));
	}
}

void MyFrame::OnSend(wxCommandEvent& WXUNUSED(event))
{
	wxString str = "Hello";
	wxCharBuffer buffer = str.ToUTF8();
	if (_app->_com->Write(buffer.data())) {
		txtRx->AppendText(str);
	}
	else {
		txtRx->AppendText(wxT("Write error.\n"));
	}
}

void MyFrame::OnTimer(wxTimerEvent& WXUNUSED(event))
{

}

void MyFrame::ProcessChar(char ch)
{
	this->txtRx->AppendText(wxString::Format(wxT("%c"), ch));
}

void MyFrame::Write(std::string mes)
{
	txtRx->AppendText(wxString::Format(wxT("%s"),mes + "\n"));
}

void MyFrame::ClearText(wxCommandEvent& WXUNUSED(event))
{
	txtRx->Clear();
}

void MyFrame::OnSequence(wxCommandEvent& WXUNUSED(event))
{
	//if (...current content has not been saved...)
	//{
	//	if (wxMessageBox(_("Current content has not been saved! Proceed?"), _("Please confirm"),
	//		wxICON_QUESTION | wxYES_NO, this) == wxNO)
	//		return;
	//	//else: proceed asking to the user the new file to open
	//}

	if (_dlgSeqFile->ShowModal() == wxID_CANCEL)
		return;     // the user changed idea...

	wxString filePath = _dlgSeqFile->GetPath();
	this->Write("Opening " + filePath.ToStdString());
	try {
		this->_seq = ceConfig::ReadJson(filePath.ToStdString());
		for (auto const& jv : this->_seq) {
			// std::cout<<jv<<endl;
			// this->Print(ceConfig::ToString(jv));
			//uint8_t delay = uint8_t(jv["delay"].asUInt());
			//string cmd = jv["cmd"].asString();
			//this->Send(cmd);
		}
	}
	catch (...) {
		this->Write("Error in json values");
	}
}