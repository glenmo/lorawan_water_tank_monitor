#!/usr/bin/env python3
"""
Water Tank Monitor - LoRaWAN Web Server
Subscribes to ChirpStack MQTT and displays data on webpage
"""

from flask import Flask, render_template, jsonify
import paho.mqtt.client as mqtt
import json
import struct
import base64
from datetime import datetime
import threading

app = Flask(__name__)

# MQTT Configuration
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC = "application/+/device/+/event/up"
DEVICE_EUI = "a84041d111896c86"

# Current data storage
current_data = {
    'tank_level': 0.0,
    'voltage': 0.0,
    'rssi': 0,
    'snr': 0,
    'timestamp': None,
    'status': 'waiting',
    'frame_count': 0,
    'history': []
}

def decode_payload(data_string):
    """Decode base64 payload from ChirpStack"""
    try:
        # Decode from base64
        data_bytes = base64.b64decode(data_string)
        
        if len(data_bytes) >= 2:
            # Unpack as big-endian unsigned short
            level_scaled = struct.unpack('>H', data_bytes[:2])[0]
            tank_level = level_scaled / 100.0
            return tank_level
        else:
            return None
    except Exception as e:
        print(f"Error decoding payload: {e}")
        return None

def on_connect(client, userdata, flags, rc):
    """Callback when connected to MQTT broker"""
    if rc == 0:
        print("Connected to MQTT broker successfully!")
        client.subscribe(MQTT_TOPIC)
        print(f"Subscribed to: {MQTT_TOPIC}")
    else:
        print(f"Connection failed with code {rc}")

def on_message(client, userdata, msg):
    """Callback when MQTT message received"""
    try:
        payload = json.loads(msg.payload.decode())
        
        # Extract device info
        dev_eui = payload.get('deviceInfo', {}).get('devEui', '').lower()
        
        # Only process our device
        if dev_eui != DEVICE_EUI:
            return
        
        # Extract data
        data_b64 = payload.get('data', '')
        tank_level = decode_payload(data_b64)
        
        if tank_level is None:
            return
        
        # Get signal info
        rx_info = payload.get('rxInfo', [{}])[0]
        rssi = rx_info.get('rssi', 0)
        snr = rx_info.get('snr', 0)
        
        # Get frame counter
        f_cnt = payload.get('fCnt', 0)
        
        # Calculate voltage from tank level (reverse of encoding)
        # tank_level = ((voltage - 0.5) / (1.44 - 0.5)) * 100
        # voltage = (tank_level / 100) * (1.44 - 0.5) + 0.5
        voltage = (tank_level / 100.0) * 0.94 + 0.5
        
        # Update current data
        current_data['tank_level'] = tank_level
        current_data['voltage'] = voltage
        current_data['rssi'] = rssi
        current_data['snr'] = snr
        current_data['timestamp'] = datetime.now().isoformat()
        current_data['status'] = 'online'
        current_data['frame_count'] = f_cnt
        
        # Add to history (keep last 100)
        current_data['history'].append({
            'tank_level': tank_level,
            'voltage': voltage,
            'rssi': rssi,
            'snr': snr,
            'timestamp': current_data['timestamp']
        })
        
        if len(current_data['history']) > 100:
            current_data['history'] = current_data['history'][-100:]
        
        print(f"[{current_data['timestamp']}] Tank: {tank_level:.1f}%, RSSI: {rssi} dBm, SNR: {snr} dB, Frame: {f_cnt}")
        
    except Exception as e:
        print(f"Error processing message: {e}")

def mqtt_thread():
    """Run MQTT client in background thread"""
    client = mqtt.Client(client_id="lorawan_web_monitor")
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_forever()
    except Exception as e:
        print(f"MQTT Error: {e}")

@app.route('/')
def index():
    """Main dashboard page"""
    return render_template('index.html')

@app.route('/api/tank-data')
def tank_data():
    """API endpoint to retrieve current tank data"""
    return jsonify(current_data)

@app.route('/api/history')
def get_history():
    """Get historical data"""
    return jsonify(current_data.get('history', []))

if __name__ == '__main__':
    print("=" * 50)
    print("Water Tank Monitor - LoRaWAN Web Server")
    print("=" * 50)
    print("Starting MQTT subscriber thread...")
    
    # Start MQTT in background thread
    mqtt_bg = threading.Thread(target=mqtt_thread, daemon=True)
    mqtt_bg.start()
    
    print("Starting web server...")
    print(f"Access dashboard at: http://192.168.55.192:5002")
    print("=" * 50)
    
    # Run Flask server
    app.run(host='0.0.0.0', port=5002, debug=False)
