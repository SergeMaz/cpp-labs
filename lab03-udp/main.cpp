#include <iostream>
#include <winsock2.h>
#include <sys/types.h>
#include <array>
using namespace std;

void do_send(SOCKET channel);
void do_receive(SOCKET channel);

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

int main()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET channel = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (channel == INVALID_SOCKET) {
        const int error = WSAGetLastError();
        cerr << "ERROR: socket() = " << error << '\n';
        return 1;
    }

    int enabled = 1;
    int iresult = setsockopt(channel, SOL_SOCKET, SO_BROADCAST, (const char*)&enabled, sizeof(enabled));
    if (iresult < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: sockopt() = " << error << '\n';
        return 1;
    }

    cout << "Listening address:\n";
    const sockaddr_in address = ask_endpoint();
    const int error = ::bind(channel, (const sockaddr*)&address, sizeof(address));
    if (error < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: bind() = " << error << '\n';
        return 1;
    }

    char answer = '?';
    while (answer != 'q') {
        cout << "(c)heck, (r)eceive, (s)end, or (q)uit? ";
        cin >> answer;
        cin.ignore();

        switch (answer) {
        case 's':
            do_send(channel);
            break;
        case 'r':
            do_receive(channel);
            break;
        case 'q':
            continue;
        default:
            cerr << "Please enter 'r', 's', or 'q'!\n";
        }
    }
    WSACleanup();

    return 0;
}

void
do_send(SOCKET channel) {
    cout << "Enter target address:\n";
    const sockaddr_in address = ask_endpoint();
    std::string message;
    cout << "   Message: ";
    cin.ignore(); // Для работы (85)getline
    std::getline(cin, message);
    int result = ::sendto(channel, &message[0], message.size(), 0,
        (const sockaddr*)&address, sizeof(address));
    cout << "   Size of message(letters) = " << message.length() << endl;
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: send() = " << error << '\n';
        return;
    } else {
        cout << "   Size of message(bytes) = " << message.size() << endl;
    }
}


void
do_receive(SOCKET channel) {

    array<char, 1536> message;
    sockaddr_in address;
    int length = sizeof(address);
    int result = ::recvfrom(channel, &message[0], message.size(), 0, (struct sockaddr*)&address, &length);
    if (result < 0) {
        const int error = WSAGetLastError();
        cerr << "ERROR: recieve() = " << error << '\n';
        return;
    } else {
        cout << "   Message: ";
        cout.write(&message[0], result);
        cout << "\n   Sender's address: " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << '\n';
    }
}
