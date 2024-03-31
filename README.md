# Setup Guide

## Step 1: Import the following libaries into the Mbed project

- `wifi-ism43362` from [os.mbed.com/teams/Disco-L475VG-IOT/code/wifi-ism43362/](https://os.mbed.com/teams/Disco-L475VG-IOT/code/wifi-ism43362/)
- `BSP_B-L475E-IOT01` from [os.mbed.com/teams/ST/code/BSP_B-L475E-IOT01/](https://os.mbed.com/teams/ST/code/BSP_B-L475E-IOT01/)

## Step 2: Configure the Wifi settings

1. In `main.cpp`, find the lines `const char* ssid = "ssid";` and `const char* password = "password";` and
   replace the placeholders with your WiFi network's ssid and password.
2. Also in `main.cpp`, find the line that contains the IP address "10.88.111.20", and change it to the IP address of the
   device that will host the Flask server.
3. In `app.py`, find the `HOST` variable and change its value to the same IP address you used in `main.cpp`.

## Step 3: Running the Flask server

Simply run the file `app.py`. Ensure your Flask server is running before proceeding to the next steps.

## Step 4: Flashing the board

Flash the Mbed project onto the board.

## Step 5: Testing

After flashing the board, the device should automatically connect to the WiFi network and start communicating with the Flask server. 
You can check the server and Mbed Studio logs for detailed updates on the connection and interactions.

Additionally, you can connect the board to any other device or a power bank and then press the reset button to test the wireless
connectivity of the board.
