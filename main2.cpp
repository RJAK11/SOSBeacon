#include "mbed.h"
#include "ISM43362Interface.h"
#include "TCPSocket.h"

const char* ssid = "ssid";
const char* password = "password";

int connect_to_help(ISM43362Interface *wifi, SocketAddress * addr, TCPSocket * socket)
{
    // Open a socket on the network interface, and create a TCP connection to localhost
    if (socket->open(wifi) != NSAPI_ERROR_OK){
        printf("Socket opening failed\n");
        return -1;
    }

    // localhost port number is 5000
    addr->set_port(5000);
    if (socket->connect(*addr) != NSAPI_ERROR_OK) {
        printf("Failed to connect to server\n");
        return -1;
    }
    printf("Successfully connected. Begininning help packet transfer.\n");

    // Send a sos message
    const char* message = "SOS";
    int sentNum = socket->send(message, strlen(message));
    if (sentNum < 0) {
        printf("Failed to send\n");
    } else {
        printf("Message sent successfully\n");
    }

    char buffer[1024] = {0};
    nsapi_size_or_error_t receivedNum = socket->recv(buffer, sizeof(buffer));
    if (receivedNum < 0) {
        printf("Did not receive acknowledgement\n");
    } else {
        printf("Received acknowledgement!\n");
    }

    socket->close();
    return 0;
}


int main() {
    
    ISM43362Interface wifi;
    TCPSocket socket;
    SocketAddress addr;

    printf("Beginning connection\n");
    printf("Connecting to %s...\n", ssid);

    if (wifi.connect(ssid, password, NSAPI_SECURITY_WPA_WPA2) != 0) {
        printf("Failed to connect to WiFi\n");
        return -1;
    }
    printf("Connected to WiFi\n");

    // Use IP address of server host instead of 10.88.111.9
    if (static_cast<NetworkInterface*>(&wifi)->gethostbyname("10.88.111.9", &addr) != NSAPI_ERROR_OK) {
        printf("DNS failed\n");
        return -1;
    }

    int num = connect_to_help(&wifi, &addr, &socket);
    if (num != 0){
        printf("Connection Failed\n");
        return num;
    }
    
    wifi.disconnect();
    printf("Disconnected\n");

}
