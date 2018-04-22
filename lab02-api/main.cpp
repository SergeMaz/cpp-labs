#include <iostream>
#include <winsock2.h>

using namespace std;

int main()
{
    // Инициализировать API сокетов (ws2_32.dll).
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    auto channel = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int error = 0;
    int length = sizeof(error);
    getsockopt(channel, SOL_SOCKET, SO_ERROR, (char*)&error, &length);
    printf("Socket error (code %d)!\n", error);
    // Завершить работу с API сокетов (ws2_32.dll).
    WSACleanup();
    return 0;
}
