#include "CDUClient.h"
#include "resource.h"
#include <sstream>
#include <algorithm>

SOCKET g_socket = INVALID_SOCKET;
int g_cduIndex = 0;
int g_package[24][14][3] = { {{0}} };

bool InitializeWinsock() {
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

bool ConnectToServer(const char* ip, int port, HWND hwndDlg) {
    g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_socket == INVALID_SOCKET) {
        LogDebugMessage(hwndDlg, "Socket creation failed: " + std::to_string(WSAGetLastError()));
        return false;
    }

    // Set socket to non-blocking
    u_long mode = 1;
    if (ioctlsocket(g_socket, FIONBIO, &mode) != 0) {
        LogDebugMessage(hwndDlg, "Failed to set non-blocking mode: " + std::to_string(WSAGetLastError()));
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
            LogDebugMessage(hwndDlg, "Connection to " + std::string(ip) + ":" + std::to_string(port) + " failed: " + std::to_string(error));
            closesocket(g_socket);
            return false;
        }
    }
    LogDebugMessage(hwndDlg, "Connected to " + std::string(ip) + ":" + std::to_string(port));
    return true;
}

bool ReceiveCDUData(SOCKET sock, int package[24][14][3], HWND hwndDlg) {
    char buffer[4096];
    int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            return false; // No data available yet
        }
        LogDebugMessage(hwndDlg, "Receive failed: Error " + std::to_string(error));
        return false;
    }
    if (bytesReceived == 0) {
        LogDebugMessage(hwndDlg, "Connection closed by server");
        return false;
    }
    if (bytesReceived != 1012) {
        LogDebugMessage(hwndDlg, "Unexpected data size: " + std::to_string(bytesReceived) + " bytes (expected 1012)");
        return false;
    }
    buffer[bytesReceived] = '\0';
    std::string logMessage = "Received " + std::to_string(bytesReceived) + " bytes: ";
    logMessage += std::string(buffer, std::min<size_t>(100, bytesReceived));
    LogDebugMessage(hwndDlg, logMessage);

    // Skip #X prefix and check \n
    char* dataStart = buffer;
    if (bytesReceived >= 2 && buffer[0] == '#' && (buffer[1] >= '0' && buffer[1] <= '2')) {
        dataStart += 2;
        LogDebugMessage(hwndDlg, "Skipped prefix: #" + std::string(1, buffer[1]));
    }
    else {
        LogDebugMessage(hwndDlg, "No valid #X prefix found");
        return false;
    }

    // Check and skip trailing \n
    if (buffer[bytesReceived - 1] != '\n') {
        LogDebugMessage(hwndDlg, "No trailing newline found");
        return false;
    }
    bytesReceived--; // Exclude \n

    std::stringstream ss(dataStart);
    std::string token;
    int i = 0, j = 0;
    while (std::getline(ss, token, '|') && i < 24) {
        if (token.empty()) continue;
        int charVal, color, attr;
        if (sscanf_s(token.c_str(), "%d,%d,%d", &charVal, &color, &attr) == 3) {
            package[i][j][0] = charVal;
            package[i][j][1] = color;
            package[i][j][2] = attr;
            j++;
            if (j >= 14) {
                j = 0;
                i++;
            }
        }
        else {
            LogDebugMessage(hwndDlg, "Invalid token: " + token);
            return false;
        }
    }
    if (i != 24 || j != 0) {
        LogDebugMessage(hwndDlg, "Incomplete data: parsed " + std::to_string(i) + " rows, " + std::to_string(j) + " cols");
        return false;
    }
    LogDebugMessage(hwndDlg, "Parsed " + std::to_string(i * 14) + " elements successfully");
    return true;
}

void SendCDUIndex(SOCKET sock, int cduIndex, HWND hwndDlg) {
    int bytesSent = send(sock, (char*)&cduIndex, sizeof(int), 0);
    if (bytesSent == SOCKET_ERROR) {
        LogDebugMessage(hwndDlg, "Failed to send CDU index: " + std::to_string(WSAGetLastError()));
    }
    else if (bytesSent == sizeof(int)) {
        LogDebugMessage(hwndDlg, "Sent CDU index: " + std::to_string(cduIndex));
    }
}

void UpdateCDUDisplay(HWND hwndDlg, const int package[24][14][3]) {
    for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 14; j++) {
            int id = IDC_CDU_GRID + i * 14 + j;
            char text[2] = { (char)package[i][j][0], '\0' };
            SetDlgItemTextA(hwndDlg, id, text);
        }
    }
    SetDlgItemTextA(hwndDlg, IDC_CDU_BUTTON, (g_cduIndex == 0) ? "Switch to CDU1" : (g_cduIndex == 1) ? "Switch to CDU2" : "Switch to CDU0");
}

void LogDebugMessage(HWND hwndDlg, const std::string& message) {
    static std::string log;
    log = message + "\n" + log;
    if (log.size() > 1000) log = log.substr(0, 1000);
    SetDlgItemTextA(hwndDlg, IDC_DEBUG_TEXT, log.c_str());
}

void DisconnectSocket(HWND hwndDlg) {
    if (g_socket != INVALID_SOCKET) {
        shutdown(g_socket, SD_BOTH);
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        LogDebugMessage(hwndDlg, "Socket disconnected");
    }
}

INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        // Create 24x14 grid of static text controls
        for (int i = 0; i < 24; i++) {
            for (int j = 0; j < 14; j++) {
                CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_CENTER,
                    20 + j * 20, 20 + i * 20, 20, 20, hwndDlg, (HMENU)(IDC_CDU_GRID + i * 14 + j), NULL, NULL);
            }
        }
        // Create switch button
        CreateWindowA("BUTTON", "Switch to CDU1", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 500, 100, 30, hwndDlg, (HMENU)IDC_CDU_BUTTON, NULL, NULL);
        // Create disconnect button
        CreateWindowA("BUTTON", "Disconnect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            130, 500, 100, 30, hwndDlg, (HMENU)IDC_DISCONNECT_BUTTON, NULL, NULL);
        // Create debug text area
        CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
            20, 540, 300, 100, hwndDlg, (HMENU)IDC_DEBUG_TEXT, NULL, NULL);
        // Start receiving data
        ConnectToServer("192.168.1.5", 27016, hwndDlg);
        SendCDUIndex(g_socket, g_cduIndex, hwndDlg);
        SetTimer(hwndDlg, 1, 100, NULL);
        return TRUE;
    }
    case WM_TIMER:
        if (g_socket != INVALID_SOCKET && ReceiveCDUData(g_socket, g_package, hwndDlg)) {
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
            DisconnectSocket(hwndDlg);
            KillTimer(hwndDlg, 1); // Stop updates
        }
        return TRUE;
    case WM_CLOSE:
        DisconnectSocket(hwndDlg);
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
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_CDU_DIALOG), NULL, DialogProc);
    return 0;
}