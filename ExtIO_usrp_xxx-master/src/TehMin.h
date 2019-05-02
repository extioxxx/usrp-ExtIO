#pragma once

#define I2B(b)						((b) ? true : false)
#define B2I(b)						((b) ? TRUE : FALSE)
#define CLAMP(low,i,hi)				(max(min((i),(hi)),(low)))
#define ZERO_MEMORY(r)				ZeroMemory(&r, sizeof(r))
#define SAFE_DELETE(p)				{ if (p) { delete (p); (p) = NULL; } }
#define SAFE_DELETE_ARRAY(p)		{ if (p) { delete [] (p); (p) = NULL; } }
#define IS_EMPTY(str)				(((LPCTSTR)(str) == NULL) || ((*(LPCTSTR)(str)) == _T('\0')))
#define CAN_CAST(ptr,cls)			I2B(dynamic_cast<cls*>(ptr))
#define DYNAMIC_CAST(cls,ptr,var)	cls* var = dynamic_cast<cls*>(ptr)

#ifndef CRLF
#define CRLF		_T("\r\n")
#endif // CRLF

#include <vector>
#include <map>

namespace Teh
{

typedef std::vector<CString>	CStringVector;

namespace Utils
{

#ifdef FormatMessage
inline bool GetOSErrorMessage(DWORD dwErrorCode, CString& strError)
{
	LPTSTR strMessage;
	DWORD dwResult = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErrorCode, 0, (LPTSTR)&strMessage, 0, NULL);
	strError = strMessage;
	LocalFree(strMessage);
	strError.TrimRight(CRLF);
	return I2B(dwResult);
}

inline CString GetOSErrorMessage(DWORD dwErrorCode)
{
	CString strError;
	GetOSErrorMessage(dwErrorCode, strError);
	return strError;
}
#endif // FormatMessage

inline CString ToString(double d)
{
	CString str;
	str.Format(_T("%.2lf"), d);
	return str;
}
inline CString ToString(UINT d)
{
	CString str;
	str.Format(_T("%u"), d);
	return str;
}

inline CString ToString(const CStringArray& array, LPCTSTR strDelimiter = _T(", "), bool bSkipEmpty = true, LPCTSTR strFront = NULL, LPCTSTR strBack = NULL, INT_PTR iStart = 0, INT_PTR iEnd = -1)
{
	CString str;

	const INT_PTR iTotal = array.GetCount();

	if (iEnd == -1)
		iEnd = iTotal;
	else
		iEnd = min(iEnd, iTotal);

	for (INT_PTR iCount = iStart; iCount < iEnd; ++iCount)
	{
		if ((iCount > iStart) && (strDelimiter))
			str += strDelimiter;

		if ((bSkipEmpty) && (array[iCount].IsEmpty()))
			continue;

		if (strFront)
			str += strFront;

		str += array[iCount];

		if (strBack)
			str += strBack;
	}

	return str;
}
inline int Tokenise(const CString& str, CStringArray& array, TCHAR chToken, LPCTSTR strTrim = NULL, bool bSkipEmpty = false)
{
	// Makes sense to return whole string as single element if token is EOS
	//if (chToken == SZ)
	//	return -1;

	CString strBuffer(str);

	if (strTrim)
		strBuffer.Trim(strTrim);	// Trim left AND right

	if (strBuffer.IsEmpty())
		return 0;

	int iIndex = strBuffer.Find(chToken);
	if (iIndex == -1)
	{
		array.Add(strBuffer);
	}
	else
	{
		while (iIndex > -1)
		{
			CString strLeft(strBuffer.Left(iIndex));

			if ((strLeft.IsEmpty() == false) || (bSkipEmpty == false))
				array.Add(strLeft);

			strBuffer = strBuffer.Mid(iIndex + 1);

			if (strTrim)
				strBuffer.TrimLeft(strTrim);

			iIndex = strBuffer.Find(chToken);
		}

		if ((strBuffer.IsEmpty() == false) || (bSkipEmpty == false))
			array.Add(strBuffer);
	}

	return (int)array.GetCount();
}
inline int Replace(const CString& src, CString& dest, const CMapStringToString& map)
{
	int iCount = 0;

	for (int iIndex = 0;;)
	{
		int iPos = -1;
		const CMapStringToString::CPair* pPairMatch = NULL;
		for (const CMapStringToString::CPair* pPair = map.PGetFirstAssoc(); pPair; pPair = map.PGetNextAssoc(pPair))
		{
			int iFound = src.Find(pPair->key, iIndex);
			if ((iFound > -1) && ((iPos == -1) || (iFound < iPos)))
			{
				iPos = iFound;
				pPairMatch = pPair;
			}
		}

		if (iPos == -1)
		{
			dest += src.Mid(iIndex);
			break;
		}
		else
		{
			dest += src.Mid(iIndex, (iPos - iIndex));
			dest += pPairMatch->value;
			iIndex = iPos + (int)_tcslen(pPairMatch->key);
			++iCount;
		}
	}

	return iCount;
}

inline int Replace(CString& str, const CMapStringToString& map)
{
	CString strOld(str);
	str.Empty();
	return Replace(strOld, str, map);
}

}
}
