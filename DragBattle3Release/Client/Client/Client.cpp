#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <conio.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT "8080"
#define DEFAULT_BUFLEN 512
#define TRACK_LENGTH 40  
#define MAX_DISTANCE 400 

void cls() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD count;
    COORD homeCoords = { 0, 0 };

    if (hConsole == INVALID_HANDLE_VALUE) return;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;

    DWORD cellCount = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(hConsole, ' ', cellCount, homeCoords, &count);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, cellCount, homeCoords, &count);
    SetConsoleCursorPosition(hConsole, homeCoords);
}

void drawTrack(float distance) {
    cls();
    std::cout << "Your progress:\n|";
    int pos = static_cast<int>(distance * TRACK_LENGTH / MAX_DISTANCE);
    for (int i = 0; i < TRACK_LENGTH; i++) {
        if (i == pos) {
            std::cout << "<O>";
        }
        else {
            std::cout << ".";
        }
    }
    std::cout << "|\n";
}

void countdown() {
    cls();
    for (int i = 3; i > 0; i--) {
        std::cout << "Race starts in: " << i << "...\n";
        Sleep(1000);
        cls();
    }
    std::cout << "START!\n";
    Sleep(1000);
    cls();
}

int main() {
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << "\n";
        return 1;
    }

    struct addrinfo* result = NULL, * ptr = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed: " << iResult << "\n";
        WSACleanup();
        return 1;
    }

    SOCKET ConnectSocket = INVALID_SOCKET;
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            std::cerr << "socket failed: " << WSAGetLastError() << "\n";
            WSACleanup();
            return 1;
        }

        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        std::cerr << "Unable to connect to server!\n";
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to the server!\n";

    std::string playerName;
    std::cout << "Enter your name: ";
    std::getline(std::cin, playerName);

    iResult = send(ConnectSocket, playerName.c_str(), playerName.length(), 0);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "send failed: " << WSAGetLastError() << "\n";
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Waiting for the race to start...\n";

    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
    if (iResult <= 0) {
        std::cerr << "Failed to receive data from server.\n";
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }
    recvbuf[iResult] = '\0';

    if (std::string(recvbuf, iResult) == "START") {
        countdown();

        std::cout << "The race has begun! Press SPACE to accelerate.\n";
        float speed = 0, distance = 0;

        while (distance < MAX_DISTANCE) {
            if (_kbhit()) {
                char key = _getch();
                if (key == ' ') {
                    speed += 5;
                }
            }

            distance += speed * 0.1f;
            speed *= 0.98f;

            drawTrack(distance);
            Sleep(50);
        }

        iResult = send(ConnectSocket, "400", 3, 0);
        if (iResult == SOCKET_ERROR) {
            std::cerr << "send failed: " << WSAGetLastError() << "\n";
            closesocket(ConnectSocket);
            WSACleanup();
            return 1;
        }

        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            std::cout << recvbuf << "\n";
        }
    }

    std::string command;
    while (true) {
        std::cout << "Enter a command (for example, /exit): ";
        std::cin >> command;
        if (command == "/exit") break;
    }

    closesocket(ConnectSocket);
    WSACleanup();
    return 0;
}