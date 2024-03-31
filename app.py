from flask import Flask, jsonify, render_template
from flask_socketio import SocketIO
import threading
import socket

# Initialize Flask app
app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret!'
socketio = SocketIO(app, cors_allowed_origins="*")

# Configure TCP server host and port
HOST = '10.88.111.20'
PORT = 5000

# Run the TCP server
# TCP server code was created with help from: https://realpython.com/python-sockets/
def tcp_server():

    global connection

    # Create socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        # Bind socket and listen
        s.bind((HOST, PORT))
        s.listen()
        print(f"TCP Server listening on {HOST}:{PORT}")

        # Accept any conections 
        while True:
            conn, addr = s.accept()
            with conn:
                connection = conn

                # Wait to recieve data from the connection 
                while True:
                    data = conn.recv(1024)
                    if not data:
                        break
                    message = data.decode('utf-8').strip()
                    # Check message and emit the correct SOS signal using asynchronous execution
                    if message == "SOS: User Triggered":
                        socketio.start_background_task(target=emit_sos_received)
                    if message == "SOS: Temperature Triggered":
                        socketio.start_background_task(target=emit_sos_received_temp)
                    if message == "SOS: Sudden Acceleration":
                        socketio.start_background_task(target=emit_sos_acceleration)
                    if message == "User cancelling SOS":
                        socketio.start_background_task(target=emit_sos_cancellation)

# Emit the correct signal 
def emit_sos_received():
    socketio.emit('sos_received', {'message': 'SOS signal received'})
    print("SOS: User Triggered")

def emit_sos_received_temp():
    socketio.emit('sos_received_temp', {'message': 'SOS signal received due to temperature'})
    print("SOS: Temperature Triggered")

def emit_sos_acceleration():
    socketio.emit('emit_sos_acceleration', {'message': 'SOS signal due to acceleration'})
    print("SOS: Sudden Acceleration")

def emit_sos_cancellation():
    socketio.emit('emit_sos_cancellation', {'message': 'SOS signal cancelled'})
    print("User cancelling SOS")

# Route for the index page
@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('connect')
def handle_connect():
    print('Client connected')

@socketio.on('disconnect')
def handle_disconnect():
    print('Client disconnected')

# Acknowledge the message
@socketio.on('acknowledge_signal')
def handle_acknowledge_signal(data):
    print('SOS Signal Acknowledged!')
    global connection
    if connection:
        acknowledgment = "SOS Acknowledged"
        connection.send(acknowledgment.encode('utf-8'))
        print('Acknowledgment sent to SOS Beacon.')

if __name__ == '__main__':
    # Create another thread to run the TCP server concurrently
    server_thread = threading.Thread(target=tcp_server, daemon=True)
    server_thread.start()
    socketio.run(app)
