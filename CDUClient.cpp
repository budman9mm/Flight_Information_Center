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
HFONT g_hFont = NULL;
bool g_console = true; // Console debugging flag

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
        SendMessage(GetDlgItem(hwndDlg, IDC_RAW_DATA), WM_SETTEXT, 0, (LPARAM)buffer.c_str());

        if (buffer.size() < 1010) {
            if (g_console) printf_s("Waiting for more data: %zu bytes\n", buffer.size());
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
                    g_package[j][i][h] = (unsigned char)textData[idx];
                    printf_s("%c", g_package[j][i][h]);
                    idx++;
                }
                printf_s("\n");
            }
        }
        if (g_console) printf_s("Parsed text bytes\n");
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Parsed 336 text bytes\r\n")));
        if (g_console) printf_s("Sample: [%d]\n", g_package[0][0][0]);
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Sample: [" + std::to_string(g_package[0][0][0]) + "]\r\n")));
        buffer.clear();
        PostMessage(hwndDlg, WM_USER + 1, 0, 0);
    }
    if (g_hFont) {
        DeleteObject(g_hFont);
        g_hFont = NULL;
    }
    if (g_console) {
        FreeConsole();
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

void UpdateCDUDisplay(HWND hwndDlg, const int package[24][14][3]) {
    HWND grid = GetDlgItem(hwndDlg, IDC_CDU_GRID);
    if (!grid) {
        if (g_console) printf_s("Grid control not found\n");
        MessageBoxA(hwndDlg, "Grid control not found", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    std::string displayText;
    for (int i = 0; i < 14; ++i) {
        for (int j = 0; j < 24; ++j) {
            char c = (char)package[j][i][0];
            displayText += (c >= 32 && c <= 126) ? c : ' ';
        }
        displayText += "\r\n";
    }
    SetDlgItemTextA(hwndDlg, IDC_CDU_GRID, displayText.c_str());
    InvalidateRect(grid, NULL, TRUE);
    UpdateWindow(grid);
    if (g_console) printf_s("Grid updated with %zu chars\n", displayText.size());
    PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Grid updated with " + std::to_string(displayText.size()) + " chars\r\n")));
    if (package[0][0][0] == 0) {
        std::string testText = "Test Grid:\r\n";
        for (int j = 0; j < 24; j++) {
            testText += std::string(14, 'X') + "\r\n";
        }
        SetDlgItemTextA(hwndDlg, IDC_CDU_GRID, testText.c_str());
        if (g_console) printf_s("No data parsed, showing test grid\n");
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("No data parsed, showing test grid\r\n")));
    }
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
        if (g_console) printf_s("Socket disconnected\n");
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Socket disconnected\r\n")));
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
        g_hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Cascadia Code");
        if (!g_hFont) {
            g_hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
        }

        SendMessage(GetDlgItem(hwndDlg, IDC_CDU_GRID), WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT), WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(GetDlgItem(hwndDlg, IDC_RAW_DATA), WM_SETFONT, (WPARAM)g_hFont, TRUE);

        LogDebugMessage(hwndDlg, "Dialog initialized\r\n");
        InvalidateRect(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT), NULL, TRUE);
        UpdateWindow(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT));
        HWND debugText = GetDlgItem(hwndDlg, IDC_DEBUG_TEXT);
        HWND grid = GetDlgItem(hwndDlg, IDC_CDU_GRID);
        HWND rawData = GetDlgItem(hwndDlg, IDC_RAW_DATA);
        LogDebugMessage(hwndDlg, "Debug text control: " + std::string(debugText ? "Created" : "Not found") + "\r\n");
        LogDebugMessage(hwndDlg, "Grid control: " + std::string(grid ? "Created" : "Not found") + "\r\n");
        LogDebugMessage(hwndDlg, "Raw data control: " + std::string(rawData ? "Created" : "Not found") + "\r\n");
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
        }
        return TRUE;
    }
    case WM_SIZE: {
        int dlgWidth = LOWORD(lParam);
        int dlgHeight = HIWORD(lParam);
        int left = 20, top = 245, right = 20, bottom = 80;
        int gridRows = 24, gridCols = 14;
        int gridWidth = dlgWidth - left - right;
        int gridHeight = dlgHeight - top - bottom;
        MoveWindow(GetDlgItem(hwndDlg, IDC_CDU_GRID), left, top, gridWidth, gridHeight, TRUE);
        MoveWindow(GetDlgItem(hwndDlg, IDC_CDU_BUTTON), 20, dlgHeight - 70, 100, 30, TRUE);
        MoveWindow(GetDlgItem(hwndDlg, IDC_DISCONNECT_BUTTON), 130, dlgHeight - 70, 100, 30, TRUE);
        MoveWindow(GetDlgItem(hwndDlg, IDC_EXIT_BUTTON), 240, dlgHeight - 70, 70, 30, TRUE);
        MoveWindow(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT), 10, 150, 200, 45, TRUE);
        MoveWindow(GetDlgItem(hwndDlg, IDC_RAW_DATA), 10, 10, 200, 100, TRUE);
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
    case WM_DESTROY:
        if (g_hFont) {
            DeleteObject(g_hFont);
            g_hFont = NULL;
        }
        if (g_console) {
            FreeConsole();
        }
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CDU_BUTTON) {
            g_cduIndex = (g_cduIndex + 1) % 3;
            SendCDUIndex(g_socket, g_cduIndex, hwndDlg);
            UpdateCDUDisplay(hwndDlg, g_package);
        }
        else if (LOWORD(wParam) == IDC_DISCONNECT_BUTTON) {
            g_running = false;
            DisconnectSocket(hwndDlg);
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!InitializeWinsock()) {
        MessageBoxA(NULL, "Winsock initialization failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    MessageBoxA(NULL, "Starting client", "Debug", MB_OK);
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_CDU_DIALOG), NULL, DialogProc);
    return 0;
}