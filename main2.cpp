#include "DigitalOut.h"
#include "InterruptIn.h"
#include "PinNames.h"
#include "mbed.h"
#include "ISM43362Interface.h"
#include "TCPSocket.h"
#include "stm32l475e_iot01_tsensor.h"
#include <cstdio>
#include <string>

const char* ssid = "ssid";
const char* password = "password";

// Set up flag and interrupt handler for user input
volatile int send_sig = 0;
InterruptIn button(BUTTON1);

// Set up flag and ticker for LED
volatile int acknowledge = 0;
DigitalOut led(LED1);
LowPowerTicker led_toggle;

int connect_to_help(ISM43362Interface *wifi, SocketAddress * addr, TCPSocket * socket, float temp)
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

    const char* message;

    // Send a sos message that contains information of what triggered the SOS
    if (temp < 60.0) {
        message = "SOS: User Triggered";
    }
    else {
        message = "SOS: Temperature Triggered";
    }
    
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
        acknowledge = 1;
    }

    socket->close();
    return 0;
}

// Set signal flag to 1
void signal_interrupt_handler() {
    send_sig = 1;
}

// Toggle the LED
void toggle_led() {
    led = !led;
}

int main() {
    
    ISM43362Interface wifi;
    TCPSocket socket;
    SocketAddress addr;

    // Attach interrupt handler for button
    button.fall(&signal_interrupt_handler);
    // Initialize temperature sensor
    uint32_t temp_init = BSP_TSENSOR_Init();
    
    // Print if temperature sensor was successfully initialized
    if (temp_init != TSENSOR_OK) {
        printf("Error initializing temperature sensor\n");
    }
    else {
        printf("Temperature sensor successfully initialized");
    }

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

    // We sleep for 2 minutes and enter low power mode after which we will recheck the temperature -- allows us to operate in low-power mode
    while (send_sig == 0 && BSP_TSENSOR_ReadTemp() < 60.0) {
        printf("Sleeping until either user triggers SOS or auto-detected based on temperature");
        ThisThread::sleep_for(120s);
    }

    // Print what caused an SOS to be triggered
    if (send_sig != 0) {
        printf("User triggered SOS\n");
    }
    else {
        printf("Temperature reaching unsafe values (> 59°C)\n");
    }

    int num = connect_to_help(&wifi, &addr, &socket, BSP_TSENSOR_ReadTemp());
    if (num != 0){
        printf("Connection Failed\n");
        return num;
    }

    // Once we receive acknowledgement we toggle the LED. We use a low power ticker so that we can go to sleep
    if (acknowledge == 1) {
        printf("Toggling LED and going to sleep\n");
        led_toggle.attach(&toggle_led, 2s);
    }

    // Going to sleep now
    sleep();
    
    wifi.disconnect();
    printf("Disconnected\n");

}
