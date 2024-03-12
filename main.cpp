#include "mbed.h"

WiFiInterface *wifi;

int scan_count(WiFiInterface *wifi)
{
    WiFiAccessPoint *ap;

    printf("Scan:\n");

    int count = wifi->scan(NULL,0);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    count = wifi->scan(ap, count);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    return count;
}

void connect_to_help(NetworkInterface *net)
{
    // Open a socket on the network interface, and create a TCP connection to localhost
    TCPSocket socket;
    socket.open(net);

    SocketAddress a;
    net->gethostbyname("localhost", &a);
    // localhost port number is 80
    a.set_port(80);
    socket.connect(a);
    // Send a http buffer
    char sbuffer[] = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int scount = socket.send(sbuffer, sizeof sbuffer);
    printf("Sent message");

    // Recieve message
    char rbuffer[64];
    int rcount = socket.recv(rbuffer, sizeof rbuffer);
    printf("Received message");

    // Close the socket to return its memory and bring down the network interface
    socket.close();
}

int main()
{
    printf("Beginning connection\n");

    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    int count = scan_count(wifi);
    if (count == 0) {
        printf("No WIFI APs found - can't continue further.\n");
        return -1;
    }

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    printf("Successfully connected. Begininning help packet transfer.\n\n");

    connect_to_help(wifi);

    wifi->disconnect();

    printf("\nDisconnected\n");
}
