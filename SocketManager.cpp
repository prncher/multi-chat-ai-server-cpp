#include "SocketManager.h"

volatile sig_atomic_t SocketManager::signal_received = 0;
SOCKET SocketManager::listenSocket = INVALID_SOCKET;
void SocketManager::signal_handler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\nCaught Ctrl+C (SIGINT). Initiating shutdown..." << std::endl;
        signal_received = 1; // Set the flag
        for (SocketConnection *connection : socketConnections) {
            // this will make the detached thread exit after any cleanup.
            connection->closeSocket();
        }
        socketConnections.clear();
        // closing the one listening socket for exiting the main thread.
        closesocket(listenSocket);
    }
}
std::list<SocketConnection*> SocketManager::socketConnections = {};
std::map<int, SOCKET> SocketManager::connectionMaps = {};
CURL* SocketManager::curl = nullptr;

SocketManager::SocketManager() {
	std::signal(SIGINT, signal_handler);
    curl_global_init(CURL_GLOBAL_ALL);
}

SocketManager::~SocketManager()
{
    curl_global_cleanup();
}

/*
 This method starts the socket listening.
 Once a client socket establishes a connection, then listening on that connection
 is continued on a separate thread that is detached.
 Only option to stop this main thread is pressing Ctrl+C.
 If any client socket is closed, then the corresponding detached thread will 
 terminate. When all the client sockets are closed, no detached threads will
 be running, but the main thread still continue for any new client socket connection.
 The SocketConnection object created will be destroyed when the client socket is closed.
*/
void SocketManager::createAndListen() {
    WSADATA wsaData;
    int iResult;
    std::cout << "Socket Server running. Press Ctrl+C to exit." << std::endl;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return;
    }

    // Create a SOCKET for the server to listen for client connections.
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(DEFAULT_PORT); // Port to listen on

    // Setup the listening socket
    iResult = bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    iResult = listen(listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    while (!signal_received) {
        std::cout << "Ready to accept socket requests ..." << std::endl;
        SOCKET clientSocket = INVALID_SOCKET;
        clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            continue;
        }
        else {
            SocketConnection *connection = new SocketConnection(clientSocket);
            socketConnections.push_back(connection);
            std::thread recvThread([connection]() {
                connection->startReceiveMessages(
                    &connectionMaps,&socketConnections);
                });
            // An indepenedent receiving mechanism
            recvThread.detach();
        }
    }

    std::cout << "Exiting socket server." << std::endl;

    WSACleanup();
}
