#include <stdio.h>
#include <iostream>
#include <cstdint>
#include <array>
#include <vector>
#include <ctime>
#include <cstring>
#include <winsock2.h>

using namespace std;


void demo_size();
void demo_flags();
bool quot_rem(int x, int y, int* quotient, int* remainder);
void demo_pointers();
void demo_arrays();
void hex_dump(const void* address, size_t count);
void demo_alignment();
void demo_byte_order();

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    demo_size();
    demo_flags();
    demo_pointers();
    demo_arrays();
    demo_alignment();
    demo_byte_order();
    WSACleanup();
    return 0;
}

void demo_size() {
    puts(__func__);
    uint8_t x;
    uint16_t y;
    uint32_t z;

    printf("sizeof(x) = %d\n", sizeof(x));
    printf("sizeof(y) = %d\n", sizeof(y));
    printf("sizeof(z) = %d\n", sizeof(z));
    putchar('\n');
}

void demo_flags() {
    puts(__func__);
    enum Flag : uint32_t {  // Флаги будут числами типа uint32_t:
        FLAG_QUICK = 0x01,  // - быстро
        FLAG_GOOD  = 0x02,  // - качественно
        FLAG_CHEAP = 0x04   // - недорого
    };
    int flags = FLAG_GOOD | FLAG_CHEAP;  // не обязательно FLAG_GOOD


    if (flags & FLAG_GOOD){
        puts("FLAG_GOOD present");
    } else {
        puts("FLAG_GOOD absent");
    }


    if (flags & ~FLAG_QUICK) {
        puts("FLAG_QUICK absent");
    } else {
        puts("FLAG_QUICK present");
    }
    putchar('\n');
}

bool quot_rem(int x, int y, int* quotient, int* remainder) {
    if (y == 0) {
        cout << "x = " << x << " y  = " << y << "\n" << "x divided by zero!" << '\n';
    } else {
        if (quotient == NULL && remainder == NULL) {
            cout << "quotient is NULL and remainder is NULL too " << '\n';
        }
        else if (quotient == NULL) {
             cout << "quotient is NULL" << '\n';
            *remainder = x%y;
        }
        else if (remainder == NULL) {
             cout << "remainder is NULL" << '\n';
            *quotient = x/y;
        }
        else {
            *quotient = x/y;
            *remainder = x%y;
        }

        cout << "x = " << x << " y  = " << y << '\n';
    }
}

void demo_pointers(){
    puts(__func__);
    int quotient, remainder;

    quot_rem(7,2,&quotient,&remainder);
    cout << "quotient = " << quotient << " remainder = " << remainder << '\n';

    quot_rem(7,0,&quotient,&remainder);
    cout << "quotient = " << quotient << " remainder = " << remainder << '\n';

    quot_rem(7,2,NULL,&remainder);
    cout << "quotient = " << quotient << " remainder = " << remainder << '\n';

    quot_rem(7,2,&quotient,NULL);
    cout << "quotient = " << quotient << " remainder = " << remainder << '\n';

    quot_rem(7,2,NULL,NULL);
    cout << "quotient = " << quotient << " remainder = " << remainder << '\n';
    putchar('\n');
}

void demo_arrays(){
    puts(__func__);

    std::array<int, 42> static_buffer;
    printf("sizeof(static_buffer) = %d\n", sizeof(static_buffer));
    printf("sizeof(static_buffer[0] = %d\n", sizeof(static_buffer[0]));
    printf("static_buffer.size() = %u\n", static_buffer.size());
    printf("static_buffer takes %u bytes\n",
    static_buffer.size() * sizeof(static_buffer[0]));

    std::vector<int> dynamic_buffer;
    printf("sizeof(dynamic_buffer) = %d\n", sizeof(dynamic_buffer));
    printf("sizeof(dynamic_buffer[0] = %d\n", sizeof(dynamic_buffer[0]));
    printf("dynamic_buffer.size() = %u\n", dynamic_buffer.size());
    printf("dynamic_buffer takes %u bytes\n",
    dynamic_buffer.size() * sizeof(dynamic_buffer[0]));
    cout << '\n';
}

void hex_dump(const void* address, size_t count)
{
    cout << "count = " << count << '\n';
    auto bytes = reinterpret_cast<const uint8_t*>(address);
    int i;
    for (i = 0; i < count; i++) {
         printf("%02x ", bytes[i]);
    }
    putchar('\n');
}

void demo_alignment() {
    puts(__func__);
    // Показания датчика.
    #pragma pack(push, 8)
    struct Metric {
        uint32_t time;    // время съема, секунд с 00:00:00 01.01.1970 GMT
        char sensor[10];  // имя датчика (до 9 символов и завершающий '\0')
        float value;      // значение показателя
    };
    #pragma pack(pop)

    #pragma pack(push, 1)
    struct Metric2 {
        uint32_t time;    // время съема, секунд с 00:00:00 01.01.1970 GMT
        char sensor[10];  // имя датчика (до 9 символов и завершающий '\0')
        float value;      // значение показателя
    };
    #pragma pack(pop)

    Metric metric;
    metric.time = time(NULL);
    strcpy(metric.sensor, "cpu0");
    metric.value = 59.4f;
    printf("metric.time = %u\n", metric.time);
    printf("metric.sensor = '%s'\n", metric.sensor);
    printf("metric.value = %f\n", metric.value);
    hex_dump(&metric, sizeof(metric));
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&metric);
    Metric2* metric2 = reinterpret_cast<Metric2*>(bytes);
    printf("metric2->time = %u\n", metric2->time);
    printf("metric2->sensor = '%s'\n", metric2->sensor);
    printf("metric2->value = %f\n", metric2->value);
    hex_dump(metric2, sizeof(*metric2));
    cout << '\n';
}

void demo_byte_order(){
    puts(__func__);
    uint16_t host2 = 0x4242;
    printf("host2 = %04x\n", host2);
    hex_dump(&host2, sizeof(host2));
    uint16_t net2 = htons(host2);
    printf("net2 = %04x\n", net2);
    hex_dump(&net2, sizeof(net2));
    uint32_t host4 = 0x94213d3;
    printf("host4 = %04x\n", host4);
    hex_dump(&host4, sizeof(host4));
    uint32_t net4 = htonl(host4);
    printf("net4 = %04x\n", net4);
    hex_dump(&net4, sizeof(net4));
}
