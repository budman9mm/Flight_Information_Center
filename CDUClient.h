#ifndef CDUCLIENT_H
#define CDUCLIENT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

#define IDD_CDU_DIALOG 101
#define IDC_CDU_GRID 1000
#define IDC_CDU_BUTTON 2000
#define IDC_DEBUG_TEXT 2001
#define IDC_DISCONNECT_BUTTON 2002
#define IDC_EXIT_BUTTON 2003

// Global variables
extern SOCKET g_socket;
extern int g_cduIndex;
extern int g_package[24][14][3];


// Function declarations
INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool InitializeWinsock();
bool ConnectToServer(const char* ip, int port, HWND hwndDlg);
//bool ReceiveCDUData(SOCKET sock, int package[24][14][3], HWND hwndDlg);
void SendCDUIndex(SOCKET sock, int cduIndex, HWND hwndDlg);
void UpdateCDUDisplay(HWND hwndDlg, const int package[24][14][3]);
void LogDebugMessage(HWND hwndDlg, const std::string& message);
void DisconnectSocket(HWND hwndDlg);

#endif // CDUCLIENT_H
