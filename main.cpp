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

const char* ssid = "Hyperbola";
const char* password = "Rjak232177*";

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
volatile int checking_cancel = 0;
LowPowerTimer cancel_timer;

// Set up flag for which state the board is in depending on its orientation
volatile int position_state = 0;
// Set up flag for whether or not the last orientation of board was vertical
volatile bool vertical_last = 0;
// Set up flag for whether or not the last trigger was high temperature
volatile bool high_temp_last = 0;

// Variables to store info for past acceleration records
const int recordSize = 10;
float accelerationHistory[10];
int currentHistoryIndex = 0; 

volatile bool sudden_acceleration = 0;

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
    else if (temp_high == 0 && sudden_acceleration == 0) {
        message = "SOS: User Triggered";
    }
    else if (temp_high == 0 && sudden_acceleration == 1) {
        message = "SOS: Sudden Acceleration";
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

    sudden_acceleration = 0;
    // We sleep whilst a cancel request is not detected
    while (checking_cancel == 1 and cancel_timer.elapsed_time() < 5s)  {
        ThisThread::sleep_for(1s);
    }

    // If the cancel request came within 5 seconds and the original SOS was sent by the user, we communicate this over WiFi
    if (cancel_timer.elapsed_time() < 5s and checking_cancel == 2) {
        acknowledge = 0;
        // We stop the timer once a cancel request is detected
        cancel_timer.stop();

        message = "User cancelling SOS";
        int sentNum = socket->send(message, strlen(message));
        if (sentNum < 0) {
            printf("Failed to send\n");
        } else {
            printf("Cancellation message sent successfully\n");
        }


        // Reset flags for next iteration
        acknowledge = 0;
        checking_cancel = 0;
        sudden_acceleration = 0;
        send_sig = 0;
    }
    // Otherwise, we go to sleep until the next button press
    acknowledge = 0;
    checking_cancel = 0;
    sudden_acceleration = 0;
    send_sig = 0;
    // Going to sleep now
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
        led_toggle.detach();
    }

    if (send_sig == 1 && checking_cancel == 0) {
        checking_cancel = 1;
        cancel_timer.reset();
        // We start the cancel timer so that we can track after how long a cancel command went out
        cancel_timer.start();
    }

    else if (send_sig == 1 && checking_cancel == 1){
        if (cancel_timer.elapsed_time() < 5s) {
            checking_cancel = 2; // Cancel attempt within 5 seconds
        }
    }
}

// Toggle the LED
void toggle_led() {
    led = !led;
}

// check orientation of device
bool check_device_vertical(){
    int16_t xyz_counts[3] = {0};
    BSP_ACCELERO_AccGetXYZ(xyz_counts);
    int x_threshold = 850;
    return abs(xyz_counts[0]) > x_threshold;
}


// update the latest record
void updateAccelerationHistory(float magnitude) {
    accelerationHistory[currentHistoryIndex] = magnitude;
    currentHistoryIndex = (currentHistoryIndex + 1) % recordSize;
}

// calculate the average acceleration based on last 10 records
float calculateAveragePastAcceleration() {
    float sum = 0;
    for(int i = 0; i < recordSize; i++) {
        sum += accelerationHistory[i];
    }
    return sum / recordSize;
}

// update the acceleration state by checking for sudden changes in comparison to last 10 records
void check_sudden_acceleration() {
    int16_t xyz_counts[3] = {0};

    BSP_ACCELERO_AccGetXYZ(xyz_counts);
    float currentMagnitude = sqrt(xyz_counts[0] * xyz_counts[0] + xyz_counts[1] * xyz_counts[1] + xyz_counts[2] * xyz_counts[2]);

    updateAccelerationHistory(currentMagnitude);

    float averagePastAcceleration = calculateAveragePastAcceleration();

    if (currentMagnitude > averagePastAcceleration + 300) { 
        sudden_acceleration = 1;
    } else {
        sudden_acceleration = 0;
    }
}

// updates the position state of the board
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

    for(int i = 0; i < recordSize; i++) {
        accelerationHistory[i] = 1000;
    }
    
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
        while (send_sig == 0 && sudden_acceleration == 0 && (BSP_TSENSOR_ReadTemp() < 60.0 || high_temp_last == 1)) {
            printf("Sleeping until either user triggers SOS or auto-detected based on temperature\n");
            if (position_state == 4){
                position_state = 0;
            }
            update_position_state();
            check_sudden_acceleration();
            ThisThread::sleep_for(500ms);
        }

        int temp_high = 0;

        // Print what caused an SOS to be triggered
        if (send_sig != 0) {
            printf("User triggered SOS\n");
            high_temp_last = 0;
        }
        else if (sudden_acceleration != 0){
            printf("Sudden acceleration detected\n");
            high_temp_last = 0;
            led_toggle.detach();
        }
        else {
            printf("Temperature reaching unsafe values (> 59°C)\n");
            high_temp_last = 1;
            temp_high = 1;
            led_toggle.detach();
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

        
    }

}
