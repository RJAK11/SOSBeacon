from flask import Flask, jsonify, render_template
from flask_socketio import SocketIO
import threading
import socket

app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret!'
socketio = SocketIO(app, cors_allowed_origins="*")

HOST = '10.88.111.9'
PORT = 5000

# https://realpython.com/python-sockets/
def tcp_server():

    global connection

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        print(f"TCP Server listening on {HOST}:{PORT}")

        while True:
            conn, addr = s.accept()
            with conn:
                connection = conn

                while True:
                    data = conn.recv(1024)
                    if not data:
                        break
                    message = data.decode('utf-8').strip()
                    if message == "SOS":
                        socketio.start_background_task(target=emit_sos_received)

def emit_sos_received():
    socketio.emit('sos_received', {'message': 'SOS signal received'})
    print("SOS event triggered")

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('connect')
def handle_connect():
    print('Client connected')

@socketio.on('disconnect')
def handle_disconnect():
    print('Client disconnected')

@socketio.on('acknowledge_signal')
def handle_acknowledge_signal(data):
    print('SOS Signal Acknowledged!')
    global connection
    if connection:
        acknowledgment = "SOS Acknowledged"
        connection.send(acknowledgment.encode('utf-8'))
        print('Acknowledgment sent to SOS Beacon.')

if __name__ == '__main__':
    threading.Thread(target=tcp_server, daemon=True).start()
    socketio.run(app)
