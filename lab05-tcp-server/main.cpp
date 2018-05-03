#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <stdio.h>
#include <array>
#include <vector>
#include <cstring>
#include "listing.h"
#include <ctime>

using namespace std;

const uint32_t MAX_MESSAGE_LENGTH = 300;
enum Type : uint8_t {
    TYPE_GET = 0x00,
    TYPE_LIST = 0x01,
    TYPE_TIME = 0x10,
    TYPE_ERROR = 0xff
};


void serve_requests(SOCKET channel);
bool serve_request(SOCKET channel);
int receive_some(SOCKET channel, void* data, size_t size);
int send_some(SOCKET channel, const void* data, size_t size);
bool send_error(SOCKET channel, const std::string& error);
bool serve_file(SOCKET channel, uint32_t path_length);
bool serve_list(SOCKET channel);
bool process_unexpected_message(SOCKET channel, uint32_t length, Type type);
void hex_dump(const void* address, size_t count);
bool serve_time(SOCKET channel);


sockaddr_in
ask_endpoint() {
    cout << "   host: ";
    string host;
    cin >> host;

    cout << "   port: ";
    uint16_t port;
    cin >> port;

    sockaddr_in endpoint;
    ::memset(&endpoint, 0, sizeof(endpoint));

    endpoint.sin_family = AF_INET;

    endpoint.sin_addr.s_addr = inet_addr(host.c_str());

    endpoint.sin_port = ::htons(port);
    return endpoint;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    auto listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        const int error = WSAGetLastError();
        cerr << "ERROR: socket() = " << error << '\n';
        return 1;
    }

    const sockaddr_in address = ask_endpoint();
    const int bindResult = ::bind(listener, (const sockaddr*)&address, sizeof(address));
    if(bindResult != 0) {
        std::cout << "ERROR: bind() = " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    ::listen(listener, 3);

    while (true) {
        sockaddr_in addrClient;
        int len = sizeof(addrClient);
        auto channel = ::accept(listener, (sockaddr*)&addrClient, &len);
        if(channel == INVALID_SOCKET) {
            std::cout << "ERROR: accept() = " << WSAGetLastError() << std::endl;
            WSACleanup();
            break;
            return 1;
        }

        std::clog << "\n   Sender's address: " << inet_ntoa(addrClient.sin_addr)
                  << ":" << ntohs(addrClient.sin_port) << '\n';
        std::clog << "info: client connected\n";
        serve_requests(channel);


        ::closesocket(channel);
        std::clog << "info: client disconnected\n";
    }

    WSACleanup();
    return 0;
}

void
serve_requests(SOCKET channel) {
    while (serve_request(channel)) {
    }
}

bool
serve_request(SOCKET channel) {
    SOCKET client = channel;
    uint32_t length;
    receive_some(client, &length, sizeof(length));

    length = ::ntohl(length);
    if (length > MAX_MESSAGE_LENGTH) {
        send_error(client, "file length is more than 300");
    }
    Type type;
    receive_some(client, &type, sizeof(type));

    switch (type) {
    case TYPE_GET:
        return serve_file(client, length - 1);
    case TYPE_LIST:
        return serve_list(client);
    case TYPE_TIME:
        return serve_time(client);
    default:
        return process_unexpected_message(client, length, type);
    }
}

int
receive_some(SOCKET channel, void* data, size_t size) {
    auto bytes = reinterpret_cast<char*>(data);
    size_t bytes_received = 0;
    while (bytes_received < size) {
        int result = ::recv(
                         channel, &bytes[bytes_received], size - bytes_received, 0);
        if (result <= 0) {
            return result;
        }
        bytes_received += result;
    }
    return 1;
}

int send_some(SOCKET channel, const void* data, size_t size) {
    auto bytes = reinterpret_cast<const char*>(data);
    int result = ::send(channel, bytes, size, 0);
    if (result <= 0) {
        return result;
    }
    return 1;
}

bool
send_error(SOCKET channel, const std::string& error) {
    const uint32_t length = ::htonl(sizeof(Type) + error.size());
    send_some(channel, &length, sizeof(length));

    const Type type = TYPE_ERROR;
    send_some(channel, &type, sizeof(type));

    send_some(channel, &error[0], error.size());
    return true;
}

bool
process_unexpected_message(SOCKET channel, uint32_t length, Type type) {
    std::cerr << "Protocol error: unexpected message "
              << "(type=" << (int)type << ", length=" << length << ")\n";

    std::vector<uint8_t> data(length);
    int result = receive_some(channel, &data[0], data.size());
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: process_unexpected_message() = " << error << '\n';
        return false;
    }
    std::cerr << "Payload dump:\n";
    hex_dump(&data[0], data.size());
    return true;
}

void hex_dump(const void* address, size_t count) {
    cout << "count = " << count << '\n';
    auto bytes = reinterpret_cast<const uint8_t*>(address);
    unsigned int i;
    for (i = 0; i < count; i++) {
        printf("%02x ", bytes[i]);
    }
    putchar('\n');
}

bool
serve_file(SOCKET channel, uint32_t path_length) {
    int result;
    std::vector<char> path(path_length + 1, '\0');
    result = receive_some(channel, &path[0], path_length);

    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: receive_some() = " << error << '\n';
        return false;
    }

    std::fstream input(&path[0], std::ios::in | std::ios::binary);
    if (!input) {
        return send_error(channel, "file is inaccessible");
    }

    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    input.seekg(0, std::ios::beg);

    const uint32_t length = ::htonl(sizeof(Type) + size);

    result = send_some(channel, &length, sizeof(length));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: send_some() = " << error << '\n';
        return false;
    }

    Type type = TYPE_GET;

    result = send_some(channel, &type, sizeof(type));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: send_some() = " << error << '\n';
        return false;
    }
    while (true) {
        std::array<char, 4096> buffer;
        auto bytes_to_send = input.readsome(&buffer[0], buffer.size());

        if (input.bad()) {
            std::fprintf(stderr, "error: %s: I/O failure %d\n", __func__, errno);
            return false;
        }
        if (bytes_to_send == 0) {
            break;
        }

        result = send_some(channel, &buffer[0], bytes_to_send);
        if (result < 0) {
            const int error = WSAGetLastError();
            cerr << "ERROR: send_some() = " << error << '\n';
            return false;
        }

    }
    return true;
}

bool
serve_list(SOCKET channel) {
    int result;
    const auto files = list_files();
    if (files.empty()) {
        return send_error(channel, "unable to enumerate files");
    }
    std::vector<uint8_t> body;

    for (const auto& file : files) {
        const auto old_body_size = body.size();
        body.resize(old_body_size + sizeof(uint8_t) + file.length());
        uint8_t* place = &body[old_body_size];
        *place = file.length();
        place++;
        std::memcpy(place, &file[0], file.length());
    }

    const uint32_t length = ::htonl(sizeof(Type) + body.size());

    result = send_some(channel, &length, sizeof(length));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: send_some() = " << error << '\n';
        return false;
    }

    Type type = TYPE_LIST;

    result = send_some(channel, &type, sizeof(type));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: send_some() = " << error << '\n';
        return false;
    }

    result = send_some(channel, &body[0], body.size());
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: send_some() = " << error << '\n';
        return false;
    }

    return true;
}

bool
serve_time(SOCKET channel) {
    std::time_t serverTime = std::time(nullptr);
    int result = send_some(channel, &serverTime, sizeof(serverTime));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: send_some() = " << error << '\n';
        return false;
    }
    return true;
}
