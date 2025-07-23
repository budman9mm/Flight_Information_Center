#include "CDUClient.h"
#include "resource.h"
#include <sstream>
#include <algorithm>
#include <vector>
#include <tuple>
#include <cctype>
#include <stdio.h>
#include <CommCtrl.h> // Add this include to define IPM_SETADDRESS

using ParsedMessage = std::tuple<char, char, int>; // Parsed message structure

//TCP Socket
SOCKET g_socket = INVALID_SOCKET; //TCP socket for CDU package
//UDP Socket variables
SOCKET g_DataSocket = INVALID_SOCKET; //TCP socket for variable data

int g_DataPort = 46818; // UDP port for receiving CDU data


int g_cduIndex = 0;  // Index of the CDU for the Switch CDU button
int g_package[24][14][3] = { {{0}} };
HANDLE g_thread[2]; // Thread handles for the CDU and TCPData processing
unsigned int g_threadId[2]; // Thread IDs for the CDU and TCPData processing

volatile bool g_running = false;
HFONT g_hFontGrid = NULL;
HFONT g_hFontDebug = NULL;
bool g_console = true; // Console debugging flag -- Turn on for console output
bool g_connected = false;

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

bool CDU_annunEXEC[3] = { {0} },
CDU_annunDSPY[3] = { {0} },
CDU_annunFAIL[3] = { {0} },
CDU_annunMSG[3] = { {0} },
CDU_annunOFST[3] = { {0} };//L/R/C CDU annunciators

//data variables for the misc flight data 
struct SimVars {
    int simZuluTime; //sc
	int altitude; //sc
	int heading; //sc
    int distanceToTOD; //pmdg
    int baroMB; //sc
    int IASMach; //sc
	bool simPause; //sc
} g_simVars;;


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

bool ConnectToServer(const char* ip, int port, HWND hwndDlg, SOCKET* pSocket) {
    *pSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*pSocket == INVALID_SOCKET) {
        if (g_console) printf_s("Socket creation failed: %d\n", WSAGetLastError());
        MessageBoxA(hwndDlg, ("Socket creation failed: " + std::to_string(WSAGetLastError())).c_str(), "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    u_long mode = 1;
    if (ioctlsocket(*pSocket, FIONBIO, &mode) != 0) {
        if (g_console) printf_s("Failed to set non-blocking mode: %d\n", WSAGetLastError());
        MessageBoxA(hwndDlg, ("Failed to set non-blocking mode: " + std::to_string(WSAGetLastError())).c_str(), "Error", MB_OK | MB_ICONERROR);
        closesocket(*pSocket);
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serverAddr.sin_addr);

    if (connect(*pSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            if (g_console) printf_s("Connection to %s:%d failed: %d\n", ip, port, error);
            MessageBoxA(hwndDlg, (std::string("Connection to ") + ip + ":" + std::to_string(port) + " failed: " + std::to_string(error)).c_str(), "Error", MB_OK | MB_ICONERROR);
            closesocket(*pSocket);
            return false;
        }
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(*pSocket, &writeSet);
        struct timeval timeout = { 2, 0 };
        if (select(0, NULL, &writeSet, NULL, &timeout) <= 0) {
            if (g_console) printf_s("Connection to %s:%d timed out\n", ip, port);
            MessageBoxA(hwndDlg, "Connection timed out", "Error", MB_OK | MB_ICONERROR);
            closesocket(*pSocket);
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

/*
bool ConnectToServerUDP(const char* ip, int port, HWND hwndDlg) {
    g_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_udpSocket == INVALID_SOCKET) {
        if (g_console) printf_s("UDP Socket creation failed: %d\n", WSAGetLastError());
        MessageBoxA(hwndDlg, ("UDP Socket creation failed: " + std::to_string(WSAGetLastError())).c_str(), "Error", MB_OK | MB_ICONERROR);
        return false;
    }
    // BIND to local port 46818
    sockaddr_in localAddr = {};
    localAddr.sin_family = AF_INET;
    //localAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all local interfaces
    localAddr.sin_port = htons(port); 

    if (bind(g_udpSocket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        MessageBoxA(hwndDlg, ("UDP bind failed: " + std::to_string(error)).c_str(), "Error", MB_OK | MB_ICONERROR);
        closesocket(g_udpSocket);
        return false;
    }
	u_long mode = 1; // Set non-blocking mode
    if (ioctlsocket(g_udpSocket, FIONBIO, &mode) != 0) {
        if (g_console) printf_s("Failed to set UDP non-blocking mode: %d\n", WSAGetLastError());
        MessageBoxA(hwndDlg, ("Failed to set UDP non-blocking mode: " + std::to_string(WSAGetLastError())).c_str(), "Error", MB_OK | MB_ICONERROR);
        closesocket(g_udpSocket);
        return false;
	}

    /*g_udpServerAddr.sin_family = AF_INET;
    g_udpServerAddr.sin_port = htons(port);

    inet_pton(AF_INET, ip, &g_udpServerAddr.sin_addr);
    if (connect(g_udpSocket, (sockaddr*)&g_udpServerAddr, sizeof(g_udpServerAddr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            if (g_console) printf_s("UDP connection to %s:%d failed: %d\n", ip, port, error);
            MessageBoxA(hwndDlg, (std::string("UDP connection to ") + ip + ":" + std::to_string(port) + " failed: " + std::to_string(error)).c_str(), "Error", MB_OK | MB_ICONERROR);
            closesocket(g_udpSocket);
            return false;
        }
    }
    if (g_console) printf_s("Connected to UDP server %s:%d\n", ip, port);
    MessageBoxA(hwndDlg, (std::string("Connected to UDP server ") + ip + ":" + std::to_string(port)).c_str(), "Info", MB_OK);
	PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Connected to UDP server " + std::string(ip) + ":" + std::to_string(g_udpPort) + "\r\n")));
    g_connected = true;
    SetDlgItemTextA(hwndDlg, IDC_DISCONNECT_BUTTON, "Disconnect");
	return true;
}
*/

int countDigits(int n) {  // Counts the number of digits in an integer for parsedMessage value
    if (n == 0) return 1;
    return static_cast<int>(std::log10(std::abs(n))) + 1;
}

int ProcessZulu(int zuluTime) {
    // Convert Zulu time in seconds to HHMMSS format
    int hours = zuluTime / 3600;
    int minutes = (zuluTime % 3600) / 60;
    int seconds = zuluTime % 60;
    return (hours * 10000) + (minutes * 100) + seconds; // Return as HHMMSS
}


// Parses all messages in the buffer of the form: <delim><code><value>
// Example: "#a1", "$b123", etc.
std::vector<ParsedMessage> ParseDelimitedMessages(const std::string& buffer, const std::string& delimiters = "#$") {
    std::vector<ParsedMessage> results;
    size_t i = 0;
    while (i < buffer.size()) {
        if (delimiters.find(buffer[i]) != std::string::npos) {
            char delim = buffer[i];
            if (i + 1 < buffer.size() && std::isalpha(buffer[i + 1])) {
                char code = buffer[i + 1];
                size_t valStart = i + 2;
                size_t valEnd = valStart;
                while (valEnd < buffer.size() && delimiters.find(buffer[valEnd]) == std::string::npos) {
                    valEnd++;
                }
                std::string valueStr = buffer.substr(valStart, valEnd - valStart);
                int value = 0;
                try {
                    value = std::stoi(valueStr);
                }
                catch (...) {
                    value > 99999; // or handle error as needed
                }
                results.emplace_back(delim, code, value);
                i = valEnd;
                continue;
            }
        }
        ++i;
    }
    return results;
}

DWORD WINAPI ReceiveThreadTCPCDU(LPVOID lpParam) {
    HWND hwndDlg = (HWND)lpParam;
    std::string buffer;
    char tempBuffer[4096];
    //CDU_annunEXEC[0] = 1; //test force the STATIC_LT3 on white
    while (g_running && g_socket != INVALID_SOCKET) {
        int bytesReceived = recv(g_socket, tempBuffer, sizeof(tempBuffer) - 1, 0);
        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                Sleep(100);
                continue;
            }
            if (g_console) printf_s("Receive failed ReceiveThreadTCPCDU: Error %d\n", error);
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Receive failed ReceiveThreadTCPCDU: Error " + std::to_string(error) + "\r\n")));
            break;
        }
        if (bytesReceived == 0) {
            if (g_console) printf_s("Connection closed by server\n");
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Connection closed by server\r\n")));
            break;
        }
        tempBuffer[bytesReceived] = '\0';
        buffer += tempBuffer;
        //if (g_console) printf_s("Received %d bytes, total buffered: %zu\n", bytesReceived, buffer.size());
        //PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Received " + std::to_string(bytesReceived) + " bytes, total buffered: " + std::to_string(buffer.size()) + "\r\n")));
        //if (g_console) printf_s("Raw data: %.*s\n", (int)std::min<size_t>(100, buffer.size()), buffer.c_str());
        //PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Raw data: " + buffer.substr(0, std::min<size_t>(100, buffer.size())) + "\r\n")));
        
        if (buffer.size() > 2 && buffer[0] == '#' ) {
			//PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("First two bytes: " + buffer.substr(0, 2) + "\r\n")));
            //PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("First 4 bytes: " + buffer.substr(0, 4) + "\r\n")));
			
            switch (buffer[1]) {
               
						//CDU data
                    case 'X': //CDU data
                        //if (g_console) printf_s("CDU data received, size: %zu bytes\n", buffer.size());
                        //PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("CDU data received, size: " + std::to_string(buffer.size()) + " bytes\r\n")));
                        if (buffer.size() < 1010) {
                            if (g_console) printf_s("Insufficient data for text: %zu bytes\n", buffer.size());
                            //PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Insufficient data for text: " + std::to_string(buffer.size()) + " bytes\r\n")));
                            break;
                        }
                        else if (buffer.size() == 1010) {
                            std::string textData = buffer.substr(2, 1008);
                            //printf_s("%s\n", textData.c_str());
                            int idx = 0;
                            for (int h = 0; h < 3; h++) {
                                for (int i = 0; i < 14; ++i) {
                                    for (int j = 0; j < 24; ++j) {
                                        g_package[j][i][h] = (char)textData[idx];
                                        if (g_console) {
                                            //printf_s("%d", g_package[j][i][h]);
                                        }
                                        idx++;
                                    }
                                    //if (g_console) printf_s("\n");
                                }
                            }
                            //if (g_console) printf_s("CDU data processed, updating display...\n");
                            //PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("CDU data processed, updating display...\r\n")));
                            UpdateCDUDisplay(hwndDlg, g_package);
                            buffer.clear(); // Clear the buffer after processing
                            tempBuffer[0] = '\0'; // Reset tempBuffer to avoid processing old data
                        }
						else break; // If the size is not 1010, ignore the data
                        break;
                    default:
						buffer.clear(); // Clear the buffer after processing
                        if (g_console) printf_s("Unknown command received: %c\n", buffer[1]);
                        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Unknown command received: " + std::string(1, buffer[1]) + "\r\n")));
						break;
				} //end switch
				buffer.erase(0, 2); // Remove the processed command from the buffer
        
        } else {
			buffer.clear(); // Clear the buffer if it doesn't start with '#'
			tempBuffer[0] = '\0'; // Reset tempBuffer to avoid processing old data
	    }
    }
    return 0;
}
//Changing UDP to TCP on its own port 46818, for Data
DWORD WINAPI ReceiveThreadTCPData(LPVOID lpParam) {
    HWND hwndDlg = (HWND)lpParam;
    std::string buffer;
    char tempBuffer[4096];
	
    while (g_running && g_DataSocket != INVALID_SOCKET) {
        int bytesReceived = recv(g_DataSocket, tempBuffer, sizeof(tempBuffer) - 1, 0);
        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                Sleep(100);
                continue;
            }
            if (g_console) printf_s("Receive failed ReceiveThreadTCPData: Error %d\n", error);
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Receive failed ReceiveThreadTCPData: Error " + std::to_string(error) + "\r\n")));
            break;
        }
        if (bytesReceived == 0) {
            if (g_console) printf_s("Connection closed by server\n");
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Connection closed by server\r\n")));
            break;
        }
        // Ensure the buffer is null-terminated
        if (bytesReceived < sizeof(tempBuffer) - 1) {
            tempBuffer[bytesReceived] = '\0'; // Null-terminate the received data
        }
        else {
            tempBuffer[sizeof(tempBuffer) - 1] = '\0'; // Ensure null-termination
        }

        if (g_console) printf_s("TCPData Received %d bytes: %s\n", bytesReceived, buffer.c_str());
		// append the received data to the buffer
        buffer.append(tempBuffer, bytesReceived);
        // Parse the received data into messages
        auto messages = ParseDelimitedMessages(buffer);
        size_t lastParsedPos = 0;
        for (const auto& msg : messages) {
            char delim, code;
            int value;
            std::tie(delim, code, value) = msg;
            
            // Find the position of this message in the buffer
            size_t pos = buffer.find(delim, lastParsedPos);

            if (pos != std::string::npos) {
				int digits = countDigits(value);
				if (g_console) printf_s("countDigits: %d\n", digits);
                lastParsedPos = pos + 2 + digits; // delim + code + value
            }
			//Process the parsed message here
            if (delim == '$') {
                switch (code) {
                case 'P': // SC Sim Paused?
                {
                    g_simVars.simPause = (value != 0);
                    if (g_console) printf_s("Sim Pause: %s\n", g_simVars.simPause ? "Paused" : "Running");
                    PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Sim Pause: " + std::string(g_simVars.simPause ? "Paused" : "Running") + "\r\n")));
                    HWND hPause = GetDlgItem(hwndDlg, IDC_STATIC_PAUSE_LT4);
                    if (hPause) {
                        std::string pauseText = g_simVars.simPause ? "Paused" : "Running";
                        SetDlgItemTextA(hwndDlg, IDC_STATIC_PAUSE_LT4, pauseText.c_str());
                        InvalidateRect(hPause, NULL, TRUE); // Mark the control for redraw
                        UpdateWindow(hPause);               // Force immediate redraw
                    }
                    break;
                }
                }//end switch
            }
            if (delim == '#') {
				HWND hExec = GetDlgItem(hwndDlg, IDC_STATIC_EXEC_LT3);
                switch (code) {
                    case 'a':  // EXEC annunciator CDU0
                        CDU_annunEXEC[0] = value;
						hExec = GetDlgItem(hwndDlg, IDC_STATIC_EXEC_LT3);
                        if (hExec) {
                            InvalidateRect(hExec, NULL, TRUE); // Mark the control for redraw
                            UpdateWindow(hExec);               // Force immediate redraw
                        }
                        break;
                    case 'b':  // DSPY annunciator CDU0
                        CDU_annunDSPY[0] = (value);
						hExec = GetDlgItem(hwndDlg, IDC_STATIC_DSPY_LT4);
                        if (hExec) {
                            InvalidateRect(hExec, NULL, TRUE); // Mark the control for redraw
                            UpdateWindow(hExec);               // Force immediate redraw
						}
                        break;
                    case 'c':  // FAIL annunciator CDU0
                        CDU_annunFAIL[0] = (value);
						hExec = GetDlgItem(hwndDlg, IDC_STATIC_FAIL_LT5);
                        if (hExec) {
                            InvalidateRect(hExec, NULL, TRUE); // Mark the control for redraw
							UpdateWindow(hExec);               // Force immediate redraw
                            }
                        break;
                    case 'd':  // MSG annunciator CDU0
                        CDU_annunMSG[0] = (value);
						hExec = GetDlgItem(hwndDlg, IDC_STATIC_MSG_LT6);
                        if (hExec) {
                            InvalidateRect(hExec, NULL, TRUE); // Mark the control for redraw
                            UpdateWindow(hExec);               // Force immediate redraw
						}
                        break;
                    case 'e':  // OFST annunciator CDU0
                        CDU_annunOFST[0] = (value);
						hExec = GetDlgItem(hwndDlg, IDC_STATIC_OFST_LT7);
                        if (hExec) {
                            InvalidateRect(hExec, NULL, TRUE); // Mark the control for redraw
                            UpdateWindow(hExec);               // Force immediate redraw
                        }
                        break;
                    case 'f': // EXEC annunciator CDU1
                        CDU_annunEXEC[1] = (value);
                        break;
                    case 'g': // DSPY annunciator CDU1
                        CDU_annunDSPY[1] = (value);
                        break;
                    case 'h': // FAIL annunciator CDU1
                        CDU_annunFAIL[1] = (value);
                        break;
                    case 'i': // MSG annunciator CDU1
                        CDU_annunMSG[1] = (value);
                        break;
                    case 'j': // OFST annunciator CDU1
                        CDU_annunOFST[1] = (value);
                        break;
                    case 'k': // EXEC annunciator CDU2
                        CDU_annunEXEC[2] = (value);
                        break;
                    case 'l': // DSPY annunciator CDU2
                        CDU_annunDSPY[2] = (value);
                        break;
                    case 'm': // FAIL annunciator CDU2
                        CDU_annunFAIL[2] = (value);
                        break;
                    case 'n': // MSG annunciator CDU2
                        CDU_annunMSG[2] = (value);
                        break;
                    case 'o': // OFST annunciator CDU2
                        CDU_annunOFST[2] = (value);
                        break;
                    case 'p': // Sim Zulu Time - sent only once
                    {
                        g_simVars.simZuluTime = ProcessZulu(value);

                        if (g_console) printf_s("Zulu Time: %d\n", g_simVars.simZuluTime);
                        //Format the returned value from HHMMSS to HH:MM and drop the seconds
                        std::string zuluTimeText = std::to_string(g_simVars.simZuluTime / 10000) + ":" + std::to_string((g_simVars.simZuluTime % 10000) / 100);
                        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Zulu Time: " + zuluTimeText + "\r\n")));
                        //update IDC_STATIC_SIMTIME
                        {
                            HWND hSimTime = GetDlgItem(hwndDlg, IDC_STATIC_SIMTIME);
                            if (hSimTime) {
                                std::string simTimeText = "Zulu Time: " + zuluTimeText;
                                SetDlgItemTextA(hwndDlg, IDC_STATIC_SIMTIME, simTimeText.c_str());
                                InvalidateRect(hSimTime, NULL, TRUE); // Mark the control for redraw
                                UpdateWindow(hSimTime);               // Force immediate redraw
                            }
                        }
                        break;
                    }
                    case 'q': // Sim Altitude
						g_simVars.altitude = value;
						if (g_console) printf_s("Altitude: %d\n", g_simVars.altitude);
						PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Altitude: " + std::to_string(g_simVars.altitude) + "\r\n")));
						//update IDC_STATIC_ALTITUDE
                        {
                            HWND hAltitude = GetDlgItem(hwndDlg, IDC_STATIC_ALTITUDE);
                            if (hAltitude) {
                                std::string altitudeText = "Altitude: " + std::to_string(g_simVars.altitude);
                                SetDlgItemTextA(hwndDlg, IDC_STATIC_ALTITUDE, altitudeText.c_str());
                                InvalidateRect(hAltitude, NULL, TRUE); // Mark the control for redraw
                                UpdateWindow(hAltitude);               // Force immediate redraw
                            }
                        }
                        break;
					case 'r': // Sim Heading
                        g_simVars.heading = value;
                        if (g_console) printf_s("Heading: %d\n", g_simVars.heading);
                        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Heading: " + std::to_string(g_simVars.heading) + "\r\n")));
                        //update IDC_STATIC_HEADING
                        {
                            HWND hHeading = GetDlgItem(hwndDlg, IDC_STATIC_HEADING);
                            if (!hHeading) {
                                printf("IDC_STATIC_HEADING not found!\n");
                            }
                            if (hHeading) {
                                std::string headingText = "Heading: " + std::to_string(g_simVars.heading);
                                SetDlgItemTextA(hwndDlg, IDC_STATIC_HEADING, headingText.c_str());
                                InvalidateRect(hHeading, NULL, TRUE); // Mark the control for redraw
                                UpdateWindow(hHeading);               // Force immediate redraw
                            }
                        }
						break;
                    case 's': // Distance to TOD
                        g_simVars.distanceToTOD = value;
                        if (g_console) printf_s("Distance TOD: %d\n", g_simVars.distanceToTOD);
                        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Distance TOD: " + std::to_string(g_simVars.distanceToTOD) + "\r\n")));
                        //update IDC_STATIC_DISTANCE_TOD
                        {
                            HWND hDistanceTOD = GetDlgItem(hwndDlg, IDC_STATIC_DIST_TOD);
                            if (hDistanceTOD) {
                                std::string distanceText = "Distance TOD: " + std::to_string(g_simVars.distanceToTOD);
                                SetDlgItemTextA(hwndDlg, IDC_STATIC_DIST_TOD, distanceText.c_str());
                                InvalidateRect(hDistanceTOD, NULL, TRUE); // Mark the control for redraw
                                UpdateWindow(hDistanceTOD);               // Force immediate redraw
                            }
						}
                        break;
                    case 't': // Barometric Pressure in MB
						g_simVars.baroMB = value;
                        if (g_console) printf_s("Baro: %d MB\n", g_simVars.baroMB);
                        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Baro: " + std::to_string(g_simVars.baroMB) + " MB\r\n")));
                        //update IDC_STATIC_BARO
                        {
                            HWND hBaro = GetDlgItem(hwndDlg, IDC_STATIC_BARO);
                            if (hBaro) {
                                std::string baroText = "Baro: " + std::to_string(g_simVars.baroMB) + " MB";
                                SetDlgItemTextA(hwndDlg, IDC_STATIC_BARO, baroText.c_str());
                                InvalidateRect(hBaro, NULL, TRUE); // Mark the control for redraw
                                UpdateWindow(hBaro);               // Force immediate redraw
                            }
                        }
                        break;
					case 'u': // IAS/Mach
                        g_simVars.IASMach = value;
                        if (g_console) printf_s("IAS/Mach: %d\n", g_simVars.IASMach);
                        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("IAS/Mach: " + std::to_string(g_simVars.IASMach) + "\r\n")));
                        //update IDC_STATIC_IASMACH
                        {
                            HWND hIASMach = GetDlgItem(hwndDlg, IDC_STATIC_IASMACH);
                            if (hIASMach) {
                                std::string iasMachText = "IAS/Mach: " + std::to_string(g_simVars.IASMach);
                                SetDlgItemTextA(hwndDlg, IDC_STATIC_IASMACH, iasMachText.c_str());
                                InvalidateRect(hIASMach, NULL, TRUE); // Mark the control for redraw
                                UpdateWindow(hIASMach);               // Force immediate redraw
                            }
                        }
                        break;
                    
                    default:
                      
						break;
				}
            }
            if (g_console) printf("Delimiter: %c, Code: %c, Value: %d\n", delim, code, value);
            PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Delimiter: " + std::string(1, delim) + ", Code: " + std::string(1, code) + ", Value: " + std::to_string(value) + "\r\n")));
        }
        // Remove processed data from buffer
        if (lastParsedPos > 0 && lastParsedPos <= buffer.size()) {
            buffer.erase(0, lastParsedPos);
        }
       
        PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("TCPData Received " + std::to_string(bytesReceived) + " bytes: " + std::string(buffer) + "\r\n")));
        buffer[bytesReceived] = '\0';
        
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
    
    //PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Sub: [" + std::to_string(g_package[2][1][0]) + "]\r\n")));
    SetDlgItemTextA(hwndDlg, IDC_CDU_GRID, displayText.c_str());
    InvalidateRect(grid, NULL, TRUE);
    UpdateWindow(grid);
    //PostMessage(hwndDlg, WM_USER + 2, 0, (LPARAM)(new std::string("Grid updated with " + std::to_string(displayText.size()) + " chars\r\n")));
    
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
        //SetDlgItemTextA(hwndDlg, IDC_CDU_GRID, "Initial Grid\r\n");
        InvalidateRect(hwndDlg, NULL, TRUE);
        UpdateWindow(hwndDlg);
        if (ConnectToServer("192.168.1.5", 27016, hwndDlg, &g_socket)) { //CDU
            SendCDUIndex(g_socket, g_cduIndex, hwndDlg);
            g_running = true;
            //g_thread[0] = CreateThread(NULL, 0, &ReceiveThreadTCP, hwndDlg, 0, &g_threadId[0]);
			g_thread[0] = CreateThread(NULL, 0, ReceiveThreadTCPCDU, hwndDlg, 0, NULL);
            if (!g_thread[0]) {
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
        if (ConnectToServer("192.168.1.5", 46818, hwndDlg, &g_DataSocket)) {  //Data
			g_thread[1] = CreateThread(NULL, 0, ReceiveThreadTCPData, hwndDlg, 0, NULL);
            if (!g_thread[1]) {
                if (g_console) printf_s("Failed to create UDP receive thread\n");
                LogDebugMessage(hwndDlg, "Failed to create UDP receive thread\r\n");
			}
            // Optionally send an initial command or data to the TCP server
			send(g_DataSocket, "CDU Client Init", 15, 0);
		    
			//sendto(g_udpSocket, "CDU Client Init", 15, 0, (sockaddr*)&g_udpServerAddr, sizeof(g_udpServerAddr));
			// SendCDUControl(g_udpSocket, 0, 0, hwndDlg, "Initial UDP Command");
			LogDebugMessage(hwndDlg, "Connected to UDP server\r\n");
        }
        HWND hIpAddr = GetDlgItem(hwndDlg, IDC_IPADDRESS1);
        if (hIpAddr) {
            SendMessage(hIpAddr, IPM_SETADDRESS, 0, 
                MAKEIPADDRESS(192,168,1,5));
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
                      
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (lpdis->CtlID == IDC_STATIC_EXEC_LT3) {
            // Example: choose color based on a condition
            COLORREF fillColor = RGB(0, 0, 0); // Default background color black
            if (CDU_annunEXEC[0] == 1) {
                fillColor = RGB(255, 235, 59); // Yellow if condition met
            }
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
        if (lpdis->CtlID == IDC_STATIC_DSPY_LT4) {
            // Fill background as before
            COLORREF fillColor = RGB(0, 0, 0);
            if (CDU_annunDSPY[0] == 1) {
                fillColor = RGB(255, 0, 0); // Example: red if active
            }
            HBRUSH hBrush = CreateSolidBrush(fillColor);
            FillRect(lpdis->hDC, &lpdis->rcItem, hBrush);
            DeleteObject(hBrush);

            // Create a vertical font (rotated 90 degrees)
            LOGFONT lf = {0};
            lf.lfHeight = 18; // Font height
            lf.lfEscapement = 900; // 90 degrees
            lf.lfOrientation = 900;
            lf.lfWeight = FW_BOLD;
            strcpy_s(lf.lfFaceName, "Cascadia Code"); // Or your preferred font

            HFONT hFont = CreateFontIndirect(&lf);
            HFONT hOldFont = (HFONT)SelectObject(lpdis->hDC, hFont);

            SetBkMode(lpdis->hDC, TRANSPARENT);
            SetTextColor(lpdis->hDC, RGB(255, 255, 255)); // White text

            // The text to display
            const char* text = "DSPY";
            // Calculate position (centered)
            SIZE textSize;
            GetTextExtentPoint32A(lpdis->hDC, text, (int)strlen(text), &textSize);
            int x = lpdis->rcItem.left + (lpdis->rcItem.right - lpdis->rcItem.left - textSize.cy) / 2;
            int y = lpdis->rcItem.top + (lpdis->rcItem.bottom - lpdis->rcItem.top + textSize.cx) / 2;

            TextOutA(lpdis->hDC, x, y, text, (int)strlen(text));

            SelectObject(lpdis->hDC, hOldFont);
            DeleteObject(hFont);

            return TRUE;
		}
        if (lpdis->CtlID == IDC_STATIC_FAIL_LT5) {
            COLORREF fillColor = RGB(0, 0, 0); // Default background color black
            if (CDU_annunFAIL[0] == 1) {
                fillColor = RGB(255, 0, 0); // Red if condition met
            }
			HBRUSH hBrush = CreateSolidBrush(fillColor);
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrush);
			DeleteObject(hBrush);

            // Create a vertical font (rotated 90 degrees)
            LOGFONT lf = { 0 };
            lf.lfHeight = 18; // Font height
            lf.lfEscapement = 900; // 90 degrees
            lf.lfOrientation = 900;
            lf.lfWeight = FW_BOLD;
            strcpy_s(lf.lfFaceName, "Cascadia Code"); // Or your preferred font

			HFONT hFont = CreateFontIndirect(&lf);
			HFONT hOldFont = (HFONT)SelectObject(lpdis->hDC, hFont);

			SetBkMode(lpdis->hDC, TRANSPARENT);
			SetTextColor(lpdis->hDC, RGB(255, 255, 255)); // White text
			// The text to display
			const char* text = "FAIL";
			// Calculate position (centered)
			SIZE textSize;
			GetTextExtentPoint32A(lpdis->hDC, text, (int)strlen(text), &textSize);
			int x = lpdis->rcItem.left + (lpdis->rcItem.right - lpdis->rcItem.left - textSize.cy) / 2;
			int y = lpdis->rcItem.top + (lpdis->rcItem.bottom - lpdis->rcItem.top + textSize.cx) / 2;
			TextOutA(lpdis->hDC, x, y, text, (int)strlen(text));
			SelectObject(lpdis->hDC, hOldFont);
            DeleteObject(hFont);

            return TRUE;
		}
        if (lpdis->CtlID == IDC_STATIC_MSG_LT6) {
            COLORREF fillColor = RGB(0, 0, 0); // Default background color black
            if (CDU_annunMSG[0] == 1) {
                fillColor = RGB(255, 0, 0); // Red if condition met
            }
            HBRUSH hBrush = CreateSolidBrush(fillColor);
            FillRect(lpdis->hDC, &lpdis->rcItem, hBrush);
            DeleteObject(hBrush);

            // Create a vertical font (rotated 90 degrees)
            LOGFONT lf = { 0 };
            lf.lfHeight = 18; // Font height
            lf.lfEscapement = 900; // 90 degrees
            lf.lfOrientation = 900;
            lf.lfWeight = FW_BOLD;
            strcpy_s(lf.lfFaceName, "Cascadia Code"); // Or your preferred font

            HFONT hFont = CreateFontIndirect(&lf);
            HFONT hOldFont = (HFONT)SelectObject(lpdis->hDC, hFont);

            SetBkMode(lpdis->hDC, TRANSPARENT);
            SetTextColor(lpdis->hDC, RGB(255, 255, 255)); // White text
            // The text to display
            const char* text = "MSG";
            // Calculate position (centered)
            SIZE textSize;
            GetTextExtentPoint32A(lpdis->hDC, text, (int)strlen(text), &textSize);
            int x = lpdis->rcItem.left + (lpdis->rcItem.right - lpdis->rcItem.left - textSize.cy) / 2;
            int y = lpdis->rcItem.top + (lpdis->rcItem.bottom - lpdis->rcItem.top + textSize.cx) / 2;
            TextOutA(lpdis->hDC, x, y, text, (int)strlen(text));
            SelectObject(lpdis->hDC, hOldFont);
            DeleteObject(hFont);
            return TRUE;
		}
        if (lpdis->CtlID == IDC_STATIC_OFST_LT7) {
            COLORREF fillColor = RGB(0, 0, 0); // Default background color black
            if (CDU_annunOFST[0] == 1) {
                fillColor = RGB(255, 0, 0); // Red if condition met
            }
            HBRUSH hBrush = CreateSolidBrush(fillColor);
            FillRect(lpdis->hDC, &lpdis->rcItem, hBrush);
            DeleteObject(hBrush);

            // Create a vertical font (rotated 90 degrees)
            LOGFONT lf = { 0 };
            lf.lfHeight = 18; // Font height
            lf.lfEscapement = 900; // 90 degrees
            lf.lfOrientation = 900;
            lf.lfWeight = FW_BOLD;
            strcpy_s(lf.lfFaceName, "Cascadia Code"); // Or your preferred font

            HFONT hFont = CreateFontIndirect(&lf);
            HFONT hOldFont = (HFONT)SelectObject(lpdis->hDC, hFont);

            SetBkMode(lpdis->hDC, TRANSPARENT);
            SetTextColor(lpdis->hDC, RGB(255, 255, 255)); // White text
            // The text to display
            const char* text = "OFST";
            // Calculate position (centered)
            SIZE textSize;
            GetTextExtentPoint32A(lpdis->hDC, text, (int)strlen(text), &textSize);
            int x = lpdis->rcItem.left + (lpdis->rcItem.right - lpdis->rcItem.left - textSize.cy) / 2;
            int y = lpdis->rcItem.top + (lpdis->rcItem.bottom - lpdis->rcItem.top + textSize.cx) / 2;
            TextOutA(lpdis->hDC, x, y, text, (int)strlen(text));
            SelectObject(lpdis->hDC, hOldFont);
            DeleteObject(hFont);
            
            return TRUE;
        }
        if (lpdis->CtlID == IDC_STATIC_PAUSE_LT4) {
            // Choose color based on pause state
            COLORREF fillColor = g_simVars.simPause ? RGB(255, 0, 0) : RGB(0, 180, 0); // Red for paused, green for running
            HBRUSH hBrush = CreateSolidBrush(fillColor);
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
            HGDIOBJ oldPen = SelectObject(lpdis->hDC, hPen);
            HGDIOBJ oldBrush = SelectObject(lpdis->hDC, hBrush);

            // Draw rounded rectangle
            RoundRect(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top,
                lpdis->rcItem.right, lpdis->rcItem.bottom, 12, 12);

            // Set up font (12pt)
            LOGFONT lf = { 0 };
            lf.lfHeight = -13; // 12pt at 96 DPI
            lf.lfWeight = FW_BOLD;
            strcpy_s(lf.lfFaceName, "Cascadia Code");
            HFONT hFont = CreateFontIndirect(&lf);
            HFONT hOldFont = (HFONT)SelectObject(lpdis->hDC, hFont);

            // Set text color and background
            SetBkMode(lpdis->hDC, TRANSPARENT);
            SetTextColor(lpdis->hDC, RGB(255, 255, 255));

            // Text to display
            const char* text = g_simVars.simPause ? "PAUSED" : "RUNNING";

            // Center the text
            SIZE textSize;
            GetTextExtentPoint32A(lpdis->hDC, text, (int)strlen(text), &textSize);
            int x = lpdis->rcItem.left + (lpdis->rcItem.right - lpdis->rcItem.left - textSize.cx) / 2;
            int y = lpdis->rcItem.top + (lpdis->rcItem.bottom - lpdis->rcItem.top - textSize.cy) / 2;
            TextOutA(lpdis->hDC, x, y, text, (int)strlen(text));

            // Cleanup
           
            SelectObject(lpdis->hDC, oldBrush);
            SelectObject(lpdis->hDC, oldPen);
            DeleteObject(hFont);
            DeleteObject(hBrush);
            DeleteObject(hPen);

            return TRUE;

        }
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
        if (LOWORD(wParam) == IDC_REFRESH_BUTTON2) {
            //Send TCP command "R" to server
			send(g_DataSocket, "R\n", 1, 0);
        }
        else if (LOWORD(wParam) == IDC_DISCONNECT_BUTTON) {
            if (g_connected) {
                g_running = false;
                DisconnectSocket(hwndDlg);
            }
            else {
                HWND hIpAddr = GetDlgItem(hwndDlg, IDC_IPADDRESS1);
                DWORD ip;
                char ipStr[16];
                if (hIpAddr && SendMessage(hIpAddr, IPM_GETADDRESS, 0, (LPARAM)&ip) == 4) {
                    sprintf_s(ipStr, "%u.%u.%u.%u",
                        FIRST_IPADDRESS(ip),
                        SECOND_IPADDRESS(ip),
                        THIRD_IPADDRESS(ip),
                        FOURTH_IPADDRESS(ip));
                } else {
                    strcpy_s(ipStr, "192.168.1.5"); // fallback
                }
                if (ConnectToServer(ipStr, 27016, hwndDlg, &g_socket)) {
                    SendCDUIndex(g_socket, g_cduIndex, hwndDlg);
                    g_running = true;
                    g_thread[0] = CreateThread(NULL, 0, ReceiveThreadTCPCDU, hwndDlg, 0, NULL);
                    if (!g_thread[0]) {
                        if (g_console) printf_s("Failed to create receive thread\n");
                        LogDebugMessage(hwndDlg, "Failed to create receive thread\r\n");
                    }
                } else {
                    MessageBoxA(hwndDlg, "Connection failed.", "Error", MB_OK | MB_ICONERROR);
                }

            }
        }
        else if (LOWORD(wParam) == IDC_EXIT_BUTTON) {
            g_running = false;
            DisconnectSocket(hwndDlg);
            if (g_thread[0]) {
                WaitForSingleObject(g_thread[0], 1000);
                CloseHandle(g_thread[0]);
                g_thread[0] = NULL;
            }
            if (g_thread[1]) {
                WaitForSingleObject(g_thread[1], 1000);
                CloseHandle(g_thread[1]);
                g_thread[1] = NULL;
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
        if (g_thread[0]) {
            WaitForSingleObject(g_thread[0], 1000);
            CloseHandle(g_thread[0]);
            g_thread[0] = NULL;
        }
        if (g_thread[1]) {
            WaitForSingleObject(g_thread[1], 1000);
            CloseHandle(g_thread[1]);
            g_thread[1] = NULL;
        }
        shutdown(g_socket, SD_BOTH);
        closesocket(g_socket);
        shutdown(g_DataSocket, SD_BOTH);
        closesocket(g_DataSocket);
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
    //MessageBoxA(NULL, "Starting client", "Debug", MB_OK);
    int dlgResult = DialogBox(hInstance, MAKEINTRESOURCE(IDD_CDU_DIALOG), NULL, DialogProc);
    if (dlgResult == -1) {
        MessageBoxA(NULL, "DialogBox failed!", "Error", MB_OK | MB_ICONERROR);
    }
    
    return 0;
}