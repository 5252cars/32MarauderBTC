#!/usr/bin/env python3
"""
K5MarauderBTC Control Interface

This script provides a web interface and serial communication to control
the K5MarauderBTC device from a Raspberry Pi 5 running Kali Linux.
"""

import os
import time
import json
import serial
import argparse
import threading
from flask import Flask, render_template, request, jsonify

# Create Flask app
app = Flask(__name__)

# Global variables
serial_port = None
serial_connected = False
device_status = {
    "mode": "unknown",
    "btc_price": 0.0,
    "btc_change": 0.0,
    "wifi_connected": False,
    "last_update": 0
}

def connect_serial(port, baudrate):
    """Connect to the serial port"""
    global serial_port, serial_connected
    try:
        serial_port = serial.Serial(port, baudrate, timeout=1)
        time.sleep(2)  # Wait for connection to establish
        serial_connected = True
        print(f"Connected to {port} at {baudrate} baud")
        return True
    except Exception as e:
        print(f"Failed to connect to {port}: {e}")
        serial_connected = False
        return False

def send_command(command):
    """Send a command to the device"""
    global serial_port, serial_connected
    if not serial_connected:
        return False, "Not connected to device"
    
    try:
        serial_port.write(f"{command}\n".encode())
        time.sleep(0.1)
        response = serial_port.read(serial_port.in_waiting).decode()
        return True, response
    except Exception as e:
        print(f"Failed to send command: {e}")
        return False, str(e)

def update_status():
    """Update the device status"""
    global device_status
    success, response = send_command("status")
    if success:
        try:
            # Parse the JSON response
            status = json.loads(response)
            device_status.update(status)
        except json.JSONDecodeError:
            print(f"Failed to parse status response: {response}")
    
    # Schedule the next update
    threading.Timer(5.0, update_status).start()

@app.route('/')
def index():
    """Render the main control page"""
    return render_template('index.html', status=device_status)

@app.route('/api/status')
def api_status():
    """Return the current device status"""
    return jsonify(device_status)

@app.route('/api/command', methods=['POST'])
def api_command():
    """Send a command to the device"""
    command = request.json.get('command')
    if not command:
        return jsonify({"success": False, "error": "No command provided"}), 400
    
    success, response = send_command(command)
    return jsonify({"success": success, "response": response})

@app.route('/api/mode', methods=['POST'])
def api_mode():
    """Set the device mode"""
    mode = request.json.get('mode')
    if not mode:
        return jsonify({"success": False, "error": "No mode provided"}), 400
    
    success, response = send_command(f"mode {mode}")
    return jsonify({"success": success, "response": response})

def main():
    """Main function"""
    parser = argparse.ArgumentParser(description='K5MarauderBTC Control Interface')
    parser.add_argument('--port', default='/dev/ttyUSB0', help='Serial port')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate')
    parser.add_argument('--host', default='0.0.0.0', help='Host to listen on')
    parser.add_argument('--port-web', type=int, default=8080, help='Port to listen on')
    args = parser.parse_args()
    
    # Connect to the serial port
    if connect_serial(args.port, args.baud):
        # Start the status update thread
        update_status()
        
        # Start the web server
        app.run(host=args.host, port=args.port_web, debug=True)
    else:
        print("Failed to connect to the device. Please check the connection and try again.")

if __name__ == '__main__':
    main()
