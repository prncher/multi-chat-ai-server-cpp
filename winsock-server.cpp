// winsock-server.cpp : This file contains the 'main' function. Program execution begins and ends here.
//
#include "SocketManager.h"

int main()
{
    SocketManager *manager = new SocketManager();
    manager->createAndListen();
    delete manager;
    return 0;
}
