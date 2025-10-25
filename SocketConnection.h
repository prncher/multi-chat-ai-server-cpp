#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <map>
#include <list>
#include <vector>
#include <sstream>
#include <regex>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>

#include <curl/curl.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "libcrypto.lib")

#define EXTENDED_PAYLOAD_INDICATOR 127 // as byte: 01111111
#define FIRST_SIXTEEN_BITS 126 // as byte: 01111110
#define MASK_SIZE 4
#define DEFAULT_BUFLEN 1024
#define FIRST_BIT 128
#define SECOND_BIT 129
#define OPCODE_TEXT 0x01
#define SECRET_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
constexpr auto LLAMA_CPP_PORT = 8100;

struct FrameData {
    std::vector<char> data;
    size_t size;
};

struct memory {
    char* response;
    size_t size;
};

struct CaseInsensitiveCompare {
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        return std::lexicographical_compare(lhs.begin(), lhs.end(),
            rhs.begin(), rhs.end(),
            [](char c1, char c2) {
                return std::toupper(c1) < std::toupper(c2);
            });
    }
};

class SocketConnection {
    public:
        // Constructor
        SocketConnection();
        SocketConnection(SOCKET clientSocket);
        // Member functions
        void closeSocket();
        void startReceiveMessages(std::map<int, SOCKET> *socketConnections,
            std::list<SocketConnection*>* listConnections);
    private:
        SOCKET clientSocket;
        bool upgradedConnection;
        std::string url;
        std::string parseMessage(const char* buffer, size_t buffer_size);
        FrameData prepareMessage(const std::string& message);
        void sendUpgrade(std::map<int, SOCKET> *socketConnections,
            const char* buffer, int buffer_size);
        void sendUpgradeMessage(const std::string& websocket_key);
        std::string create_websocket_accept_key(const std::string& websocket_key);
        std::string encode(const std::string& input);
        std::string sendChatQuery(const std::string& query);
        std::string getContent(const std::string& query);
        std::string transformMarkdown(std::string& answer);
        static size_t writeCallback(char* data, size_t size, size_t nmemb, void* clientp);
};