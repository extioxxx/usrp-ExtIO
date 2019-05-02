#pragma once

#include <uhd/utils/thread.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/types/device_addr.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <iostream>

#include <string>

using namespace std;
using namespace uhd;



#define SAFE_DELETE(p)				{ if (p) { delete (p); (p) = NULL; } }
#define SAFE_DELETE_ARRAY(p)		{ if (p) { delete [] (p); (p) = NULL; } }
#define IS_EMPTY(str)				(((LPCTSTR)(str) == NULL) || ((*(LPCTSTR)(str)) == _T('\0')))
#define CAN_CAST(ptr,cls)			I2B(dynamic_cast<cls*>(ptr))
#define DYNAMIC_CAST(cls,ptr,var)	cls* var = dynamic_cast<cls*>(ptr)
#define SAMPLE_PER_BUFF 2016
#define SUPERBUFFERCNT 256

usrp::multi_usrp::sptr getUSRP();
int CreateUhd(const char *devargs);
bool StartUhd();
void logger(const char *fn);
double SetSampleRate(double rate);
double GetSampleRate();
double GetFreq();
long SetLO(double dFreq);
double setGain(double gain);
int setAnt(string ant);
