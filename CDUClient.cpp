#include "CDUClient.h"
#include "resource.h"
#include <sstream>
#include <algorithm>

SOCKET g_socket = INVALID_SOCKET;
int g_cduIndex = 0;
int g_package[24][14][3] = { {{0}} };
HANDLE g_thread = NULL;
volatile bool g_running = false;

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

    // Set socket to non-blocking
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
            MessageBoxA(hwndDlg, ("Connection to " + std::string(ip) + ":" + std::to_string(port) + " failed: " + std::to_string(error)).c_str(), "Error", MB_OK | MB_ICONERROR);
        closesocket(g_socket);
        return false;
    }
    }
    MessageBoxA(hwndDlg, (std::string("Connected to ") + ip + ":" + std::to_string(port)).c_str(), "Info", MB_OK);
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
                Sleep(100); // Brief pause to avoid CPU spin
                continue;
            }
            SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Receive failed: Error " + std::to_string(error)).c_str());
            break;
        }
        if (bytesReceived == 0) {
            SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, "Connection closed by server");
            break;
        }
        tempBuffer[bytesReceived] = '\0';
        buffer += tempBuffer;
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Received " + std::to_string(bytesReceived) + " bytes, total buffered: " + std::to_string(buffer.size())).c_str());

        // Log raw data
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Raw data: " + buffer.substr(0, std::min<size_t>(100, buffer.size()))).c_str());

        // Need at least 1012 bytes
        if (buffer.size() < 1012) {
            SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Waiting for more data: " + std::to_string(buffer.size()) + " bytes").c_str());
            continue;
        }

        // Check #X prefix
        char* dataStart = &buffer[0];
        if (buffer.size() >= 2 && buffer[0] == '#' && (buffer[1] >= '0' && buffer[1] <= '2')) {
            dataStart += 2;
            SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Skipped prefix: #" + std::string(1, buffer[1])).c_str());
        }
        else {
            SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("No valid #X prefix found: " + buffer.substr(0, std::min<size_t>(10, buffer.size()))).c_str());
            buffer.clear();
            continue;
        }

        // Check trailing \n
        if (buffer[buffer.size() - 1] != '\n') {
            SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, "No trailing newline, waiting for more data");
            continue;
        }
        buffer.pop_back();

        std::stringstream ss(dataStart);
    std::string token;
        int i = 0, j = 0;
    while (std::getline(ss, token, '|') && i < 24) {
        if (token.empty()) continue;
        int charVal, color, attr;
        if (sscanf_s(token.c_str(), "%d,%d,%d", &charVal, &color, &attr) == 3) {
                g_package[i][j][0] = charVal;
                g_package[i][j][1] = color;
                g_package[i][j][2] = attr;
            j++;
            if (j >= 14) {
                j = 0;
                i++;
            }
        }
            else {
                SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Invalid token: " + token).c_str());
                buffer.clear();
                break;
    }
    return i == 24;
}
        if (i != 24 || j != 0) {
            SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Incomplete data: parsed " + std::to_string(i) + " rows, " + std::to_string(j) + " cols").c_str());
            buffer.clear();
            continue;
        }
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, "Parsed 336 elements successfully");
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Sample: [" + std::to_string(g_package[0][0][0]) + "," + std::to_string(g_package[0][0][1]) + "," + std::to_string(g_package[0][0][2]) + "]").c_str());
        buffer.clear();
        SendMessage(hwndDlg, WM_USER + 1, 0, 0); // Trigger display update
    }
    return 0;
}

void SendCDUIndex(SOCKET sock, int cduIndex, HWND hwndDlg) {
    int bytesSent = send(sock, (char*)&cduIndex, sizeof(int), 0);
    if (bytesSent == SOCKET_ERROR) {
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Failed to send CDU index: " + std::to_string(WSAGetLastError())).c_str());
    }
    else if (bytesSent == sizeof(int)) {
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Sent CDU index: " + std::to_string(cduIndex)).c_str());
    }
}

void UpdateCDUDisplay(HWND hwndDlg, const int package[24][14][3]) {
    HWND grid = GetDlgItem(hwndDlg, IDC_CDU_GRID);
    if (!grid) {
        MessageBoxA(hwndDlg, "Grid control not found", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    std::string displayText = "CDU Grid:\n";
    for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 14; j++) {
            char c = (char)package[i][j][0];
            displayText += (c >= 32 && c <= 126) ? c : ' ';
        }
        displayText += "\n";
    }
    SetDlgItemTextA(hwndDlg, IDC_CDU_GRID, displayText.c_str());
    InvalidateRect(grid, NULL, TRUE);
    UpdateWindow(grid);
    SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Grid updated with " + std::to_string(displayText.size()) + " chars").c_str());
    if (package[0][0][0] == 0) {
        std::string testText = "Test Grid:\n";
        for (int i = 0; i < 24; i++) {
            testText += std::string(14, 'X') + "\n";
        }
        SetDlgItemTextA(hwndDlg, IDC_CDU_GRID, testText.c_str());
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, "No data parsed, showing test grid");
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
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, "Socket disconnected");
    }
    SetDlgItemTextA(hwndDlg, IDC_CDU_BUTTON, (g_cduIndex == 0) ? "Switch to CDU1" : (g_cduIndex == 1) ? "Switch to CDU2" : "Switch to CDU0");
}

INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, "Dialog initialized");
        InvalidateRect(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT), NULL, TRUE);
        UpdateWindow(GetDlgItem(hwndDlg, IDC_DEBUG_TEXT));
        HWND debugText = GetDlgItem(hwndDlg, IDC_DEBUG_TEXT);
        HWND grid = GetDlgItem(hwndDlg, IDC_CDU_GRID);
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Debug text control: " + std::string(debugText ? "Created" : "Not found")).c_str());
        SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, ("Grid control: " + std::string(grid ? "Created" : "Not found")).c_str());
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
        // Create switch button
        CreateWindowA("BUTTON", "Switch to CDU1", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 500, 100, 30, hwndDlg, (HMENU)IDC_CDU_BUTTON, NULL, NULL);
        // Start receiving data
        SendCDUIndex(g_socket, g_cduIndex);
        SetTimer(hwndDlg, 1, 100, NULL); // Update every 100ms
        return TRUE;
    }
    case WM_USER + 1:
            UpdateCDUDisplay(hwndDlg, g_package);
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