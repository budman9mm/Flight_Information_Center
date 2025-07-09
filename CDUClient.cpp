#include "CDUClient.h"
#include "resource.h"
#include <sstream>
#include <algorithm>
#include <stdio.h>

SOCKET g_socket = INVALID_SOCKET;
int g_cduIndex = 0;
int g_package[24][14][3] = { {{0}} };
HANDLE g_thread = NULL;
volatile bool g_running = false;
HFONT g_hFontGrid = NULL;
HFONT g_hFontDebug = NULL;
bool g_console = false; // Console debugging flag -- Turn on for console output
bool g_connected = false;

//PMDG annunciators
bool CDU_annunEXEC, CDU_annunDSPY, CDU_annunFAIL, CDU_annunMSG, CDU_annunOFST;//L/R/C CDU annunciators
bool CDU_annunEXEC_p, CDU_annunDSPY_p, CDU_annunFAIL_p, CDU_annunMSG_p, CDU_annunOFST_p;

// Button command structure
struct ButtonCommand {
    UINT controlId;
    int zcode;
    const char* descrition;
};
const ButtonCommand cduButtons[] = {
    {IDC_LSK1L, 69960, "EVT_CDU_L_L1"},
	{IDC_LSK2L, 69961, "EVT_CDU_L_L2"},
	{IDC_LSK3L, 69962, "EVT_CDU_L_L3"},
	{IDC_LSK4L, 69963, "EVT_CDU_L_L4"},
	{IDC_LSK5L, 69964, "EVT_CDU_L_L5"},
	{IDC_LSK6L, 69965, "EVT_CDU_L_L6"},
	{IDC_LSK1R, 69966, "EVT_CDU_L_R1"},
	{IDC_LSK2R, 69967, "EVT_CDU_L_R2"},
	{IDC_LSK3R, 69968, "EVT_CDU_L_R3"},
	{IDC_LSK4R, 69969, "EVT_CDU_L_R4"},
	{IDC_LSK5R, 69970, "EVT_CDU_L_R5"},
	{IDC_LSK6R, 69971, "EVT_CDU_L_R6"},
	{IDC_INIT_REF, 69972, "EVT_CDU_L_INIT_REF"},
	{IDC_RTE, 69973, "EVT_CDU_L_RTE"},
	{IDC_DEP_ARR, 69974, "EVT_CDU_L_DEP_ARR"},
	{IDC_ALTN, 69975, "EVT_CDU_L_ALTN"},
	{IDC_VNAV, 69976, "EVT_CDU_L_VNAV"},
	{IDC_FIX, 69977, "EVT_CDU_L_FIX"},
	{IDC_LEGS, 69978, "EVT_CDU_L_LEGS"},
	{IDC_HOLD, 69979, "EVT_CDU_L_HOLD"},
	{IDC_FMC_COMM, 73103, "EVT_CDU_L_FMC_COMM"},
	{IDC_PROG, 69980, "EVT_CDU_L_PROG"},
	{IDC_KEY_EXEC, 69981, "EVT_CDU_L_EXEC"},
    {IDC_MENU, 69982, "EVT_CDU_L_MENU"},
	{IDC_NAV_RAD, 69983, "EVT_CDU_L_NAV_RAD"},
	{IDC_PREV_PAGE, 69984, "EVT_CDU_L_PREV_PAGE"},
	{IDC_NEXT_PAGE, 69985, "EVT_CDU_L_NEXT_PAGE"},
	{IDC_KEY_1, 69986, "EVT_CDU_L_1"},
	{IDC_KEY_2, 69987, "EVT_CDU_L_2"},
	{IDC_KEY_3, 69988, "EVT_CDU_L_3"},
	{IDC_KEY_4, 69989, "EVT_CDU_L_4"},
	{IDC_KEY_5, 69990, "EVT_CDU_L_5"},
	{IDC_KEY_6, 69991, "EVT_CDU_L_6"},
	{IDC_KEY_7, 69992, "EVT_CDU_L_7"},
	{IDC_KEY_8, 69993, "EVT_CDU_L_8"},
	{IDC_KEY_9, 69994, "EVT_CDU_L_9"},
	{IDC_KEY_DOT, 69995, "EVT_CDU_L_DOT"},
	{IDC_KEY_0, 69996, "EVT_CDU_L_0"},
	{IDC_KEY_PLUS_MINUS, 69997, "EVT_CDU_L_PLUS_MINUS"},
	{IDC_KEY_A, 69998, "EVT_CDU_L_A"},
	{IDC_KEY_B, 69999, "EVT_CDU_L_B"},
	{IDC_KEY_C, 70000, "EVT_CDU_L_C"},
	{IDC_KEY_D, 70001, "EVT_CDU_L_D"},
	{IDC_KEY_E, 70002, "EVT_CDU_L_E"},
	{IDC_KEY_F, 70003, "EVT_CDU_L_F"},
	{IDC_KEY_G, 70004, "EVT_CDU_L_G"},
	{IDC_KEY_H, 70005, "EVT_CDU_L_H"},
	{IDC_KEY_I, 70006, "EVT_CDU_L_I"},
	{IDC_KEY_J, 70007, "EVT_CDU_L_J"},
	{IDC_KEY_K, 70008, "EVT_CDU_L_K"},
	{IDC_KEY_L, 70009, "EVT_CDU_L_L"},
	{IDC_KEY_M, 70010, "EVT_CDU_L_M"},
	{IDC_KEY_N, 70011, "EVT_CDU_L_N"},
	{IDC_KEY_O, 70012, "EVT_CDU_L_O"},
	{IDC_KEY_P, 70013, "EVT_CDU_L_P"},
	{IDC_KEY_Q, 70014, "EVT_CDU_L_Q"},
	{IDC_KEY_R, 70015, "EVT_CDU_L_R"},
	{IDC_KEY_S, 70016, "EVT_CDU_L_S"},
	{IDC_KEY_T, 70017, "EVT_CDU_L_T"},
	{IDC_KEY_U, 70018, "EVT_CDU_L_U"},
	{IDC_KEY_V, 70019, "EVT_CDU_L_V"},
	{IDC_KEY_W, 70020, "EVT_CDU_L_W"},
	{IDC_KEY_X, 70021, "EVT_CDU_L_X"},
	{IDC_KEY_Y, 70022, "EVT_CDU_L_Y"},
	{IDC_KEY_Z, 70023, "EVT_CDU_L_Z"},
	{IDC_KEY_SP, 70024, "EVT_CDU_L_SPACE"},
	{IDC_KEY_DEL, 70025, "EVT_CDU_L_DEL"},
	{IDC_KEY_SLASH, 70026, "EVT_CDU_L_SLASH"},
	{IDC_KEY_CLR, 70027, "EVT_CDU_L_CLR"},
	//{IDC_KEY_BRITENESS, 70032, "EVT_CDU_L_BRIGHTNESS"},
};

bool InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        if (g_console) printf_s("Winsock init failed: %d\n", result);
        MessageBoxA(NULL, ("Winsock init failed: " + std::to_string(result)).c_str(), "Error", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

bool ConnectToServer(const char* ip, int port, HWND hwndDlg) {
    g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_socket == INVALID_SOCKET) {
        if (g_console) printf_s("Socket creation failed: %d\n", WSAGetLastError());
        MessageBoxA(hwndDlg, ("Socket creation failed: " + std::to_string(WSAGetLastError())).c_str(), "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    u_long mode = 1;
    if (ioctlsocket(g_socket, FIONBIO, &mode) != 0) {
        if (g_console) printf_s("Failed to set non-blocking mode: %d\n", WSAGetLastError());
        MessageBoxA(hwndDlg, ("Failed to set non-blocking mode: " + std::to_string(WSAGetLastError())).c_str(), "Error", MB_OK | MB_ICONERROR);
        closesocket(g_socket);
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serverAddr.sin_addr);

    if (connect(g_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            if (g_console) printf_s("Connection to %s:%d failed: %d\n", ip, port, error);
            MessageBoxA(hwndDlg, (std::string("Connection to ") + ip + ":" + std::to_string(port) + " failed: " + std::to_string(error)).c_str(), "Error", MB_OK | MB_ICONERROR);
            closesocket(g_socket);
            return false;
        }
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(g_socket, &writeSet);
        struct timeval timeout = { 2, 0 };
        if (select(0, NULL, &writeSet, NULL, &timeout) <= 0) {
            if (g_console) printf_s("Connection to %s:%d timed out\n", ip, port);
            MessageBoxA(hwndDlg, "Connection timed out", "Error", MB_OK | MB_ICONERROR);
            closesocket(g_socket);
            return false;
        }
    }
    if (g_console) printf_s("Connected to %s:%d\n", ip, port);
    MessageBoxA(hwndDlg, (std::string("Connected to ") + ip + ":" + std::to_string(port)).c_str(), "Info", MB_OK);
    PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Connected to " + std::string(ip) + ":" + std::to_string(port) + "\r\n")));
    g_connected = true;
    SetDlgItemTextA(hwndDlg, IDC_DISCONNECT_BUTTON, "Disconnect");
    return true;
}

DWORD WINAPI ReceiveThread(LPVOID lpParam) {
    HWND hwndDlg = (HWND)lpParam;
    std::string buffer;
    char tempBuffer[4096];
    while (g_running && g_socket != INVALID_SOCKET) {
        int bytesReceived = recv(g_socket, tempBuffer, sizeof(tempBuffer) - 1, 0);
        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                Sleep(100);
                continue;
            }
            if (g_console) printf_s("Receive failed: Error %d\n", error);
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Receive failed: Error " + std::to_string(error) + "\r\n")));
            break;
        }
        if (bytesReceived == 0) {
            if (g_console) printf_s("Connection closed by server\n");
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Connection closed by server\r\n")));
            break;
        }
        tempBuffer[bytesReceived] = '\0';
        buffer += tempBuffer;
        if (g_console) printf_s("Received %d bytes, total buffered: %zu\n", bytesReceived, buffer.size());
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Received " + std::to_string(bytesReceived) + " bytes, total buffered: " + std::to_string(buffer.size()) + "\r\n")));
        if (g_console) printf_s("Raw data: %.*s\n", (int)std::min<size_t>(100, buffer.size()), buffer.c_str());
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Raw data: " + buffer.substr(0, std::min<size_t>(100, buffer.size())) + "\r\n")));
      

        if (buffer.size() < 1010) {
            if (g_console) printf_s("Incomplete data received(1010): %zu bytes\n", buffer.size());
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Waiting for more data: " + std::to_string(buffer.size()) + " bytes\r\n")));
            continue;
        }

        if (buffer.size() >= 2 && buffer[0] == '#' && buffer[1] =='X') {
            if (g_console) printf_s("Skipped prefix: #%c\n", buffer[1]);
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Skipped prefix: #" + std::string(1, buffer[1]) + "\r\n")));
        }
        else {
            if (g_console) printf_s("No valid #X prefix found: %.*s\n", (int)std::min<size_t>(10, buffer.size()), buffer.c_str());
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("No valid #X prefix found: " + buffer.substr(0, std::min<size_t>(10, buffer.size())) + "\r\n")));
            buffer.clear();
            continue;
        }

        //if (buffer[buffer.size() - 1] != '\n') {
           // if (g_console) printf_s("No trailing newline, waiting for more data\n");
            //PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("No trailing newline, waiting for more data\r\n")));
            //continue;
        //}

        if (buffer.size() < 1010) {
            if (g_console) printf_s("Insufficient data for text: %zu bytes\n", buffer.size());
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Insufficient data for text: " + std::to_string(buffer.size()) + " bytes\r\n")));
            continue;
        }

        std::string textData = buffer.substr(2, 1008);
		//printf_s("%s\n", textData.c_str());
        int idx = 0; 
        for (int h = 0; h < 3; h++) {
            for (int i = 0; i < 14; ++i) {
                for (int j = 0; j < 24; ++j) {
                    g_package[j][i][h] = (char)textData[idx];
                    if (g_console) {
                        printf_s("%d", g_package[j][i][h]);
					}       
                    idx++;
                }
                 if (g_console) printf_s("\n");
            }
        }
        //if (g_console) printf_s("Parsed text bytes\n");
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Parsed 336 text bytes\r\n")));
        if (g_console) printf_s("Sample: [%d]\n", g_package[0][0][0]);
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Sample: [" + std::to_string(g_package[0][0][0]) + "]\r\n")));
        buffer.clear();
        PostMessage(hwndDlg, WM_USER + 1, 0, 0);
    }
    
    return 0;
}

void SendCDUIndex(SOCKET sock, int cduIndex, HWND hwndDlg) {
    std::string message = "SET CDU " + std::to_string(cduIndex);
    int bytesSent = send(sock, message.c_str(), static_cast<int>(message.size()), 0);
    if (bytesSent == SOCKET_ERROR) {
        if (g_console) printf_s("Failed to send CDU index: %d\n", WSAGetLastError());
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Failed to send CDU index: " + std::to_string(WSAGetLastError()) + "\r\n")));
    }
    else {
        if (g_console) printf_s("Sent CDU index: %d\n", cduIndex);
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Sent CDU index: " + std::to_string(cduIndex) + "\r\n")));
    }
}

void SendCDUControl(SOCKET sock, int controlCode, int param, HWND hwndDlg, std::string controlName) {
    std::string message = "Z" + std::to_string(controlCode) + ":" + std::to_string(param);//Zxxxxx:x
    int bytesSent = send(sock, message.c_str(), static_cast<int>(message.size()), 0);
    if (bytesSent == SOCKET_ERROR) {
        if (g_console) printf_s("Failed to send CDU control: %d\n", WSAGetLastError());
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Failed to send CDU control: " + std::to_string(WSAGetLastError()) + "\r\n")));
    }
    else {
        if (g_console) printf_s("Sent CDU control: %d:%d\n", controlCode, param);
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Sent " + controlName + " " + std::to_string(controlCode) +
            + ":" + std::to_string(param) + "\r\n")));
    }
}

void UpdateCDUDisplay(HWND hwndDlg, const int package[24][14][3]) {
    HWND grid = GetDlgItem(hwndDlg, IDC_CDU_GRID);
    if (!grid) {
        if (g_console) printf_s("Grid control not found\n");
        MessageBoxA(hwndDlg, "Grid control not found", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    std::string displayText; //Use wchar_t if we need to suport Unicode characters
    for (int i = 0; i < 14; ++i) {
		char c = ' ';
        for (int j = 0; j < 24; ++j) {
            if (package[j][i][0] == -94) {
                c = '»';  //Right arrow
                displayText += c;
            }
            else if (package[j][i][0] == -95) {
                c = '«';  //Left arrow
                displayText += c;
            }
            else if (package[j][i][0] == -80) {
                c = '°'; //Degree symbol
                displayText += c;
            }
            else if (package[j][i][0] == -22) { 
                c = '¤'; //replaces the box
                displayText += c;
            }
			else {  //No substitution, use the character as is
                c = (char)package[j][i][0];
                displayText += c;
            }
        }
        displayText += "\r\n";
    }
    
    PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Sub: [" + std::to_string(g_package[2][1][0]) + "]\r\n")));
	//SetDlgItemTextW(hwndDlg, IDC_CDU_GRID, displayText.c_str()); // Set wide string text to grid control
    SetDlgItemTextA(hwndDlg, IDC_CDU_GRID, displayText.c_str());
    InvalidateRect(grid, NULL, TRUE);
    UpdateWindow(grid);
    PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Grid updated with " + std::to_string(displayText.size()) + " chars\r\n")));
    
}

void LogDebugMessage(HWND hwndDlg, const std::string& message) {
    HWND debugText = GetDlgItem(hwndDlg, IDC_DEBUG_TEXT);
    if (debugText) {
        int len = GetWindowTextLengthA(debugText);
        SendMessageA(debugText, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessageA(debugText, EM_REPLACESEL, FALSE, (LPARAM)message.c_str());
        SendMessageA(debugText, EM_SCROLLCARET, 0, 0);
        InvalidateRect(debugText, NULL, TRUE);
        UpdateWindow(debugText);
    }
    else {
        if (g_console) printf_s("Debug text control not found: %s\n", message.c_str());
        MessageBoxA(hwndDlg, ("Debug text control not found: " + message).c_str(), "Error", MB_OK | MB_ICONERROR);
    }
}

void DisconnectSocket(HWND hwndDlg) {
    if (g_socket != INVALID_SOCKET) {
        shutdown(g_socket, SD_BOTH);
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        g_connected = false;
        if (g_console) printf_s("Socket disconnected\n");
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Socket disconnected\r\n")));
        SetDlgItemTextA(hwndDlg, IDC_DISCONNECT_BUTTON, "Connect");
    }
}

INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        if (g_console) {
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
        }
        g_hFontGrid = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Cascadia Code");
        /*if (!g_hFontGrid) {
            g_hFontGrid = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
        }*/
        g_hFontDebug = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Cascadia Code");
        if (!g_hFontDebug) {
            g_hFontDebug = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
        }
        SendMessage(GetDlgItem(hwndDlg, IDC_CDU_GRID), WM_SETFONT, (WPARAM)g_hFontGrid, TRUE);
        SendMessage(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT), WM_SETFONT, (WPARAM)g_hFontDebug, TRUE);

        LogDebugMessage(hwndDlg, "Dialog initialized\r\n");
        InvalidateRect(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT), NULL, TRUE);
        UpdateWindow(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT));
        HWND debugText = GetDlgItem(hwndDlg, IDC_DEBUG_TEXT);
        HWND grid = GetDlgItem(hwndDlg, IDC_CDU_GRID);
		HWND hGrid = GetDlgItem(hwndDlg, IDC_CDU_GRID); 
        //SetWindowLongPtr(hGrid, GWLP_WNDPROC, (LONG_PTR)GridWndProc);
        LogDebugMessage(hwndDlg, "Debug text control: " + std::string(debugText ? "Created" : "Not found") + "\r\n");
        LogDebugMessage(hwndDlg, "Grid control: " + std::string(grid ? "Created" : "Not found") + "\r\n");
        SetDlgItemTextA(hwndDlg, IDC_CDU_GRID, "Initial Grid\r\n");
        InvalidateRect(hwndDlg, NULL, TRUE);
        UpdateWindow(hwndDlg);
        if (ConnectToServer("192.168.1.5", 27016, hwndDlg)) {
            SendCDUIndex(g_socket, g_cduIndex, hwndDlg);
            g_running = true;
            g_thread = CreateThread(NULL, 0, ReceiveThread, hwndDlg, 0, NULL);
            if (!g_thread) {
                if (g_console) printf_s("Failed to create receive thread\n");
                LogDebugMessage(hwndDlg, "Failed to create receive thread\r\n");
            }
            // Recreate and reapply the font to the grid control
            if (!g_hFontGrid) {
                g_hFontGrid = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Cascadia Code");
            }
            SendMessage(GetDlgItem(hwndDlg, IDC_CDU_GRID), WM_SETFONT, (WPARAM)g_hFontGrid, TRUE);

        }
        return TRUE;
    }
    case WM_SIZE: {
        int dlgWidth = LOWORD(lParam);
        int dlgHeight = HIWORD(lParam);
        int left = 10, top = 10, right = 10, bottom = 80;
        int gridRows = 24, gridCols = 14;
        int gridWidth = dlgWidth - left - right;
        int gridHeight = 240;
        MoveWindow(GetDlgItem(hwndDlg, IDC_CDU_GRID), left, top, gridWidth, gridHeight, TRUE);
        MoveWindow(GetDlgItem(hwndDlg, IDC_CDU_BUTTON), 10, dlgHeight - 70, 100, 30, TRUE);
        MoveWindow(GetDlgItem(hwndDlg, IDC_DISCONNECT_BUTTON), 120, dlgHeight - 70, 100, 30, TRUE);
        MoveWindow(GetDlgItem(hwndDlg, IDC_EXIT_BUTTON), 230, dlgHeight - 70, 70, 30, TRUE);
        MoveWindow(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT), 10, dlgHeight - 140, 280, 60, TRUE);
        return TRUE;
    }
    case WM_USER + 1:
        UpdateCDUDisplay(hwndDlg, g_package);
        return TRUE;
    case WM_USER + 2: {
        std::string* msg = (std::string*)lParam;
        if (msg) {
            LogDebugMessage(hwndDlg, *msg);
            delete msg;
        }
        return TRUE;
    }
    
    case WM_CTLCOLORBTN: { // Handle button control colors
        HDC hdcButton = (HDC)wParam;
        HWND hwndButton = (HWND)lParam;
        if (hwndButton == GetDlgItem(hwndDlg, IDC_CDU_BUTTON) ||
            hwndButton == GetDlgItem(hwndDlg, IDC_DISCONNECT_BUTTON) ||
            hwndButton == GetDlgItem(hwndDlg, IDC_EXIT_BUTTON)) {
            SetTextColor(hdcButton, RGB(0, 0, 0)); // Black text
            SetBkColor(hdcButton, RGB(200, 200, 200)); // Light gray background
            return (INT_PTR)GetStockObject(LTGRAY_BRUSH);
        }
        break;
    }
                       // Add this to your DialogProc or main window procedure
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (lpdis->CtlID == IDC_STATIC_EXEC_LT3) {
            // Example: choose color based on a condition
            COLORREF fillColor = RGB(0, 200, 0); // Default green
            //if (/* your condition here */) {
              //  fillColor = RGB(200, 0, 0); // Red if condition met
           // }
            HBRUSH hBrush = CreateSolidBrush(fillColor);
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
            HGDIOBJ oldPen = SelectObject(lpdis->hDC, hPen);
            HGDIOBJ oldBrush = SelectObject(lpdis->hDC, hBrush);
            RoundRect(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top,
                lpdis->rcItem.right, lpdis->rcItem.bottom, 8, 8);
            SelectObject(lpdis->hDC, oldBrush);
            SelectObject(lpdis->hDC, oldPen);
            DeleteObject(hBrush);
            DeleteObject(hPen);
            return TRUE;
        }
        break;
    }
    case WM_DESTROY:
        if (g_hFontGrid) {
            DeleteObject(g_hFontGrid);
            g_hFontGrid = NULL;
        }
        if (g_hFontDebug) {
            DeleteObject(g_hFontDebug);
            g_hFontDebug = NULL;
        }
        if (g_console) {
            FreeConsole();
        }
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CDU_BUTTON) {
            g_cduIndex = (g_cduIndex + 1) % 3;
            if (g_connected) {
                SendCDUIndex(g_socket, g_cduIndex, hwndDlg);
            }
            UpdateCDUDisplay(hwndDlg, g_package);
        }    
        else if (LOWORD(wParam) == IDC_DISCONNECT_BUTTON) {
            if (g_connected) {
                g_running = false;
                DisconnectSocket(hwndDlg);
            }
            else {
                if (ConnectToServer("192.168.1.5", 27016, hwndDlg)) {
                    SendCDUIndex(g_socket, g_cduIndex, hwndDlg);
                    g_running = true;
                    g_thread = CreateThread(NULL, 0, ReceiveThread, hwndDlg, 0, NULL);
                    if (!g_thread) {
                        if (g_console) printf_s("Failed to create receive thread\n");
                        LogDebugMessage(hwndDlg, "Failed to create receive thread\r\n");
                    }
                }
            }
        }
        else if (LOWORD(wParam) == IDC_EXIT_BUTTON) {
            g_running = false;
            DisconnectSocket(hwndDlg);
            if (g_thread) {
                WaitForSingleObject(g_thread, 1000);
                CloseHandle(g_thread);
                g_thread = NULL;
            }
            WSACleanup();
            EndDialog(hwndDlg, 0);
        }
        else {
			for (const auto& button : cduButtons) { // Iterate through cdu button commands struct
                if (LOWORD(wParam) == button.controlId) {
                    if (g_connected) {
                        SendCDUControl(g_socket, button.zcode, 1, hwndDlg, button.descrition);
                    }
                    else {
                        MessageBoxA(hwndDlg, "Not connected to server", "Error", MB_OK | MB_ICONERROR);
                    }
                    break;
                }
            }
		}
        return TRUE;
    case WM_CLOSE:
        g_running = false;
        DisconnectSocket(hwndDlg);
        if (g_thread) {
            WaitForSingleObject(g_thread, 1000);
            CloseHandle(g_thread);
            g_thread = NULL;
        }
        WSACleanup();
        EndDialog(hwndDlg, 0);
        return TRUE;
    }
    return FALSE;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    if (!InitializeWinsock()) {
        MessageBoxA(NULL, "Winsock initialization failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    MessageBoxA(NULL, "Starting client", "Debug", MB_OK);
    int dlgResult = DialogBox(hInstance, MAKEINTRESOURCE(IDD_CDU_DIALOG), NULL, DialogProc);
    if (dlgResult == -1) {
        MessageBoxA(NULL, "DialogBox failed!", "Error", MB_OK | MB_ICONERROR);
    }
    return 0;
}