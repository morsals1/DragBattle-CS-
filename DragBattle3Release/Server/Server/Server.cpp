#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT "8080"
#define DEFAULT_BUFLEN 512
#define MAX_PLAYERS 2
#define TRACK_LENGTH 400

SOCKET clients[MAX_PLAYERS];
float distances[MAX_PLAYERS] = { 0, 0 };
bool playerFinished[MAX_PLAYERS] = { false, false };
std::string playerNames[MAX_PLAYERS];
bool raceFinished = false;

struct ClientData {
    int playerIndex;
    SOCKET clientSocket;
};

void sendWinner(int winnerIndex) {
    std::string winnerMessage = "Winner\n";
    std::string loserMessage = "Loser\n";

    send(clients[winnerIndex], winnerMessage.c_str(), winnerMessage.length(), 0);
    send(clients[1 - winnerIndex], loserMessage.c_str(), loserMessage.length(), 0);

    std::cout << "Player " << playerNames[winnerIndex] << " won the race!\n";
    std::cout << "Player " << playerNames[1 - winnerIndex] << " lost the race.\n";
}

DWORD WINAPI handleClient(LPVOID lpParam) {
    ClientData* data = (ClientData*)lpParam;
    int playerIndex = data->playerIndex;
    SOCKET clientSocket = data->clientSocket;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    while (true) {
        int bytesReceived = recv(clientSocket, recvbuf, recvbuflen, 0);
        if (bytesReceived <= 0) {
            std::cout << "Player " << playerNames[playerIndex] << " disconnected.\n";
            return 0;
        }

        recvbuf[bytesReceived] = '\0';
        float speed = atof(recvbuf);
        distances[playerIndex] += speed;

        if (distances[playerIndex] >= TRACK_LENGTH && !playerFinished[playerIndex]) {
            playerFinished[playerIndex] = true;

            if (!raceFinished) {
                raceFinished = true;
                sendWinner(playerIndex);
            }
            return 0;
        }
    }
    return 0;
}

int main() {
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << "\n";
        return 1;
    }

    struct addrinfo* result = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed: " << iResult << "\n";
        WSACleanup();
        return 1;
    }

    SOCKET ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << "\n";
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << "\n";
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << "\n";
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Waiting for players...\n";

    for (int i = 0; i < MAX_PLAYERS; i++) {
        clients[i] = accept(ListenSocket, NULL, NULL);
        if (clients[i] == INVALID_SOCKET) {
            std::cerr << "accept failed: " << WSAGetLastError() << "\n";
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }

        std::cout << "Player " << i + 1 << " connected! Requesting name...\n";

        char nameBuffer[DEFAULT_BUFLEN];
        int bytesReceived = recv(clients[i], nameBuffer, DEFAULT_BUFLEN, 0);
        if (bytesReceived <= 0) {
            std::cerr << "Failed to receive name from player " << i + 1 << ".\n";
            closesocket(clients[i]);
            continue;
        }
        nameBuffer[bytesReceived] = '\0';
        playerNames[i] = std::string(nameBuffer);

        std::cout << "Player " << i + 1 << " name: " << playerNames[i] << "\n";
    }

    std::cout << "The race begins!\n";
    std::string startMsg = "START";

    for (int i = 0; i < MAX_PLAYERS; i++) {
        iResult = send(clients[i], startMsg.c_str(), startMsg.length(), 0);
        if (iResult == SOCKET_ERROR) {
            std::cerr << "send failed: " << WSAGetLastError() << "\n";
            closesocket(clients[i]);
            WSACleanup();
            return 1;
        }
    }

    ClientData clientData1 = { 0, clients[0] };
    ClientData clientData2 = { 1, clients[1] };

    HANDLE thread1 = CreateThread(NULL, 0, handleClient, &clientData1, 0, NULL);
    HANDLE thread2 = CreateThread(NULL, 0, handleClient, &clientData2, 0, NULL);

    WaitForSingleObject(thread1, INFINITE);
    WaitForSingleObject(thread2, INFINITE);

    closesocket(ListenSocket);
    WSACleanup();

    std::cout << "The race is over!\n";
    return 0;
}