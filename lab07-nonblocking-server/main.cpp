#define WINVER _WIN32_WINNT_WIN7
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <array>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>
#include "listing.h"
#include "sync.h"
#include <winsock2.h>


using namespace std;

struct Client {
    SOCKET channel;
    sockaddr_in peer;
};

struct Server {
    SOCKET listener;
    std::vector<Client> clients;
};


bool make_nonblocking(SOCKET handle);
bool run_server(Server& server);
bool process_listener(Server& server, WSAPOLLFD fd);
bool process_client(Server& server, size_t index, WSAPOLLFD fd);
void sock_err(WSAPOLLFD fd);

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

    auto listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        const int error = WSAGetLastError();
        std::cerr << "ERROR: socket() = " << error << endl;
        return 1;
    }

    const sockaddr_in address = ask_endpoint();
    const int bindResult = ::bind(listener, (const sockaddr*)&address, sizeof(address));
    if(bindResult != 0) {
        std::cerr << "ERROR: bind() = " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }
    make_nonblocking(listener);

    while (true) {
        Server server;
        server.listener = listener;
        while (run_server(server));
        for(const Client& client : server.clients) {
            ::closesocket(client.channel);
        }
        ::closesocket(server.listener);
    }

    return 0;
}

bool
make_nonblocking(SOCKET handle) {
    unsigned long int on = 1;
    const int result = ::ioctlsocket(handle, FIONBIO, &on);
    if (result < 0) {
        fprintf(stderr, "error: ioctlsocket()=%d\n", WSAGetLastError());
        return false;
    }
    return true;
}

bool
run_server(Server& server) {
    std::vector<WSAPOLLFD> fds;

    for (const auto& client : server.clients) {
        WSAPOLLFD fd;
        fd.fd = client.channel;
        fd.events = POLLIN;
        fds.push_back(fd);
    }

    WSAPOLLFD fd;
    fd.fd = listener;
    fd.events = POLLIN;
    fds.push_back(fd);

    ::WSAPoll(&fds[0], fds.size(), -1);
    if (result < 0) {
        fprintf(stderr, "error: WSAPoll()=%d\n", WSAGetLastError());
        return false;
    }

    process_listener(server, fds.back());
    for (size_t i = 0; i < server.clients.size(); i++) {
        const auto progress = process_client(server, i, fds[i]);
        if (!progress) {
            ::closesocket(server.clients[i].channel);
            server.clients[i].channel = INVALID_SOCKET;
        }
    }

    auto it = server.clients.begin();
    while (it != server.clients.end()) {
        if (it->channel == INVALID_SOCKET) {
            it = server.clients.erase(it);
        } else {
            ++it;
        }
    }

    return true;
}

void
sock_err (WSAPOLLFD fd) {
    getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&error, &length);
    fprintf(srderr, "error: Socket()=%d\n", error);
    return;
}

bool
process_listener(Server& server, WSAPOLLFD fd) {
    if (fd.revents & POLLERR) {
        sock_err(fd.fd);
        return false;
    }
    fd.fd = listener;
    // fd.revents & POLLIN
    while (true) {
        sockaddr_in peer;
        int peer_size = sizeof(peer);
        auto channel = ::accept(listener, (struct sockaddr*)&peer, &peer_size);
        if (channel == INVALID_SOCKET) {
            const int code = ::WSAGetLastError();
            if (code == WSAEWOULDBLOCK) {
                return true;
            }
            return false;
        }
        if (make_nonblocking(channel) == false) {
            ::closesocket(channel);
            continue;
        }
        Client client;
        client.channel = channel;
        client.peer = peer;
        server.clients.push_back(client);

        fprintf(stderr, "info: client connected: peer=%s, clients=%u\n",
                endpoint_to_string(peer).c_str(), server.clients.size());
    }
}
