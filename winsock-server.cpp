// winsock-server.cpp : This file contains the 'main' function. Program execution begins and ends here.
//
#include "SocketManager.h"

/*
Main entry point for the socket server application.
The server listens on port 5600 as default.
The createAndListen method of the SocketManager is an infinite loop
and can be interrupted by pressing Ctrl+C to destroy the manager pointer and exit.
*/
int main()
{
    SocketManager *manager = new SocketManager();
    manager->createAndListen();
    delete manager;
    return 0;
}
