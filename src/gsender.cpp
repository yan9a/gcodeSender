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
#include <deque>
#include <utility>

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

#define GS_TIMER_INT 100 // timer interval
// ----------------------------------------------------------------------------
class MyFrame;
class MyApp : public wxApp
{
public:
	virtual bool OnInit();
	MyFrame* _fra;
	ceWxSerial* _com;
	ceLog* _log;
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
	//void OnOpen(wxCommandEvent& event);
	//void OnClose(wxCommandEvent& event);
	//void SelPort(wxCommandEvent& event);
	void OnClear(wxCommandEvent& event);
	void OnSend(wxCommandEvent& event);
	void OnTimer(wxTimerEvent& event);
	void ProcessChar(char ch);
	void ClearText(wxCommandEvent& event);
	void OnSequence(wxCommandEvent& WXUNUSED(event));
	void Write(std::string mes);
	std::vector<std::pair<std::string, uint32_t>> _cmdLoop;
	int ReadSeq(std::string seqtag, std::vector<std::pair<std::string, uint32_t>>& cmdv);
	void ChkSeq();
    // ctor(s)
private:
	MyApp* _app;
	wxButton *btnStart;
	wxTimer _timer;
	wxTextCtrl *txtRx;
	wxButton* btnSequence;
	wxFileDialog* _dlgSeqFile;
	Json::Value _seq;
	int _repeat; // number of times to repeat
	int _currentn; // number of times repeated
	int _currenti; // current index
	int _currentd; // current delay
	int _en_seq; // enable sequence
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
	this->_log = new ceLog(ceMisc::exedir() + "log/", 10);
	this->_log->SetEnPrintf(false);
	this->_log->Print("App logs " + to_string(10) + " days");
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
 //   fileMenu->Append(ID_OPEN_MNU, wxT("&Open\tAlt-O"), wxT("Open serial port"));
	//fileMenu->Append(ID_CLOSE_MNU, wxT("&Close\tAlt-C"), wxT("Close serial port"));
	fileMenu->Append(ID_CLEAR_MNU, wxT("Clea&r\tAlt-R"), wxT("Clear text"));
	//fileMenu->Append(ID_SELPORT_MNU, wxT("&Serial Port\tAlt-S"), wxT("Select serial port"));
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
	//Connect(ID_OPEN_MNU,wxEVT_COMMAND_MENU_SELECTED,wxCommandEventHandler(MyFrame::OnOpen));
	//Connect(ID_CLOSE_MNU,wxEVT_COMMAND_MENU_SELECTED,wxCommandEventHandler(MyFrame::OnClose));
	//Connect(ID_SELPORT_MNU, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::SelPort));
	Connect(ID_GTIMER,wxEVT_TIMER, wxTimerEventHandler(MyFrame::OnTimer));
	Connect(ID_CLEAR_MNU, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::ClearText));

	// For sequence of movements
	this->btnSequence = new wxButton(this, ID_BTN_SEQUENCE, wxT("Load"), wxPoint(170, 5), wxSize(100, 25));
	Connect(ID_BTN_SEQUENCE, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::OnSequence));

	_dlgSeqFile = new wxFileDialog(this, wxT("Open GCode sequence file"), (SEQUENCE_DIR), "sequence.json", "JSON files (*.json)|*.json");
	this->Centre(wxBOTH);

	_timer.Start(GS_TIMER_INT-1);
	_en_seq = 0;
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

//void MyFrame::OnOpen(wxCommandEvent& WXUNUSED(event))
//{
//	if(_app->_com->Open()) this->Write("Error opening port " + _app->_com->GetPort());
//	else this->Write("Port opened: " + _app->_com->GetPort());
//}

//void MyFrame::OnClose(wxCommandEvent& WXUNUSED(event))
//{
//	_app->_com->Close();
//	this->Write("Port closed:" +  _app->_com->GetPort());
//}

//void MyFrame::SelPort(wxCommandEvent& WXUNUSED(event))
//{
//	if (_app->_com->IsOpened()) {
//		this->Write("Close the opening port first: " + _app->_com->GetPort());
//	}
//	else {
//        wxString cdev=wxString::Format(wxT("%s"), _app->_com->GetPort());
//		wxString device = wxGetTextFromUser(wxT("Enter the port"), wxT("Set Port"), cdev);
//		string str = device.ToStdString();
//		if (str.length() > 0) {
//			_app->_com->SetPortName(str);
//		}
//
//        this->Write("Port selected: " + _app->_com->GetPort());
//	}
//}

void MyFrame::OnSend(wxCommandEvent& WXUNUSED(event))
{
	_currentn = 0;
	_currenti = 0;
	_currentd = 0;
	_en_seq = 1;

	this->Write("Started sending gcode...");

	if (_app->_com->Open()) this->Write("Error opening port " + _app->_com->GetPort());
	else this->Write("Port opened: " + _app->_com->GetPort());


}

void MyFrame::OnTimer(wxTimerEvent& WXUNUSED(event))
{
	ChkSeq();
}

void MyFrame::ChkSeq()
{
	static int td = 0; // time to track delay
	int n;
	if (_en_seq)
	{
		if (td >= _currentd) {
			td = 0; // reset time for delay
			n = (int)_cmdLoop.size();
			if (n > 0) {
				if (_currentn < _repeat) {

					if (_currenti < n) {
						auto m = _cmdLoop[_currenti];
						string gcode = m.first;
						int dly = m.second;
						if (_app->_com->Write((char*)gcode.c_str())) {
							this->Write(gcode);
							this->Write("Delay: " + to_string(dly) + " Cmd i: " + to_string(_currenti));
						}
						else {
							this->Write("Error in sending " + gcode);
						} 
						_currentd = dly;
						_currenti++;
					}

					// if end of cmd vector
					if (_currenti >= n) {
						_currenti = 0;
						_currentn++;
						this->Write("Command loop: " + to_string(_currentn));
					}						
				}

				// check n after executing
				if (_currentn >= _repeat) {
					_en_seq = 0;
					this->Write("Completed repeating command loop " + to_string(_repeat) + " times");
					_app->_com->Close();
					this->Write("Port closed:" + _app->_com->GetPort());
				}
			}
			else {
				_en_seq = 0; // no cmd
				this->Write("Disable sequence because of the empty command loop");
				_app->_com->Close();
				this->Write("Port closed:" + _app->_com->GetPort());
			}
		}
		else {
			td += GS_TIMER_INT; // add interval
		}
	}
}

void MyFrame::ProcessChar(char ch)
{
	this->txtRx->AppendText(wxString::Format(wxT("%c"), ch));
}

void MyFrame::Write(std::string mes)
{
	string txt;
	static std::deque<std::string> list;
	list.push_back(mes + "\n");
	if (list.size() > 40) {
		list.pop_front();
	}
	for (auto& m : list) {
		txt += m;
	}
	txtRx->Clear();
	txtRx->AppendText(wxString::Format(wxT("%s"),txt));

	_app->_log->Print(mes);
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
	_dlgSeqFile->SetDirectory(ceMisc::exedir());

	if (_dlgSeqFile->ShowModal() == wxID_CANCEL)
		return;     // the user changed idea...

	wxString filePath = _dlgSeqFile->GetPath();
	this->Write("Opening " + filePath.ToStdString());

	try {
		this->_seq = ceConfig::ReadJson(filePath.ToStdString());
		_repeat = ReadSeq("Loop", _cmdLoop);
	}
	catch (...) {
		this->Write("Error in json values");
	}
}

int MyFrame::ReadSeq(std::string seqtag, std::vector<std::pair<std::string, uint32_t>>& cmdv)
{
	int n = 0;
	std::pair<std::string, int> m;
	string cmd;
	// read loop
	Json::Value cseq = _seq[seqtag]["Sequence"];
	cmdv.clear();
	if (!cseq.isNull()) {
		this->Write("Loaded sequence...");
		if (cseq.isArray()) {
			for (auto obj : cseq) {
				cmd = obj.asString();
				m.first = _seq[cmd]["GCode"].asString();
				m.second = _seq[cmd]["Delay"].asUInt();
				cmdv.push_back(m);
				this->Write(m.first + " Delay: " + to_string(m.second));
			}
		}
		else {
			cmd = cseq.asString();
			m.first = _seq[cmd]["GCode"].asString();
			m.second = _seq[cmd]["Delay"].asUInt();
			cmdv.push_back(m);
			this->Write(m.first + " Delay: " + to_string(m.second));
		}

		/// read repeat count
		n = _seq[seqtag]["Repeat"].asUInt();

		// read port
#if defined(CE_WINDOWS)
		string str = _seq["Port"]["Windows"].asString();
#else
		string str = _seq["Port"]["Linux"].asString();
#endif
		
		if (str.length() > 0) {
			_app->_com->SetPortName(str);
		}
		this->Write("Port selected: " + _app->_com->GetPort());

	}
	else {
		this->Write(seqtag + " sequence json not found...");
	}
	return n;
}