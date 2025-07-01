#include "CDUClient.h"
#include "resource.h"
#include <sstream>

SOCKET g_socket = INVALID_SOCKET;
int g_cduIndex = 0;
int g_package[24][14][3] = { {{0}} };

bool InitializeWinsock() {
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

bool ConnectToServer(const char* ip, int port) {
    g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_socket == INVALID_SOCKET) return false;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serverAddr.sin_addr);

    if (connect(g_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(g_socket);
        return false;
    }
    return true;
}

bool ReceiveCDUData(SOCKET sock, int package[24][14][3]) {
    char buffer[4096];
    int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) return false;
    buffer[bytesReceived] = '\0';

    std::stringstream ss(buffer);
    std::string token;
    int i = 0, j = 0, k = 0;
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
    }
    return i == 24;
}

void SendCDUIndex(SOCKET sock, int cduIndex) {
    send(sock, (char*)&cduIndex, sizeof(int), 0);
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
        // Start receiving data
        SendCDUIndex(g_socket, g_cduIndex);
        SetTimer(hwndDlg, 1, 100, NULL); // Update every 100ms
        return TRUE;
    }
    case WM_TIMER:
        if (ReceiveCDUData(g_socket, g_package)) {
            UpdateCDUDisplay(hwndDlg, g_package);
        }
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CDU_BUTTON) {
            g_cduIndex = (g_cduIndex + 1) % 3;
            SendCDUIndex(g_socket, g_cduIndex);
            UpdateCDUDisplay(hwndDlg, g_package);
        }
        return TRUE;
    case WM_CLOSE:
        closesocket(g_socket);
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
    if (!ConnectToServer("192.168.1.5", 27016)) {
        MessageBoxA(NULL, "Failed to connect to server", "Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        return 1;
    }
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_CDU_DIALOG), NULL, DialogProc);
    return 0;
}