/*
 * ExtIO wrapper for librtlsdr
 * Copyleft by Jos?Araújo [josemariaaraujo@gmail.com]
 * Don't care about licenses (DWTFYW), but per GNU I think I'm required to leave the following here:
 *
 * Based on original work from Ian Gilmour (MM6DOS) and Youssef Touil from SDRSharp
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* ******************************************************************************
 * Simple ExtIO wrapper for librtlsdr.  
 * 
 * These guys done a great job on the lib. All credits to them
 *
 * Original librtlsdr git:
 *
 * git://git.osmocom.org/rtl-sdr
 */

#include <windowsx.h>
#include "stdafx.h"
#include "extio_uhd.h"
#include "afxwin.h"
#include "resource.h"
#include <string>
#include <signal.h>
#include "ExtIO_xxx.h"
#include "TehMin.h"

#include "extio_uhd.h"

void(*WinradCallBack)(int, int, float, void *) = NULL;
#define WINRAD_SRCHANGE 100
#define WINRAD_LOCHANGE 101
#define WINRAD_ATTCHANGE 125

static INT_PTR CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
HWND h_dialog = NULL;
HMODULE hInst;

HANDLE worker_handle = INVALID_HANDLE_VALUE;
HANDLE worker_handle2 = INVALID_HANDLE_VALUE;
void ThreadProc(void * param);
void ThreadProcBuffer(void * param);
int Start_Thread();
int Stop_Thread();

struct superBuffer {
	bool isfilled = { 0 };
	int16_t num_rx_samps = { 0 };
	int16_t buffer[SAMPLE_PER_BUFF * sizeof(short) *3 + 1] = { 0 };
}buffers[SUPERBUFFERCNT];
extern usrp::multi_usrp::sptr m_dev;
extern bool	m_bRunning;
extern rx_streamer::sptr rx_stream;
extern gain_range_t  m_gainRange;
extern double m_dGain;
extern string m_otw;
string m_new_otw = "sc8";

string localaddr = " ";

static void printMSG2(string message) {
	HWND listloghWnd = GetDlgItem(h_dialog, IDC_LIST_LOG);
	int dwPos = SendMessage(listloghWnd, LB_ADDSTRING, 0, (LPARAM)message.c_str());
	SendMessage(listloghWnd, WM_VSCROLL, SB_BOTTOM, 0);
}


extern "C"
bool  LIBRTL_API __stdcall InitHW(char *name, char *model, int& type){
	//MessageBox(NULL, TEXT("initHW"), NULL, MB_OK);
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	type = 3;	// H/W type
	_tcscpy_s(name, 16, "USRP");	// /FCD/RTL2832/BorIP

	if (h_dialog) {
		ShowWindow(h_dialog, SW_SHOW);
		//SetForegroundWindow(h_dialog);
	}
	return TRUE;
}

extern "C"
int LIBRTL_API __stdcall GetStatus()
{
	/* dummy function */		
    return 0;
}

extern "C"
bool  LIBRTL_API __stdcall OpenHW()
{
	//MessageBox(NULL, TEXT("OpenHW"),NULL, MB_OK);
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	try {
		//hModule
		hInst = AfxGetInstanceHandle();
		//h_dialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOGBAR), NULL, (DLGPROC)MainDlgProc);
		h_dialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DIALOGBAR), NULL, (DLGPROC)MainDlgProc);
	}
	catch (boost::system::system_error &e) {
		MessageBox(NULL, e.what(), NULL, MB_OK);
	}
	catch (...) {}

	if (h_dialog)
		ShowWindow(h_dialog, SW_SHOW);

	logger(".\\uhd.log");
	CreateUhd(localaddr.c_str());

	HWND sampleratehWnd = GetDlgItem(h_dialog, IDC_COMBO_SAMPLE_RATE);
	SendMessage(sampleratehWnd, CB_RESETCONTENT, 0, 0);
	double dd = 60e6; //default 12M
	string rates[7] = {"60","32","16","8","4","2","1"};
	for (int i = 0;i<6;i++){
		SendMessage(sampleratehWnd, CB_ADDSTRING, 0, (LPARAM)rates[i].c_str());
	}
	SendMessage(sampleratehWnd, CB_SETCURSEL, 2, 0);//16M

	
	HWND sourcehWnd = GetDlgItem(h_dialog, IDC_COMBO_SOURCE);
	SendMessage(sourcehWnd, CB_RESETCONTENT, 0, 0);
	string sr[2] = { "TX/RX","RX2" };
	for (int i = 0; i < 2; i++) {
		SendMessage(sourcehWnd, CB_ADDSTRING, 0, (LPARAM)sr[i].c_str());
	}
	SendMessage(sourcehWnd, CB_SETCURSEL, 1, 0);

	HWND otwhWnd = GetDlgItem(h_dialog, IDC_COMBO_OTW);
	SendMessage(otwhWnd, CB_RESETCONTENT, 0, 0);
	string sr2[3] = { "sc8","sc12","sc16" };
	for (int i = 0; i < 3; i++) {
		SendMessage(otwhWnd, CB_ADDSTRING, 0, (LPARAM)sr2[i].c_str());
	}
	SendMessage(otwhWnd, CB_SETCURSEL, 0, 0);

	WinradCallBack(-1, WINRAD_ATTCHANGE, 0, NULL);

	return TRUE;
}

extern "C"
void LIBRTL_API __stdcall CloseHW()
{
	//MessageBox(NULL, TEXT("CloseHW"),NULL, MB_OK);

	if (worker_handle2 != INVALID_HANDLE_VALUE) {
		// Wait 1s for thread to die
		WaitForSingleObject(worker_handle2, 2000);
		CloseHandle(worker_handle2);
		worker_handle2 = INVALID_HANDLE_VALUE;
		printMSG2("ExtIO data thread stopped!");
	}
	if (worker_handle != INVALID_HANDLE_VALUE) {
		WaitForSingleObject(worker_handle, 2000);
		CloseHandle(worker_handle);
		worker_handle = INVALID_HANDLE_VALUE;
		printMSG2("Recv thread stopped!");
	}

	if (h_dialog)
		DestroyWindow(h_dialog);

}


extern "C"
int LIBRTL_API __stdcall SetHWLO(long freq)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	//MessageBox(NULL, TEXT("SetHWLO"), NULL, MB_OK);
	if (getUSRP() == NULL) {
		return 1;	// Higher than possible
	}

	double lOfreq = double(freq);
	if (lOfreq < 1e3) {
		lOfreq = 1e3;
	}

	long iResult = SetLO(lOfreq);

	WinradCallBack(-1, WINRAD_LOCHANGE, 0, NULL);
	return iResult;
}


extern "C"
int LIBRTL_API __stdcall StartHW(long freq)
{
	//MessageBox(NULL, TEXT("StartHW"),NULL, MB_OK);
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	if (getUSRP() == NULL)
		return 1;	


	int nSampleRate = StartUhd();
	if (nSampleRate == 0)
		return 1;

	SetHWLO(freq);
	Start_Thread();

	return SAMPLE_PER_BUFF;
}

extern "C"
void LIBRTL_API __stdcall StopHW()
{
	//MessageBox(NULL, TEXT("StopHW"),NULL, MB_OK);
	if (m_bRunning) {
		EnableWindow(GetDlgItem(h_dialog, IDD_DIALOGBAR), TRUE);

		Stop_Thread();

		for (int i = 0; i < SUPERBUFFERCNT; i++) {
			buffers[i].isfilled = false;
			buffers[i].num_rx_samps = 0;
		}
		for (int i = 0; i < SUPERBUFFERCNT; i++)
			memset( buffers[i].buffer, 0, SAMPLE_PER_BUFF * sizeof(short)*3 );
	}
}


extern "C"
long LIBRTL_API __stdcall GetHWLO()
{
	//MessageBox(NULL, TEXT("GetHWLO"),NULL, MB_OK);
	if (getUSRP() == NULL) {
		return 0;
	}

	int64_t lOfreq = (int64_t)GetFreq();

	return (long)(lOfreq & 0xFFFFFFFF);
}


extern "C"
long LIBRTL_API __stdcall GetHWSR()
{
	//MessageBox(NULL, TEXT("GetHWSR"),NULL, MB_OK);
	if (!m_dev) {
		return 48000;	// Sensible default
	}

	string m_strLastError =_T("Getting HW sample rate...\n");	// Only happens once automatically at beginning

	//return /*250000*/(long)m_pUSRP->m_desired_sample_rate;	// 64M / decim (min = 250k)
	double dRate = GetSampleRate();
	long sr = (long)round(dRate);

	return sr;
}


extern "C"
void LIBRTL_API __stdcall ShowGUI()
{
	ShowWindow(h_dialog,SW_SHOW);
	SetForegroundWindow(h_dialog);
	return;
}

extern "C"
void LIBRTL_API  __stdcall HideGUI()
{
	ShowWindow(h_dialog,SW_HIDE);
	return;
}

extern "C"
void LIBRTL_API  __stdcall SwitchGUI()
{
	if (IsWindowVisible(h_dialog))
		ShowWindow(h_dialog,SW_HIDE);
	else
		ShowWindow(h_dialog,SW_SHOW);
	return;
}


extern "C"
void LIBRTL_API __stdcall SetCallback(void (* myCallBack)(int, int, float, void *))
{

	WinradCallBack = myCallBack;

    return;
}
int Start_Thread()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	m_bRunning = true;

	//If already running, exit
	if (worker_handle != INVALID_HANDLE_VALUE) {
		printMSG2("recv thread running ......");
	}else{
		worker_handle = (HANDLE)_beginthread(ThreadProc, 0, NULL);
		if (worker_handle == INVALID_HANDLE_VALUE)
			return -1;
		SetThreadPriority(worker_handle, THREAD_PRIORITY_TIME_CRITICAL);
		printMSG2("Recv thread OK!");

	}
	if (worker_handle2 != INVALID_HANDLE_VALUE) {
		printMSG2("ExtIO data thread running ......");
	}else{
		worker_handle2 = (HANDLE)_beginthread(ThreadProcBuffer, 0, NULL);
		if (worker_handle2 == INVALID_HANDLE_VALUE)
			return -1;
		//SetThreadPriority(worker_handle2, THREAD_PRIORITY_TIME_CRITICAL);
		printMSG2("ExtIO data thread OK!");
	}

	return 0;
}

int Stop_Thread()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	m_bRunning = false;
	printMSG2("Stop!");
	for (int i = 0; i < SUPERBUFFERCNT; i++) {
		buffers[i].isfilled = false;
		buffers[i].num_rx_samps = 0;
		memset(buffers[i].buffer, 0, SAMPLE_PER_BUFF * sizeof(short) * 3);
	}
	for (int i = 0; i < SUPERBUFFERCNT; i++)

	return 0;
}

CRITICAL_SECTION cs;
void ThreadProcBuffer(void *p)
{
	Sleep(1000);

	while (1) {
		if (!m_bRunning) {
			Sleep(50);
			continue;
		}

		for (int i = 0; i < SUPERBUFFERCNT; i++) {
			if (buffers[i].isfilled && buffers[i].num_rx_samps > 0) {
				if (WinradCallBack) WinradCallBack(buffers[i].num_rx_samps, 0, 0, (short*)(buffers[i].buffer) );

				//EnterCriticalSection(&cs);
				buffers[i].isfilled = false;
				buffers[i].num_rx_samps = 0;
				//LeaveCriticalSection(&cs);

			}
		}
	}
}

extern UINT m_nOverflows;
extern UINT m_nShortReads;
extern double m_dSampleRate;
void ThreadProc(void *p)
{
	//signal(SIGINT, sig_int_handle);
	unsigned long long num_total_samps = 0;
	uhd::rx_metadata_t md;
	UINT spp = SAMPLE_PER_BUFF;

	//std::string cpu_format = "fc32";
	//std::string wire_format = "sc16";
	//uhd::stream_args_t stream_args(cpu_format, wire_format);
	//stream_args.args["spp"] = Teh::Utils::ToString(spp); //samples per packet
	//rx_stream = m_dev->get_rx_stream(stream_args);

	uhd::stream_args_t rx_stream_args("sc16","sc8");//sc8, sc12 http://files.ettus.com/manual/structuhd_1_1stream__args__t.html
	rx_stream_args.args["spp"] = Teh::Utils::ToString(spp); //samples per packet
	rx_stream = m_dev->get_device()->get_rx_stream(rx_stream_args);


	//uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
	uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
	//stream_cmd.num_samps = SAMPLE_PER_BUFF;
	stream_cmd.stream_now = true;
	//stream_cmd.time_spec = uhd::time_spec_t();
	stream_cmd.time_spec = m_dev->get_time_now();
	rx_stream->issue_stream_cmd(stream_cmd);

	int i = 0;
	while (1) {
		if (!m_bRunning) {
			Sleep(50);
			continue;
		}

		if (m_otw != m_new_otw) {
			uhd::stream_args_t rx_stream_args("sc16", m_new_otw.c_str());
			rx_stream_args.args["spp"] = Teh::Utils::ToString(spp); //samples per packet
			rx_stream = m_dev->get_device()->get_rx_stream(rx_stream_args);
			uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
			stream_cmd.stream_now = true;
			stream_cmd.time_spec = m_dev->get_time_now();
			rx_stream->issue_stream_cmd(stream_cmd);
			printMSG2( string("set OTW_format from ") + m_otw + " to " + m_new_otw);

			m_otw = m_new_otw;
		}

		//EnterCriticalSection(&cs);
		buffers[i].isfilled = false;
		buffers[i].num_rx_samps = 0;
		//LeaveCriticalSection(&cs);
		memset(buffers[i].buffer, 0, SAMPLE_PER_BUFF * sizeof(short) * 3);

		int num_rx_samps = rx_stream->recv(buffers[i].buffer, SAMPLE_PER_BUFF, md, 3, true);
		//int num_rx_samps = rx_stream->recv(buffer, SAMPLE_PER_BUFF, md);
		if (num_rx_samps != SAMPLE_PER_BUFF) {
			++m_nShortReads;
		}
		if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
			printMSG2("Timeout while streaming......");
			continue;
		}
		if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW) {
			++m_nOverflows;
			string str = ("Overflow: [") + Teh::Utils::ToString(m_nOverflows)
				+ "] at rate: [" + Teh::Utils::ToString(m_dSampleRate) + "]";
			if (m_nOverflows % 10 == 0)
				printMSG2(str);
			continue;
		}
		if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
			printMSG2(_T("Receive error: ") + md.strerror());
			continue;
		}
		num_total_samps += num_rx_samps;

		//printf("num_rx_samps = %d \n",num_rx_samps);
		//EnterCriticalSection(&cs);
		buffers[i].isfilled = true;
		buffers[i].num_rx_samps = num_rx_samps;
		//LeaveCriticalSection(&cs);
		//if (WinradCallBack) WinradCallBack(num_rx_samps, 0, 0, (short*)buffers[i].buffer);

		if (++i >= SUPERBUFFERCNT) {
			i = 0;
		}
	}
	_endthread();
}

int newgainpos = 0;
static INT_PTR CALLBACK MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	double rate = 2;
	double freq = 106.6e6;
	char str[100] = "4\0";
	HWND hGain;

	switch (uMsg){
		case WM_INITDIALOG:
		{
			//ComboBox_SetCurSel(GetDlgItem(hwndDlg, IDC_DEVICE), device_default);
			//ComboBox_AddString(GetDlgItem(hwndDlg, IDC_SAMPLERATE), samplerates[i].name);

			return TRUE;
		}
		case WM_COMMAND:
			switch (GET_WM_COMMAND_ID(wParam, lParam))
			{
			case IDC_BUTTON1:
				WinradCallBack(-1, WINRAD_SRCHANGE, 0, NULL);// Signal application
				return TRUE;
			case IDC_LIST_LOG:
				//int iIndex = m_cntrlList_Log.AddString(str);
				//printMSG(str);
				//WinradCallBack(-1, WINRAD_SRCHANGE, 0, NULL);// Signal application
				return TRUE;
			case IDC_COMBO_SAMPLE_RATE:
				return TRUE;
			case IDC_BUTTON_SET_SAMPLE_RATE:
				SendMessage(GetDlgItem(h_dialog, IDC_COMBO_SAMPLE_RATE), WM_GETTEXT, 20, (LPARAM)str);
				rate = atof(str);
				SetSampleRate(rate * 1e6);
				freq = GetFreq();
				SetLO(freq);
				WinradCallBack(-1, WINRAD_SRCHANGE, 0, NULL);// Signal application
				return TRUE;
			case IDC_BUTTON_SET_GAIN:
				if (m_dev) {
					double gain = (double)newgainpos * m_gainRange.stop()/100;
					//printMSG2(string(_T("newgainpos: ") + Teh::Utils::ToString(gain)) );
					if (abs(gain - m_dGain) > 0.01) {
						m_dGain = setGain(gain);
						WinradCallBack(-1, WINRAD_SRCHANGE, 0, NULL);
					}
				}
				return TRUE;
			case IDC_COMBO_SOURCE:
				return TRUE;
			case IDC_BUTTON_SOURCE:
				SendMessage(GetDlgItem(h_dialog, IDC_COMBO_SOURCE), WM_GETTEXT, 20, (LPARAM)str);
				if (m_dev) {
					setAnt(str);
					WinradCallBack(-1, WINRAD_SRCHANGE, 0, NULL);
				}
				return TRUE;
			case IDC_BUTTON_SET_OTW:
				SendMessage(GetDlgItem(h_dialog, IDC_COMBO_OTW), WM_GETTEXT, 20, (LPARAM)str);
				if (m_dev) {
					m_new_otw = string(str);
					//WinradCallBack(-1, WINRAD_SRCHANGE, 0, NULL);
				}
				return TRUE;
			}

		case WM_VSCROLL:
			hGain = GetDlgItem(h_dialog, IDC_GAIN);
			if ((HWND)lParam == hGain){
				//pos: [0,  100]
				int pos = 100-SendMessage(hGain, TBM_GETPOS, (WPARAM)0, (LPARAM)0);
				if(m_dev)
					_stprintf_s(str, 255, TEXT("%.0lf"), (double) pos * m_gainRange.stop()/100 );
				else
					_stprintf_s(str, 255, TEXT("%d %c\0"), pos, '%');
				Static_SetText(GetDlgItem(h_dialog, IDC_GAINVALUE), str);
				newgainpos = pos;

				return TRUE;
			}
			break;
		case WM_CLOSE:
			ShowWindow(h_dialog, SW_HIDE);
			return TRUE;
			break;
		case WM_DESTROY:
			h_dialog = NULL;
			return TRUE;
			break;
		case WM_CTLCOLORSTATIC:
			break;
	}

	return FALSE;
}

