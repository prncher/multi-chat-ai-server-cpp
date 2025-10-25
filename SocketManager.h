#pragma once
#include <csignal>
#include <iostream>
#include <list>
#include <thread>
#include <curl/curl.h>
#include "SocketConnection.h"

constexpr auto DEFAULT_PORT = 5600;

class SocketManager
{
public:
    // Constructor
    SocketManager();
    ~SocketManager();
    // Member functions
    void createAndListen();
    // static members
	static volatile sig_atomic_t signal_received;
    static void signal_handler(int signum);
    static std::list<SocketConnection*> socketConnections;
    static std::map<int, SOCKET> connectionMaps;
    static SOCKET listenSocket;
    static CURL* curl;
};

