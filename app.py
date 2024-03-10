from flask import Flask, request, render_template
from flask_socketio import SocketIO, emit

app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret!'
socketio = SocketIO(app)


@app.route('/')
def home():
    return render_template('index.html')

@app.route('/sos', methods=['POST'])
def handle_sos_signal():
    print("Recieved SOS Signal!")
    socketio.emit('sos_recieved')

@socketio.on('connect')
def handle_connect():
    print('Client connected')

@socketio.on('acknowledge_signal')
def handle_acknowledge_signal():
    print('SOS Signal Acknowledged!')

@socketio.on('disconnect')
def handle_disconnect():
    print('Client disconnected')
