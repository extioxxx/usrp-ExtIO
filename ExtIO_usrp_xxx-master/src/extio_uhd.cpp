#include "stdafx.h"
#include "extio_uhd.h"
#include "TehMin.h"
#include "resource.h"
#include <windowsx.h>

#include <afxmt.h>
#include <uhd/utils/log.hpp>
#include <uhd/utils/log_add.hpp>


string antdefault("RX2");//TX/RX RX2
string clockref("internal");

double ratedefault(16e6);
double freqdefault(106.6e6);
double gaindefault(30);

usrp::multi_usrp::sptr m_dev;
rx_streamer::sptr rx_stream;

string m_strLastError;
gain_range_t  m_gainRange;

double m_dSampleRate;
double m_dFreq;
double m_dGain;
string m_strAntenna;
string m_otw = "sc8";

double 	m_fpga_master_clock_freq;
bool	m_bRunning;
UINT	m_nOverflows;
UINT	m_nShortReads;

extern HWND h_dialog;

usrp::multi_usrp::sptr getUSRP() {
	return m_dev;
}

static void printMSG(string message) {
	HWND listloghWnd = GetDlgItem(h_dialog, IDC_LIST_LOG);
	int dwPos = SendMessage(listloghWnd, LB_ADDSTRING, 0, (LPARAM)message.c_str());
	SendMessage(listloghWnd, WM_VSCROLL, SB_BOTTOM, 0);
}

static void myUHDLogger(const uhd::log::logging_info &info)
{
	//build a log message formatted from the information
	std::string message;

	if (not info.file.empty())
	{
		std::string shortfile = info.file.substr(info.file.find_last_of("/\\") + 1);
		message += "[" + shortfile + ":" + std::to_string(info.line) + "] ";
	}

	if (not info.component.empty())
	{
		message += "[" + info.component + "] ";
	}

	message += info.message;
	//MessageBox(NULL, message.c_str(), NULL, MB_OK);
	printMSG(message);

	switch (info.verbosity)
	{
	case uhd::log::trace:   break;
	case uhd::log::debug:   break;
	case uhd::log::info:    break;
	case uhd::log::warning: break;
	case uhd::log::error:   break;
	case uhd::log::fatal:   break;
	default: break;
	}
}


void logger(const char *fn) {
/*
trace 	displays every available log message
debug 	displays most log messages necessary for debugging internals
info 	informational messages about setup and what is going on
warning 	something is not right but operation can continue
error 	something has gone wrong
fatal 	something has gone horribly wrong
off 	logging is turned off
 trace = 0, debug = 1, info = 2, warning = 3,  error = 4, fatal = 5, off = 6
*/
	uhd::log::set_log_level(uhd::log::severity_level::info);

	if (fn && strnlen(fn, 254) >0) {
		uhd::log::add_logger("extioUHDDevice", &myUHDLogger);
	}
}

int CreateUhd(const char *devargs) {

	if (m_dev){
		m_strLastError = _T("Device already exists");
		return 0;
	}

	try {
		uhd::set_thread_priority_safe();

		device_addr_t addr((LPCSTR)devargs);
		m_dev = usrp::multi_usrp::make(addr);
		if (!m_dev )
			return -1;
		m_dev->set_clock_source(clockref);
		m_strLastError = "Setting clock source:  " + clockref;
		printMSG(m_strLastError);

		string strSubDevice = "A:A";

		//set the board on 10.2 to use the A RX frontend (RX channel 0)
		//dev->set_rx_subdev_spec("A:A", 0);
		//set the board on 10.3 to use the B RX frontend (RX channel 1)
		//dev->set_rx_subdev_spec("A:B", 1);
		//m_dev->set_rx_subdev_spec(usrp::subdev_spec_t(strSubDevice));
		m_dev->set_rx_subdev_spec( (usrp::subdev_spec_t(strSubDevice)), 0);
	}catch (const std::runtime_error& e){
		m_strLastError = _T("While setting UHD RX sub-device: ") + CString(e.what());
		printMSG(m_strLastError);
		return -1;
	}catch (...){
		//AfxTrace(_T("Failed to set sub-device specification: %s\n"), strHint);
		m_strLastError = _T("Unknown while setting UHD RX sub-device");
		printMSG(m_strLastError);
		return -2;
	}
	
	size_t nMBs = m_dev->get_num_mboards();

	m_fpga_master_clock_freq = m_dev->get_master_clock_rate();
	m_strLastError = _T("master clock frequency: ") + Teh::Utils::ToString(m_fpga_master_clock_freq);
	printMSG(m_strLastError);
	if (m_fpga_master_clock_freq <= 0){
		m_strLastError = _T("Invalid master clock frequency: ") + Teh::Utils::ToString(m_fpga_master_clock_freq);
		printMSG(m_strLastError);
		return -6;
	}

	m_gainRange = m_dev->get_rx_gain_range();
	m_strLastError = _T( "gain range: ") + 
		Teh::Utils::ToString(m_gainRange.start()) + "0 to " + Teh::Utils::ToString(m_gainRange.stop()) ;
	printMSG(m_strLastError);

	m_dSampleRate = SetSampleRate(ratedefault);
	//m_dSampleRate = m_dev->get_rx_rate();

	SetLO(freqdefault);

	setGain(gaindefault);

	setAnt(antdefault);

	return 0;
}

int setAnt(string ant) {
	try {
		m_strAntenna = "Getting antenna: " + m_dev->get_rx_antenna();
		printMSG(m_strAntenna);

		m_dev->set_rx_antenna(ant);
		m_strAntenna = "Setting antenna: " + m_dev->get_rx_antenna();
		printMSG(m_strAntenna);
	}catch (const std::runtime_error& e) {
		m_strLastError = _T("Exception while getting antenna: ") + CString(e.what()) + _T("\n");
		printMSG(m_strLastError);
	}catch (...) {
		m_strLastError = _T("Exception while getting antenna\n");
		printMSG(m_strLastError);
	}

	return 0;
}


double setGain(double gain) {
	m_dGain = m_dev->get_rx_gain();
	m_strLastError = _T("Getting gain: ") + Teh::Utils::ToString(m_dGain);
	printMSG(m_strLastError);

	m_dev->set_rx_gain(gain);
	m_dGain = m_dev->get_rx_gain();
	m_strLastError = _T("Setting gain: ") + Teh::Utils::ToString(m_dGain);
	printMSG(m_strLastError);

	int nPos = 100- 100 * (m_dGain / m_gainRange.stop()) ;
	SendMessage(GetDlgItem(h_dialog, IDC_GAIN), TBM_SETPOS, (WPARAM)1, (LPARAM)nPos );
	Static_SetText(GetDlgItem(h_dialog, IDC_GAINVALUE), Teh::Utils::ToString(m_dGain) );

	return (m_dGain);
}

void ResetUhdStats()
{
	m_nOverflows = 0;
	m_nShortReads = 0;
}

double SetSampleRate(double rate) {
	m_dSampleRate = m_dev->get_rx_rate();
	m_strLastError = _T("Getting sample rate: ") + Teh::Utils::ToString(m_dSampleRate);
	printMSG(m_strLastError);

	m_dev->set_rx_rate(rate);
	m_dSampleRate = m_dev->get_rx_rate();
	m_strLastError = _T("Setting sample rate: ") + Teh::Utils::ToString(m_dSampleRate);
	printMSG(m_strLastError);
	return m_dSampleRate;

}

double GetSampleRate() {
	m_dSampleRate = m_dev->get_rx_rate();
	return m_dSampleRate;
}

double GetFreq() {
	m_dFreq = m_dev->get_rx_freq();
	return m_dFreq;
}

long SetLO(double dFreq) {
	//m_dev->set_rx_freq(dFreq);
	uhd::tune_result_t m_tuneResult;

	try{
		m_dFreq = m_dev->get_rx_freq();
		m_tuneResult = m_dev->set_rx_freq(dFreq);
		m_strLastError = _T("Setting frequency from: [") 
			+ Teh::Utils::ToString(m_dFreq)
			+ _T("] to [") 
			+ Teh::Utils::ToString(m_tuneResult.actual_rf_freq)
			+ "]";
		printMSG(m_strLastError);
	}catch (const std::runtime_error& e){
		m_strLastError = _T("While setting frequency: ") + CString(e.what());
		printMSG(m_strLastError);
		return -1;
	}catch (...){
		m_strLastError = _T("Unknown while setting frequency");
		printMSG(m_strLastError);
		return -1;
	}
	m_dFreq = m_tuneResult.actual_rf_freq;
	//printMSG( string("actual_rf_freq = " + Teh::Utils::ToString(m_tuneResult.actual_rf_freq)) );
	//printMSG( string("actual_dsp_freq = " + Teh::Utils::ToString(m_tuneResult.actual_dsp_freq)) );
	// Take 'abs' for when using Basic/LF RX: LO is 0 and CORDIC is doing (negative) shift
	return abs(m_tuneResult./*actual_inter_freq*/actual_rf_freq + m_tuneResult.actual_dsp_freq);
	//return m_tuneResult.actual_rf_freq;
}

bool StartUhd()
{
	m_strLastError.clear();

	if (!m_dev){
		m_strLastError = _T("No device");
		printMSG(m_strLastError);
		return false;
	}

	try{
		uhd::stream_cmd_t cmd = uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS;
		cmd.stream_now = true;
		m_dev->issue_stream_cmd(cmd);
	}catch (const std::runtime_error& e){
		m_strLastError = _T("While starting stream: ") + CString(e.what());
		printMSG(m_strLastError);
		return false;
	}catch (...){
		m_strLastError = _T("Unknown while starting stream");
		printMSG(m_strLastError);
		return false;
	}

	ResetUhdStats();

	return true;
}


