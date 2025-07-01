#ifndef CDUCLIENT_H
#define CDUCLIENT_H

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

#define IDD_CDU_DIALOG 101
#define IDC_CDU_GRID 1000
#define IDC_CDU_BUTTON 2000

// Global variables
extern SOCKET g_socket;
extern int g_cduIndex;
extern int g_package[24][14][3];

// Function declarations
INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool InitializeWinsock();
bool ConnectToServer(const char* ip, int port);
bool ReceiveCDUData(SOCKET sock, int package[24][14][3]);
void SendCDUIndex(SOCKET sock, int cduIndex);
void UpdateCDUDisplay(HWND hwndDlg, const int package[24][14][3]);

#endif // CDUCLIENT_H