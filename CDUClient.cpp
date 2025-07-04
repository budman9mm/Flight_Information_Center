#include "CDUClient.h"
#include "resource.h"
#include <sstream>
#include <algorithm>

SOCKET g_socket = INVALID_SOCKET;
int g_cduIndex = 0;
int g_package[24][14][3] = { {{0}} };
HANDLE g_thread = NULL;
volatile bool g_running = false;
HFONT g_hFont = NULL;

bool InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        MessageBoxA(NULL, ("Winsock init failed: " + std::to_string(result)).c_str(), "Error", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

bool ConnectToServer(const char* ip, int port, HWND hwndDlg) {
    g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_socket == INVALID_SOCKET) {
        MessageBoxA(hwndDlg, ("Socket creation failed: " + std::to_string(WSAGetLastError())).c_str(), "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    u_long mode = 1;
    if (ioctlsocket(g_socket, FIONBIO, &mode) != 0) {
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
            MessageBoxA(hwndDlg, (std::string("Connection to ") + ip + ":" + std::to_string(port) + " failed: " + std::to_string(error)).c_str(), "Error", MB_OK | MB_ICONERROR);
            closesocket(g_socket);
            return false;
        }
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(g_socket, &writeSet);
        struct timeval timeout = { 2, 0 };
        if (select(0, NULL, &writeSet, NULL, &timeout) <= 0) {
            MessageBoxA(hwndDlg, "Connection timed out", "Error", MB_OK | MB_ICONERROR);
            closesocket(g_socket);
            return false;
        }
    }
    MessageBoxA(hwndDlg, (std::string("Connected to ") + ip + ":" + std::to_string(port)).c_str(), "Info", MB_OK);
    PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Connected to " + std::string(ip) + ":" + std::to_string(port))));
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
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Receive failed: Error " + std::to_string(error))));
            break;
        }
        if (bytesReceived == 0) {
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Connection closed by server")));
            break;
        }
        tempBuffer[bytesReceived] = '\0';
        buffer += tempBuffer;
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Received " + std::to_string(bytesReceived) + " bytes, total buffered: " + std::to_string(buffer.size()))));
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Raw data: " + buffer.substr(0, std::min<size_t>(100, buffer.size())))));
        SendMessage(GetDlgItem(hwndDlg, IDC_RAW_DATA), WM_SETTEXT, 0, (LPARAM)buffer.c_str());

        if (buffer.size() < 1012) {
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Waiting for more data: " + std::to_string(bytesReceived) + " bytes")));
            continue;
        }

        if (buffer.size() >= 2 && buffer[0] == '#' && buffer[1] == 'X') {
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Skipped prefix: #" + std::string(1, buffer[1]))));
        }
        else {
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("No valid #X prefix found: " + buffer.substr(0, std::min<size_t>(10, buffer.size())))));
            buffer.clear();
            continue;
        }

       if (buffer.size() < 338) {
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Insufficient data for text: " + std::to_string(buffer.size()) + " bytes")));
            continue;
        }

        std::string textData = buffer.substr(2, 1008);
        for (int i = 0; i < 14; i++) {
            for (int j = 0; j < 24; j++) {
                int idx = i * 14 + j;
                g_package[i][j][0] = (unsigned char)textData[idx];
                g_package[i][j][1] = 0;
                g_package[i][j][2] = 0;
            }
        }
        //PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Parsed 336 text bytes")));
        //PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Sample: [" + std::to_string(g_package[0][0][0]) + "]")));
        buffer.clear();
        PostMessage(hwndDlg, WM_USER + 1, 0, 0);
    }
    if (g_hFont) {
        DeleteObject(g_hFont);
        g_hFont = NULL;
    }
    return 0;
}

void SendCDUIndex(SOCKET sock, int cduIndex, HWND hwndDlg) {
    std::string message = "SET CDU " + std::to_string(cduIndex);
    int bytesSent = send(sock, message.c_str(), static_cast<int>(message.size()), 0);
    if (bytesSent == SOCKET_ERROR) {
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Failed to send CDU index: " + std::to_string(WSAGetLastError()))));
    }
    else {
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Sent CDU index: " + std::to_string(cduIndex))));
    }
}

void UpdateCDUDisplay(HWND hwndDlg, const int package[24][14][3]) {
    HWND grid = GetDlgItem(hwndDlg, IDC_CDU_GRID);
    if (!grid) {
        MessageBoxA(hwndDlg, "Grid control not found", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    std::string displayText;
    for (int i = 0; i < 14; i++) {
        for (int j = 0; j < 24; j++) {
            char c = (char)package[i][j][0];
            //displayText += (c >= 32 && c <= 126) ? c : ' ';
			displayText += c; // Directly append the character
        }
        displayText += "\r\n";
    }
    SetDlgItemTextA(hwndDlg, IDC_CDU_GRID, displayText.c_str());
    InvalidateRect(grid, NULL, TRUE);
    UpdateWindow(grid);
    PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Grid updated with " + std::to_string(displayText.size()) + " chars")));
    if (package[0][0][0] == 0) {
        std::string testText = "Test Grid:\r\n";
        for (int i = 0; i < 24; i++) {
            testText += std::string(24, 'X') + "\r\n";
        }
        SetDlgItemTextA(hwndDlg, IDC_CDU_GRID, testText.c_str());
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("No data parsed, showing test grid")));
    }
}

void LogDebugMessage(HWND hwndDlg, const std::string& message) {
    HWND debugText = GetDlgItem(hwndDlg, IDC_DEBUG_TEXT);
    if (debugText) {
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, message.c_str());
        InvalidateRect(debugText, NULL, TRUE);
        UpdateWindow(debugText);
    }
    else {
        MessageBoxA(hwndDlg, ("Debug text control not found: " + message).c_str(), "Error", MB_OK | MB_ICONERROR);
    }
}

void DisconnectSocket(HWND hwndDlg) {
    if (g_socket != INVALID_SOCKET) {
        shutdown(g_socket, SD_BOTH);
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Socket disconnected")));
    }
}

INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        g_hFont = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Futura Md BT");
        if (!g_hFont) {
            g_hFont = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
        }
        SendMessage(GetDlgItem(hwndDlg, IDC_CDU_GRID), WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT), WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(GetDlgItem(hwndDlg, IDC_RAW_DATA), WM_SETFONT, (WPARAM)g_hFont, TRUE);

        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, "Dialog initialized");
        InvalidateRect(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT), NULL, TRUE);
        UpdateWindow(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT));
        HWND debugText = GetDlgItem(hwndDlg, IDC_DEBUG_TEXT);
        HWND grid = GetDlgItem(hwndDlg, IDC_CDU_GRID);
        HWND rawData = GetDlgItem(hwndDlg, IDC_RAW_DATA);
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Debug text control: " + std::string(debugText ? "Created" : "Not found")).c_str());
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Grid control: " + std::string(grid ? "Created" : "Not found")).c_str());
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Raw data control: " + std::string(rawData ? "Created" : "Not found")).c_str());
        SetDlgItemTextA(hwndDlg, IDC_CDU_GRID, "Initial Grid");
        InvalidateRect(hwndDlg, NULL, TRUE);
        UpdateWindow(hwndDlg);
        if (ConnectToServer("192.168.1.5", 27016, hwndDlg)) {
            SendCDUIndex(g_socket, g_cduIndex, hwndDlg);
            g_running = true;
            g_thread = CreateThread(NULL, 0, ReceiveThread, hwndDlg, 0, NULL);
            if (!g_thread) {
                SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, "Failed to create receive thread");
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
        int cellWidth = gridWidth / gridCols;
        int cellHeight = gridHeight / gridRows;
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
            SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, msg->c_str());
            InvalidateRect(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT), NULL, TRUE);
            UpdateWindow(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT));
            delete msg;
        }
        return TRUE;
    }
    case WM_DESTROY:
        if (g_hFont) {
            DeleteObject(g_hFont);
            g_hFont = NULL;
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