#!/usr/bin/env python3
"""
Water Tank Monitor - Web Server
Receives data from Arduino UNO R4 WiFi and displays on webpage
"""

from flask import Flask, render_template, jsonify, request
from datetime import datetime
import json
import os

app = Flask(__name__)

# Data storage
DATA_FILE = '/home/glen/tank_data.json'
current_data = {
    'tank_level': 0.0,
    'voltage': 0.0,
    'rssi': 0,
    'timestamp': None,
    'status': 'waiting',
    'history': []
}

# Load existing data if available
if os.path.exists(DATA_FILE):
    try:
        with open(DATA_FILE, 'r') as f:
            current_data = json.load(f)
    except:
        pass

def save_data():
    """Save current data to file"""
    with open(DATA_FILE, 'w') as f:
        json.dump(current_data, f, indent=2)

@app.route('/')
def index():
    """Main dashboard page"""
    return render_template('index.html')

@app.route('/api/tank-data', methods=['POST', 'GET'])
def tank_data():
    """API endpoint to receive and retrieve tank data"""
    
    if request.method == 'POST':
        # Receive data from Arduino
        try:
            data = request.get_json()
            
            tank_level = float(data.get('tank_level', 0))
            voltage = float(data.get('voltage', 0))
            rssi = int(data.get('rssi', 0))
            
            # Update current data
            current_data['tank_level'] = tank_level
            current_data['voltage'] = voltage
            current_data['rssi'] = rssi
            current_data['timestamp'] = datetime.now().isoformat()
            current_data['status'] = 'online'
            
            # Add to history (keep last 100 readings)
            current_data['history'].append({
                'tank_level': tank_level,
                'voltage': voltage,
                'rssi': rssi,
                'timestamp': current_data['timestamp']
            })
            
            if len(current_data['history']) > 100:
                current_data['history'] = current_data['history'][-100:]
            
            # Save to file
            save_data()
            
            print(f"[{current_data['timestamp']}] Tank: {tank_level:.1f}%, Voltage: {voltage:.2f}V, RSSI: {rssi} dBm")
            
            return jsonify({'status': 'success', 'message': 'Data received'}), 200
            
        except Exception as e:
            print(f"Error receiving data: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 400
    
    else:
        # Return current data for webpage
        return jsonify(current_data), 200

@app.route('/api/history')
def get_history():
    """Get historical data"""
    return jsonify(current_data.get('history', [])), 200

if __name__ == '__main__':
    print("=" * 50)
    print("Water Tank Monitor - Web Server Starting...")
    print("=" * 50)
    print(f"Access dashboard at: http://192.168.55.192:5001")
    print(f"Data file: {DATA_FILE}")
    print("=" * 50)
    
    # Run server
    app.run(host='0.0.0.0', port=5001, debug=True)
