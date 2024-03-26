#include "DigitalOut.h"
#include "InterruptIn.h"
#include "PinNames.h"
#include "mbed.h"
#include "ISM43362Interface.h"
#include "TCPSocket.h"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_accelero.h"
#include <cstdio>
#include <string>

const char* ssid = "ssid";
const char* password = "password";

ISM43362Interface wifi;
TCPSocket socket;
SocketAddress addr;

// Set up flag and interrupt handler for user input
volatile int send_sig = 0;
InterruptIn button(BUTTON1);

// Set up flag and ticker for LED
volatile int acknowledge = 0;
DigitalOut led(LED1);
LowPowerTicker led_toggle;

// Set up flag and timer for checking if user wants to cancel SOS
volatile int checking_cancel = 1;
LowPowerTimer cancel_timer;

// Set up flag for which state the board is in depending on its orientation
volatile int position_state = 0;
// Set up flag for whether or not the last orientation of board was vertical
volatile bool vertical_last = 0;
// Set up flag for whether or not the last trigger was high temperature
volatile bool high_temp_last = 0;

int connect_to_help(ISM43362Interface *wifi, SocketAddress * addr, TCPSocket * socket, int temp_high, bool cancelling)
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

    // Send a sos message that contains information of what triggered the SOS, or if it is being cancelled
    if (temp_high == 0 && cancelling) {
        message = "User cancelling SOS";
    }
    else if (temp_high == 0) {
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

// Set signal flag to 1 and also set the cancel flag
void signal_interrupt_handler() {
    if (position_state == 2 || position_state == 3){
        send_sig = 1;
    }

    if (checking_cancel == 0) {
        checking_cancel = 1;
    }
}

// Toggle the LED
void toggle_led() {
    led = !led;
}

bool check_device_vertical(){
    int16_t xyz_counts[3] = {0};
    BSP_ACCELERO_AccGetXYZ(xyz_counts);
    int x_threshold = 850;
    return abs(xyz_counts[0]) > x_threshold;
}

void update_position_state(){
    bool curr_vertical = check_device_vertical();
    if (curr_vertical != vertical_last){
        if (curr_vertical && (position_state == 0 || position_state == 2)){
            position_state += 1;
        }
        else if (!curr_vertical && (position_state == 1 || position_state == 3)){
            position_state += 1;
        }
    }
    vertical_last = curr_vertical;
}

int main() {

    // Attach interrupt handler for button
    button.fall(&signal_interrupt_handler);
    // Initialize temperature sensor
    uint32_t temp_init = BSP_TSENSOR_Init();
    // Initialize accelerometer sensor
    uint32_t acc_init = BSP_ACCELERO_Init();
    
    // Print if temperature sensor was successfully initialized
    if (temp_init != TSENSOR_OK) {
        printf("Error initializing temperature sensor\n");
    }
    else {
        printf("Temperature sensor successfully initialized\n");
    }

    // Print if accelerometer sensor was successfully initialized
    if (acc_init != ACCELERO_OK) {
        printf("Error initializing accelerometer sensor\n");
    }
    else {
        printf("Accelerometer sensor successfully initialized\n");
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

    while(true){

        // We sleep for 2 minutes and enter low power mode after which we will recheck the temperature -- allows us to operate in low-power mode
        while (send_sig == 0 && (BSP_TSENSOR_ReadTemp() < 60.0 || high_temp_last == 1)) {
            printf("Sleeping until either user triggers SOS or auto-detected based on temperature\n");
            if (position_state == 4){
                position_state = 0;
            }
            update_position_state();
            ThisThread::sleep_for(2s);
        }

        int temp_high = 0;

        // Print what caused an SOS to be triggered
        if (send_sig != 0) {
            printf("User triggered SOS\n");
            high_temp_last = 0;
        }
        else {
            printf("Temperature reaching unsafe values (> 59°C)\n");
            high_temp_last = 1;
            temp_high = 1;
        }

        int num = connect_to_help(&wifi, &addr, &socket, temp_high, false);
        if (num != 0){
            printf("Connection Failed\n");
            return num;
        }

        // Once we receive acknowledgement we toggle the LED. We use a low power ticker so that we can go to sleep
        if (acknowledge == 1) {
            printf("Toggling LED and going to sleep\n");
            led_toggle.attach(&toggle_led, 1s);
        }

        // We set the flag to check for a cancel request
        checking_cancel = 0;
        // We start the cancel timer so that we can track after how long a cancel command went out
        cancel_timer.start();

        // We sleep whilst a cancel request is not detected
        while (checking_cancel == 0) {
            sleep();
        }

        // We stop the timer once a cancel request is detected
        cancel_timer.stop();

        // If the cancel request came within 10 seconds and the original SOS was sent by the user, we communicate this over WiFi
        if (cancel_timer.elapsed_time() < 10s and send_sig != 0) {
            acknowledge = 0;
            connect_to_help(&wifi, &addr, &socket, temp_high, true);

            // Once acknowledgement is received, we turn the LED off
            if (acknowledge == 1) {
                led = 0;
            }

            // Reset flags for next iteration
            acknowledge = 0;
            checking_cancel = 1;
            send_sig = 0;
        }
        // Otherwise, we go to sleep until the next button press
        else {
            acknowledge = 0;
            checking_cancel = 1;
            send_sig = 0;
            // Going to sleep now
            sleep();
        }
    }

}
