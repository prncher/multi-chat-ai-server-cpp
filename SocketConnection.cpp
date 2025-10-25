#include "SocketConnection.h"

SocketConnection::SocketConnection() {
    this->clientSocket = INVALID_SOCKET;
    this->upgradedConnection = false;
}
SocketConnection::SocketConnection(SOCKET clientSocket)
{
    this->clientSocket = clientSocket;
    if (this->clientSocket == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
    }
    this->upgradedConnection = false;
    url = "http://127.0.0.1:" + std::to_string(LLAMA_CPP_PORT) + "/v1/chat/completions";
}

void SocketConnection::closeSocket()
{
    closesocket(this->clientSocket);
    this->clientSocket = INVALID_SOCKET;
}
void SocketConnection::startReceiveMessages(
    std::map<int, SOCKET> *socketConnections, 
    std::list<SocketConnection*> *listConnections)
{
    int iResult;
    do {
        if (this->clientSocket == INVALID_SOCKET) {
            listConnections->remove(this);
            delete this;
            return;
        }
        char recvbuf[DEFAULT_BUFLEN];
        int recvbuflen = DEFAULT_BUFLEN;

        iResult = recv(this->clientSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            printf("Bytes received: %d\n", iResult);

            std::string data(recvbuf, iResult);
            std::size_t get_pos = data.find("GET");
            if (get_pos != std::string::npos) {
                // this is a http get call
                if (!this->upgradedConnection) {
                    this->sendUpgrade(socketConnections, recvbuf, iResult);
                }
            }
            else {
                std::string message = this->parseMessage(recvbuf, iResult);
                if (message.size() > 0) {
                    // We have the query from client.
                    // Now get the answer from the AI agent app.
                    std::string chatResult = this->sendChatQuery(message);
                    std::string content = this->getContent(chatResult);

                    // Convert the markdown to html in the most minimal way.
                    content = this->transformMarkdown(content);

                    FrameData frame = this->prepareMessage(content);
                    // Echo the buffer back to the sender
                    int iSendResult = send(this->clientSocket, frame.data.data(), (int)frame.size, 0);
                    //send(ClientSocket, recvbuf, iResult, 0);
                    if (iSendResult == SOCKET_ERROR) {
                        printf("send failed with error: %d\n", WSAGetLastError());
                        closesocket(this->clientSocket);
                    }
                    printf("Bytes sent: %d\n", iSendResult);
                }

            }
        }
    } while (iResult >= 0);

    // shutdown the connection since we're done
    iResult = shutdown(this->clientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        delete this;
        return;
    }

    // cleanup
    closesocket(this->clientSocket);

    // remove from the parent connections.
    listConnections->remove(this);

    // This object was created using new. So self destruct.
    // This code will not work if the object was not created using new.
    delete this;
}

std::string SocketConnection::parseMessage(const char* buffer, size_t buffer_size)
{
    // This assumes a single, unfragmented text frame
    unsigned char firstByte = buffer[0];
    unsigned char secondByte = buffer[1];

    // Is this a final text frame (FINAL bit set, opcode 0x1)
    if ((firstByte & 0x8F) == SECOND_BIT) {
        bool isMasked = (secondByte & FIRST_BIT) != 0;
        size_t payloadLen = secondByte & EXTENDED_PAYLOAD_INDICATOR;
        size_t offset = 2; // Start of masking key or payload

        if (payloadLen == FIRST_SIXTEEN_BITS) {
            payloadLen = (static_cast<size_t>(buffer[2]) << 8) | buffer[3];
            offset += 2;
        }
        else if (payloadLen == EXTENDED_PAYLOAD_INDICATOR) {
            // Handle 64 bit extended payload length (more complex)
            // For simplicity, assuming smaller messages for this example
            std::cerr << "Extended payload not handled, send short queries" << std::endl;
            return std::string();
        }

        if (isMasked) {
            unsigned char maskingKey[MASK_SIZE];
            memcpy(maskingKey, buffer + offset, MASK_SIZE);
            offset += MASK_SIZE;

            std::vector<char> unmaskedPayload(payloadLen);
            for (size_t i = 0; i < payloadLen; ++i) {
                unmaskedPayload[i] = buffer[offset + i] ^ maskingKey[i % MASK_SIZE];
            }
            std::string receivedText(unmaskedPayload.begin(), unmaskedPayload.end());
            std::cout << "Received text: " << receivedText << std::endl;
            return receivedText;
        }

        return std::string();
    }
    else {
        std::cerr << "Received non-text or fragmented frame." << std::endl;
        return std::string();
    }

    return std::string();
}

FrameData SocketConnection::prepareMessage(const std::string& message)
{
    /*
         0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
     | |1|2|3|       |K|             |                               |
     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
     |     Extended payload length continued, if payload len == 127  |
     + - - - - - - - - - - - - - - - +-------------------------------+
     |                               |Masking-key, if MASK set to 1  |
     +-------------------------------+-------------------------------+
     | Masking-key (continued)       |          Payload Data         |
     +-------------------------------- - - - - - - - - - - - - - - - +
     :                     Payload Data continued ...                :
     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
     |                     Payload Data continued ...                |
     +---------------------------------------------------------------+
    
    */
 
    const char* buffer = message.c_str();
    const size_t messageSize = message.size();
    uint16_t firstByte = 0x80 | OPCODE_TEXT;
    uint16_t masked = FIRST_SIXTEEN_BITS | 0x00;
    size_t frameSize = 4 + messageSize;
    std::vector<char> frameBuffer(frameSize);
    memcpy(frameBuffer.data(), &firstByte, 1);
    memcpy(frameBuffer.data() + 1, &masked, 1);
    uint8_t first = static_cast<uint8_t>((messageSize >> 8) & 0xFF);
    memcpy(frameBuffer.data() + 2, &first, 1);
    uint8_t second = static_cast<uint8_t>(messageSize & 0xFF);
    memcpy(frameBuffer.data() + 3, &second, 1);
    memcpy(frameBuffer.data() + 4, buffer, messageSize);

    return {
        frameBuffer,
        frameSize
    };
}

void SocketConnection::sendUpgrade(
    std::map<int, SOCKET> *socketConnections,
    const char* buffer, 
    int buffer_size)
{
    std::map<std::string, std::string, CaseInsensitiveCompare> headers;
    std::string data(buffer, buffer_size);

    size_t totalLength = 0;
    int totalLines = 0;

    // Create a string stream to treat the data like a file
    std::istringstream stream(data);
    std::string line;

    // Read the first line (e.g., "HTTP/1.1 200 OK" or "GET / HTTP/1.1")
    std::getline(stream, line);
    std::string::size_type client_id_pos = line.find("/ws/");
    if (client_id_pos != std::string::npos) {
        // Maximum single digit clients.
        int clientId = std::stoi(line.substr(client_id_pos + 4, 1));
        socketConnections->insert(
            std::pair<int, SOCKET>(clientId, this->clientSocket));
    }

    // Loop through each header line until an empty line is found
    while (!line.empty()) {
        totalLines += 1;
        totalLength += line.size();
        std::cout << "Length: " << line.size() << " Line Read: " << line << std::endl;
        // Find the position of the colon ':'
        std::string::size_type separator_pos = line.find(':');
        if (separator_pos != std::string::npos) {
            // Extract the key and value
            std::string key = line.substr(0, separator_pos);
            std::string value = line.substr(separator_pos + 1);

            // Trim leading and trailing whitespace from the key and value
            // (Boost has useful functions for this, but can be done manually)
            size_t start = value.find_first_not_of(" \t\r\n");
            size_t end = value.find_last_not_of(" \t\r\n");
            if (std::string::npos != start && std::string::npos != end) {
                value = value.substr(start, end - start + 1);
            }

            // Store in the map
            headers[key] = value;
        }

        std::getline(stream, line);
    }
    std::cout << "Total line length read :  " << totalLength << "Total Lines: " << totalLines << std::endl;
    if (headers["Connection"] == "Upgrade" || headers["Connection"] == "upgrade"
        && headers["Upgrade"] == "websocket") {
        this->sendUpgradeMessage(headers["Sec-WebSocket-Key"]);
        this->upgradedConnection = true;
    }
}

void SocketConnection::sendUpgradeMessage(const std::string& websocket_key)
{
    std::string sec_websocket_accept = create_websocket_accept_key(websocket_key);

    std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
    response += "Upgrade: websocket\r\n";
    response += "Connection: Upgrade\r\n";
    response += "Sec-WebSocket-Accept: " + sec_websocket_accept + "\r\n\r\n";
    int iResult = send(this->clientSocket, response.c_str(), (int)response.size(), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(this->clientSocket);
    }
    printf("Bytes Sent: %ld\n", iResult);
}

std::string SocketConnection::create_websocket_accept_key(const std::string& websocket_key)
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    std::string access_key = std::string(websocket_key + SECRET_KEY);
    SHA1(reinterpret_cast<const unsigned char*>(access_key.c_str()), access_key.size(), hash);
    std::string sec_accept_hashed = std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH);
    return this->encode(sec_accept_hashed);
}

std::string SocketConnection::encode(const std::string& input)
{
    BIO* bio = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    bio = BIO_push(bio, mem);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // No newlines
    BIO_write(bio, input.data(), (int)input.size());
    BIO_flush(bio);

    struct buf_mem_st
    {
        size_t length;  /* current number of bytes */
        char* data;
        size_t max; /* size of buffer */
    };

    buf_mem_st* bufferPtr;
    BIO_get_mem_ptr(bio, &bufferPtr);
    std::string encoded(bufferPtr->data, bufferPtr->length);

    BIO_free_all(bio);
    return encoded;
}

std::string SocketConnection::sendChatQuery(const std::string& query)
{
    CURL *curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    }

    if (curl) {
        std::string json_data = "{ \
            \"messages\": [ \
        {\"role\": \"system\", \"content\" : \"You are a helpful assistant.\"}, \
        { \"role\": \"user\", \"content\" : \"" + query + "\"} ] }";
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());

        curl_slist* contentheader = NULL;
        contentheader = curl_slist_append(contentheader, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, contentheader);

        // Set a callback function to handle the response data
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        std::string readBuffer;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        CURLcode res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        // release header
        curl_slist_free_all(contentheader);

        // release handle
        curl_easy_cleanup(curl);

        return readBuffer;
    }


    return std::string();
}

size_t SocketConnection::writeCallback(char* data, 
    size_t size, 
    size_t nmemb, 
    void* clientp) {
    char* asciiAnswer = new char[nmemb];
    std::fill(asciiAnswer, asciiAnswer + nmemb, 0);
    int j = 0;
    for (int i = 0; i < nmemb; ++i) {
        uint8_t o = static_cast<unsigned char>(data[i]);
        if (o >= 32 && o <= 127) {
            asciiAnswer[j++] = data[i];
        }
        else {
            std::cout << "Non Ascii: " << data[i] << std::endl;
        }
    }
    ((std::string*)clientp)->append(asciiAnswer,j);
    delete asciiAnswer;

    return nmemb;
}

std::string SocketConnection::getContent(const std::string& answer)
{
    std::regex pattern(R"(content\":\"(.*)\"\})");
    std::smatch matches;
    if (std::regex_search(answer, matches, pattern)) {
        return matches[1].str();
    }
    else {
        std::cout << "Received content: " << answer << std::endl;
        std::cerr << "Unable to get the answer content" << std::endl;
    }

    return std::string();
}

std::string SocketConnection::transformMarkdown(std::string& answer)
{
    std::string html = "<p>";
    size_t nmemb = answer.size();
    bool start = false;
    bool startq = false;
    for (int i = 0; i < nmemb; ++i) {
        if (answer[i] == '\"') {
            if (startq) {
                html.append("</q>", 4);
                startq = false;
            }
            else {
                html.append("<q>", 3);
                startq = true;
            }
        }
        else if (answer[i] == '\\' && i + 1 < nmemb && answer[i + 1] == '\"') {
            if (startq) {
                html.append("</q>", 4);
                startq = false;
            }
            else {
                html.append("<q>", 3);
                startq = true;
            }
            i += 1;
        }
        else if (answer[i] == '\n') {
            html.append("<br>", 4);
        }
        else if (answer[i] == '*' && i + 1 < nmemb && answer[i + 1] == '*') {
            if (start) {
                html.append("</strong>", 9);
                start = false;
            }
            else {
                html.append("<strong>", 8);
                start = true;
            }
            i += 1;
        }
        else if (answer[i] == '\\' && i + 1 < nmemb && answer[i + 1] == 'n') {
            html.append("<br>", 4);
            i += 1;
        }
        else {
            html += answer[i];
        }
    }

    html += "</p>";
    return html;
}
