from flask import Flask, request, render_template, jsonify
from flask_socketio import SocketIO, emit

app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret!'
socketio = SocketIO(app, cors_allowed_origins="*")

@app.route('/')
def home():
    return render_template('index.html')

@app.route('/sos', methods=['POST'])
def handle_sos_signal():
    print("Received SOS Signal!")
    socketio.emit('sos_received')
    return jsonify({'status': 'Success'}), 200

@socketio.on('connect')
def handle_connect():
    print('Client connected')

@socketio.on('acknowledge_signal')
def handle_acknowledge_signal(data):
    print('SOS Signal Acknowledged!')
    socketio.emit('acknowledge_signal', {'data': 'SOS Signal Acknowledged'})

@socketio.on('disconnect')
def handle_disconnect():
    print('Client disconnected')
