#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <array>
#include <stdio.h>
#include <vector>
#include <locale>
#include <ctime>

using namespace std;

enum Type : uint8_t {
    TYPE_GET = 0x00,
    TYPE_LIST = 0x01,
    TYPE_TIME = 0x10,
    TYPE_ERROR = 0xff
};

int receive_some(SOCKET channel, void* data, size_t size);
int send_some(SOCKET channel, const void* data, size_t size);
bool download(SOCKET channel, const std::string& path);
void hex_dump(const void* address, size_t count);
bool process_error_response(SOCKET channel, uint32_t text_size);
bool process_unexpected_response(SOCKET channel, uint32_t length, Type type);
bool process_get_response(SOCKET channel, const std::string& path, uint32_t file_size);
bool list_files(SOCKET channel);
bool process_list_response(SOCKET channel, uint32_t data_length);
bool server_time(SOCKET channel);


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

    auto channel = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (channel == INVALID_SOCKET) {
        const int error = WSAGetLastError();
        cerr << "ERROR: socket() = " << error << '\n';
        return 1;
    }

    const sockaddr_in address = ask_endpoint();
    const int result = ::connect(channel, (const sockaddr*)&address, sizeof(address));
    if(result != 0) {
        std::cout << "ERROR: connect() = " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    while (true) {
        std::cerr << "> ";
        std::string command;
        std::cin >> command;

        if (command == "/quit") {
            break;
        } else if (command == "/get") {
            std::string path;
            std::cin >> path;
            download(channel, path);
        } else if (command == "/list") {
            list_files(channel);
        } else if (command == "/time") {
            server_time(channel);
        } else {
            std::cerr << "Commands:\n"
                      "\t/get <file>\n"
                      "\t/list\n"
                      "\t/time\n"
                      "\t/quit\n";
        }
    }
    ::closesocket(channel);

    WSACleanup();
    return 0;
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
download(SOCKET channel, const std::string& path) {
    uint32_t length = htonl(sizeof(Type) + path.size());
    send_some(channel, &length, sizeof(length));

    Type type = TYPE_GET;
    int result;
    result = send_some(channel, &type, sizeof(type));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: send_some() = " << error << '\n';
        return false;
    }
    result = send_some(channel, &path[0], path.size());
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: send_some() = " << error << '\n';
        return false;
    }

    result = receive_some(channel, &length, sizeof(length));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: receive_some() = " << error << '\n';
        return false;
    }
    result = receive_some(channel, &type, sizeof(type));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: receive_some() = " << error << '\n';
        return false;
    }

    length = ntohl(length) - 1;

    switch (type) {
    case TYPE_ERROR:
        return process_error_response(channel, length);
    case TYPE_GET:
        return process_get_response(channel, path, length);
    default:
        return process_unexpected_response(channel, length, type);
    }
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
process_error_response(SOCKET channel, uint32_t text_size) {
    std::string text(text_size + 1, '\0');
    int result;
    result = receive_some(channel, &text[0], text_size);
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: process_error_response() = " << error << '\n';
        return false;
    }
    std::cerr << "Server error: " << text << '\n';
    return true;
}

bool
process_unexpected_response(SOCKET channel, uint32_t length, Type type) {
    std::cerr << "Protocol error: unexpected message "
              << "(type=" << (int)type << ", length=" << length << ")\n";

    std::vector<uint8_t> data(length);
    int result = receive_some(channel, &data[0], data.size());
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: process_unexpected_response() = " << error << '\n';
        return false;
    }
    std::cerr << "Payload dump:\n";
    hex_dump(&data[0], data.size());
    return true;
}

bool
process_get_response(
        SOCKET channel, const std::string& path, uint32_t file_size) {
    std::clog << "Downloading " << file_size << " bytes...\n";

    std::fstream output(path, std::ios::out | std::ios::trunc | std::ios::binary);

    uint32_t bytes_received = 0;
    while (bytes_received < file_size) {
        std::array<char, 4096> buffer;
        const int result = ::recv(channel, &buffer[0], buffer.size(), 0);
        if (result <= 0) {
            return false;
        }
        output.write(&buffer[0], result);
        bytes_received += result;
    }
    return true;
}

bool
list_files(SOCKET channel) {
    uint32_t length = htonl(sizeof(Type) + 4);
    send_some(channel, &length, sizeof(length));

    Type type = TYPE_LIST;
    int result;
    result = send_some(channel, &type, sizeof(type));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: send_some() = " << error << '\n';
        return false;
    }

    result = receive_some(channel, &length, sizeof(length));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: receive_some() = " << error << '\n';
        return false;
    }
    result = receive_some(channel, &type, sizeof(type));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: receive_some() = " << error << '\n';
        return false;
    }

    length = ntohl(length) - 1;

    switch (type) {
    case TYPE_ERROR:
        return process_error_response(channel, length);
    case TYPE_LIST:
        return process_list_response(channel, length);
    default:
        return process_unexpected_response(channel, length, type);
    }
}

bool
process_list_response(SOCKET channel, uint32_t data_length) {
    std::vector<uint8_t> data(data_length);
    int result = receive_some(channel, &data[0], data.size());
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: process_list_response() = " << error << '\n';
        return false;
    }
    const uint8_t* entry = &data[0];
    while (entry <= &data.back()) {
        const auto entry_length = *entry;
        entry++;
        const auto file_name = reinterpret_cast<const char*>(entry);
        entry += entry_length;
        std::cout.write(file_name, entry_length);
        std::cout << '\n';
    }
    return true;
}

bool
server_time(SOCKET channel) {
    uint32_t length = htonl(sizeof(Type)+4);
    send_some(channel, &length, sizeof(length));
    Type type = TYPE_TIME;
    std::time_t serverTime;
    int result = send_some(channel, &type, sizeof(type));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: send_some() = " << error << '\n';
        return false;
    }

    std::clog << "Server Time: \n";
    char buffer[100];

    result = receive_some(channel, &serverTime, sizeof(serverTime));
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: receive_some() = " << error << '\n';
        return false;
    }
    std::strftime(buffer, sizeof(buffer), "%A %c", std::localtime(&serverTime));
    std::cout << buffer << endl;

    return true;
}
