// SerialPort.cpp : implementation file
//

#include "afxcoll.h"
#include "SerialPort.h"
#include "atlstr.h"
#include "afxwin.h"

//#define _AFXDLL
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// SerialPort.cpp: implementation of the CSerialPort class.

// Written by Shibu K.V (shibukv@erdcitvm.org)
// Copyright (c) 2002
//
// To use CSerialPort, follow these steps:
//   - Copy the files SerialPort.h & SerialPort.cpp and paste it in your
//		Projects Directory.
//   - Take Project tab from your VC++ IDE,Take Add to Project->Files.Select files 
//		SerialPort.h & SerialPort.cpp and click ok
//	 -	Add the line #include "SerialPort.h" at the top of your Dialog's Header File.
//   - Create an instance of CSerialPort in your dialogs header File.Say
//		CSerialPort port;
 
// Warning: this code hasn't been subject to a heavy testing, so
// use it on your own risk. The author accepts no liability for the 
// possible damage caused by this code.
//
// Version 1.0  7 Sept 2002.

//////////////////////////////////////////////////////////////////////

// CSerialPort

CSerialPort::CSerialPort(){
}

CSerialPort::~CSerialPort(){
}


BEGIN_MESSAGE_MAP(CSerialPort, CWnd)
	//{{AFX_MSG_MAP(CSerialPort)
	// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CSerialPort message handlers

HANDLE CSerialPort::OpenPort(CString portname){
	portname= "//./" +portname;

	hComm = CreateFile(portname,
                      GENERIC_READ | GENERIC_WRITE,
                      0,
                      0,
                      OPEN_EXISTING,
                      0,
                      0);
	if(hComm==INVALID_HANDLE_VALUE){
		//MessageBox("Cannot open Communication Port.Please\nQuit the program and Re-start your PC.","Com Port Error",MB_OK+MB_ICONERROR);
		return false;}
	else
		return hComm;

}

BOOL CSerialPort::ConfigurePort(DWORD BaudRate, BYTE ByteSize, DWORD fParity, BYTE Parity, BYTE StopBits){
	if((m_bPortReady = GetCommState(hComm, &m_dcb))==0){
		MessageBox((LPCTSTR)"GetCommState Error", (LPCTSTR)"Error",MB_OK+MB_ICONERROR);
		CloseHandle(hComm);
	return false;
	}

	m_dcb.BaudRate =BaudRate;
	m_dcb.ByteSize = ByteSize;
	m_dcb.Parity =Parity ;
	m_dcb.StopBits =StopBits;
	m_dcb.fBinary=TRUE;
	m_dcb.fDsrSensitivity=false;
	m_dcb.fParity=fParity;
	m_dcb.fOutX=false;
	m_dcb.fInX=false;
	m_dcb.fNull=false;
	m_dcb.fAbortOnError=TRUE;
	m_dcb.fOutxCtsFlow=FALSE;
	m_dcb.fOutxDsrFlow=false;
	m_dcb.fDtrControl=DTR_CONTROL_DISABLE;
	m_dcb.fDsrSensitivity=false;
	m_dcb.fRtsControl=RTS_CONTROL_DISABLE;
	m_dcb.fOutxCtsFlow=false;
	m_dcb.fOutxCtsFlow=false;
	m_bPortReady = SetCommState(hComm, &m_dcb);
	
	if(m_bPortReady ==0){
		MessageBox((LPCTSTR)"SetCommState Error", (LPCTSTR)"Error",MB_OK+MB_ICONERROR);
		CloseHandle(hComm);
		return false;
	}
	return true;
}

BOOL CSerialPort::SetCommunicationTimeouts(DWORD ReadIntervalTimeout, DWORD ReadTotalTimeoutMultiplier, DWORD ReadTotalTimeoutConstant, DWORD WriteTotalTimeoutMultiplier, DWORD WriteTotalTimeoutConstant){

	if((m_bPortReady = GetCommTimeouts (hComm, &m_CommTimeouts))==0)
	   return false;
	m_CommTimeouts.ReadIntervalTimeout =ReadIntervalTimeout;
	m_CommTimeouts.ReadTotalTimeoutConstant =ReadTotalTimeoutConstant;
	m_CommTimeouts.ReadTotalTimeoutMultiplier =ReadTotalTimeoutMultiplier;
	m_CommTimeouts.WriteTotalTimeoutConstant = WriteTotalTimeoutConstant;
	m_CommTimeouts.WriteTotalTimeoutMultiplier =WriteTotalTimeoutMultiplier;
	m_bPortReady = SetCommTimeouts (hComm, &m_CommTimeouts);
	if(m_bPortReady ==0){
		MessageBox((LPCTSTR)"StCommTimeouts function failed", (LPCTSTR)"Com Port Error",MB_OK+MB_ICONERROR);
		CloseHandle(hComm);
		return false;
	}
	return true;
}

BOOL CSerialPort::WriteByte(unsigned short bybyte, DWORD bytesToWrite){
	iBytesWritten=0;
	if(WriteFile(hComm, &bybyte, bytesToWrite, &iBytesWritten, NULL)==0)
		return false;
	else 
		return true;
}

BOOL CSerialPort::ReadByte(unsigned long long &resp, DWORD bytesToRead){
	unsigned long long rx;
	resp = 255;
	DWORD dwBytesTransferred = 0;

	if (ReadFile (hComm, &rx, bytesToRead, &dwBytesTransferred, 0)){
		if (dwBytesTransferred > 0){
			resp=rx;
			return TRUE;
		}
	}		  
	return FALSE;
}

void CSerialPort::ClosePort(){
	CloseHandle(hComm);
	return;
}

int CSerialPort::ReadByte2(unsigned long long &resp, DWORD bytesToRead) {
	unsigned long long rx;
	resp = 255;
	DWORD dwBytesTransferred = 0;

	if (ReadFile(hComm, &rx, bytesToRead, &dwBytesTransferred, 0)) {
		if (dwBytesTransferred > 0) {
			resp = rx;
			return dwBytesTransferred;
		}
	}
	return -1;
}