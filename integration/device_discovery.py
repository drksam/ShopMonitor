"""
Device discovery and management module for ESP32 Machine Monitors
This module handles:
1. Automatic discovery of new ESP32 devices on the network
2. Registration and management of devices
3. OTA firmware updates for ESP32 devices
"""

import socket
import time
import threading
import json
import os
import requests
import logging
from datetime import datetime
from flask import current_app, flash, render_template, request, redirect, url_for, jsonify, Blueprint

try:
    from zeroconf import ServiceBrowser, Zeroconf, ServiceStateChange
    ZEROCONF_AVAILABLE = True
except ImportError:
    ZEROCONF_AVAILABLE = False
    logging.error("zeroconf package not installed. Device discovery will be disabled.")

from models import db, Machine, Zone, Node

# Setup logger
logger = logging.getLogger(__name__)

# Service type for the RFID Machine Monitor
SERVICE_TYPE = "_rfid-monitor._tcp.local."

# Directory for firmware storage
FIRMWARE_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "firmware")
if not os.path.exists(FIRMWARE_DIR):
    os.makedirs(FIRMWARE_DIR)

# Create blueprint for device discovery routes
device_bp = Blueprint('devices', __name__, url_prefix='/devices')

# Global list of discovered devices
discovered_devices = {}

# Lock for thread-safe access to discovered_devices
discovery_lock = threading.Lock()
discovery_lock = threading.Lock()

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class DeviceManager:
    """
    Manages ESP32 RFID Machine Monitor devices on the network
    """
    def __init__(self, app=None):
        self.app = app
        self.zeroconf = None
        self.browser = None
        self.devices = {}
        self.discovery_thread = None
        self.is_running = False
        
        if app is not None:
            self.init_app(app)
    
    def init_app(self, app):
        """Initialize with Flask app context"""
        self.app = app
        
        # Don't register routes here - they're handled in routes.py
        # The routes.py file will call these methods directly
        
        # Start discovery on app startup
        # Use with_app_context pattern instead of before_first_request (removed in Flask 2.0+)
        self.start_discovery()
        
        # Create an endpoint to manually start/stop discovery if needed
        @app.route('/api/device_discovery/start', methods=['POST'])
        def start_device_discovery():
            self.start_discovery()
            return jsonify({"status": "Discovery service started"}), 200
            
        @app.route('/api/device_discovery/stop', methods=['POST'])
        def stop_device_discovery():
            self.stop_discovery()
            return jsonify({"status": "Discovery service stopped"}), 200

    def start_discovery(self):
        """Start the mDNS discovery service"""
        if self.is_running:
            return
            
        self.is_running = True
        self.discovery_thread = threading.Thread(target=self._discovery_worker, daemon=True)
        self.discovery_thread.start()
        logger.info("mDNS discovery service started")
    
    def stop_discovery(self):
        """Stop the mDNS discovery service"""
        if not self.is_running:
            return
            
        self.is_running = False
        if self.zeroconf:
            self.zeroconf.close()
            self.zeroconf = None
        if self.discovery_thread:
            self.discovery_thread.join(timeout=1.0)
        logger.info("mDNS discovery service stopped")
    
    def _discovery_worker(self):
        """Background worker for device discovery"""
        self.zeroconf = Zeroconf()
        
        def on_service_state_change(zeroconf, service_type, name, state_change):
            global discovered_devices
            
            if state_change is ServiceStateChange.Added:
                info = zeroconf.get_service_info(service_type, name)
                if info:
                    with discovery_lock:
                        try:
                            device_name = info.server.split('.')[0]
                            # Get the first IPv4 address from the addresses list
                            device_ip = socket.inet_ntoa(info.addresses[0]) if hasattr(info, 'addresses') and info.addresses else '0.0.0.0'
                            device_port = info.port
                            
                            # Extract properties from TXT records
                            properties = {}
                            if hasattr(info, 'properties') and info.properties:
                                for key, value in info.properties.items():
                                    try:
                                        if isinstance(key, bytes):
                                            key = key.decode('utf-8')
                                        if isinstance(value, bytes):
                                            value = value.decode('utf-8')
                                        properties[key] = value
                                    except:
                                        logger.warning(f"Could not decode property {key}={value}")
                            
                            # Use MAC as device ID if available, fall back to device name
                            device_id = properties.get('mac', device_name)
                            
                            discovered_devices[device_id] = {
                                'id': device_id,
                                'name': properties.get('name', device_name),
                                'ip': device_ip,
                                'port': device_port,
                                'location': properties.get('location', 'Unknown'),
                                'zone_count': int(properties.get('zone_count', '4')),
                                'last_seen': datetime.now(),
                                'status': 'online',
                                'machines': [],
                                'node_type': properties.get('node_type', 'machine_monitor')
                            }
                            
                            # Extract machine IDs
                            for i in range(int(properties.get('zone_count', '4'))):
                                machine_id = properties.get(f'machine{i}', f'{i+1:02d}')
                                discovered_devices[device_id]['machines'].append(machine_id)
                            
                            logger.info(f"Device discovered: {device_name} at {device_ip}:{device_port}")
                        except Exception as e:
                            logger.error(f"Error processing discovered device: {e}")
            
            elif state_change is ServiceStateChange.Removed:
                info = zeroconf.get_service_info(service_type, name)
                if info:
                    device_name = info.server.split('.')[0]
                    with discovery_lock:
                        # Mark device as offline rather than removing it
                        for device_id, device in discovered_devices.items():
                            if device['name'] == device_name:
                                device['status'] = 'offline'
                                device['last_seen'] = datetime.now()
                                logger.info(f"Device offline: {device_name}")
                                break
        
        self.browser = ServiceBrowser(self.zeroconf, SERVICE_TYPE, handlers=[on_service_state_change])
        
        # Keep the thread running
        while self.is_running:
            time.sleep(1)
    
    def devices_list(self):
        """Handle route for listing all discovered devices"""
        global discovered_devices
        with discovery_lock:
            devices = list(discovered_devices.values())
        
        # Match discovered devices with database records
        with self.app.app_context():
            for device in devices:
                for machine_id in device['machines']:
                    machine = Machine.query.filter_by(machine_id=machine_id).first()
                    if machine:
                        device['has_db_record'] = True
                        break
                else:
                    device['has_db_record'] = False
        
        return render_template('devices/device_list.html', devices=devices)
    
    def device_detail(self, device_id):
        """Handle route for showing device details"""
        global discovered_devices
        
        with discovery_lock:
            device = discovered_devices.get(device_id)
            
        if not device:
            flash("Device not found", "danger")
            return redirect(url_for('devices_list'))
        
        # Get device status
        if device['status'] == 'online':
            try:
                response = requests.get(f"http://{device['ip']}/api/status", timeout=2)
                if response.status_code == 200:
                    device['details'] = response.json()
                else:
                    device['details'] = {"error": "Could not retrieve device details"}
            except Exception as e:
                device['details'] = {"error": f"Connection error: {str(e)}"}
                device['status'] = 'unreachable'
        
        # Get matching machines from database
        with self.app.app_context():
            device_machines = []
            for machine_id in device['machines']:
                machine = Machine.query.filter_by(machine_id=machine_id).first()
                if machine:
                    zone = Zone.query.get(machine.zone_id) if machine.zone_id else None
                    device_machines.append({
                        'id': machine.id,
                        'machine_id': machine.machine_id,
                        'name': machine.name,
                        'zone': zone.name if zone else "No Zone",
                        'ip_address': machine.ip_address
                    })
                else:
                    device_machines.append({
                        'machine_id': machine_id,
                        'name': f"Unregistered Machine {machine_id}",
                        'zone': "No Zone",
                        'ip_address': None
                    })
            
            device['db_machines'] = device_machines
        
        # Get firmware files for update
        firmware_files = []
        for filename in os.listdir(FIRMWARE_DIR):
            if filename.endswith('.bin'):
                firmware_files.append(filename)
        
        return render_template('devices/device_detail.html', 
                               device=device, 
                               firmware_files=firmware_files)
    
    def device_update(self, device_id):
        """Handle route for updating device firmware"""
        global discovered_devices
        
        with discovery_lock:
            device = discovered_devices.get(device_id)
            
        if not device:
            flash("Device not found", "danger")
            return redirect(url_for('devices_list'))
        
        if request.method == 'POST':
            # Handle firmware upload
            if 'firmware' in request.files:
                firmware_file = request.files['firmware']
                if firmware_file.filename != '':
                    firmware_filename = f"{datetime.now().strftime('%Y%m%d%H%M%S')}_{firmware_file.filename}"
                    firmware_path = os.path.join(FIRMWARE_DIR, firmware_filename)
                    firmware_file.save(firmware_path)
                    flash(f"Firmware {firmware_filename} uploaded successfully", "success")
                    
                    # Deploy to device if requested
                    if request.form.get('deploy_now') == 'yes':
                        return self._deploy_firmware(device, firmware_path)
            
            # Handle firmware selection for deployment
            elif 'selected_firmware' in request.form:
                selected_firmware = request.form['selected_firmware']
                firmware_path = os.path.join(FIRMWARE_DIR, selected_firmware)
                if os.path.exists(firmware_path):
                    return self._deploy_firmware(device, firmware_path)
                else:
                    flash("Selected firmware file not found", "danger")
        
        # Redirect back to device detail page
        return redirect(url_for('device_detail', device_id=device_id))
    
    def _deploy_firmware(self, device, firmware_path):
        """Deploy firmware to device"""
        try:
            # Get the ESP32 IP address
            device_ip = device['ip']
            
            # For AsyncElegantOTA, we need to upload to /update endpoint
            files = {'file': open(firmware_path, 'rb')}
            update_url = f"http://{device_ip}/update"
            
            # Send update request
            response = requests.post(update_url, files=files, timeout=60)
            
            if response.status_code == 200:
                flash(f"Firmware update sent to {device['name']}. Device is rebooting...", "success")
            else:
                flash(f"Firmware update failed: {response.text}", "danger")
        except Exception as e:
            flash(f"Update error: {str(e)}", "danger")
        
        return redirect(url_for('device_detail', device_id=device['id']))
    
    def register_node(self):
        """API endpoint for direct device registration"""
        # Get device information from request
        name = request.form.get('name', 'Unknown Device')
        location = request.form.get('location', 'Unknown Location')
        mac = request.form.get('mac')
        ip = request.form.get('ip')
        zone_count = int(request.form.get('zone_count', 4))
        
        if not mac or not ip:
            return "Missing required information", 400
        
        # Add device to discovered devices
        global discovered_devices
        with discovery_lock:
            discovered_devices[mac] = {
                'id': mac,
                'name': name,
                'ip': ip,
                'port': 80,
                'location': location,
                'zone_count': zone_count,
                'last_seen': datetime.now(),
                'status': 'online',
                'machines': []
            }
            
            # Extract machine IDs
            for i in range(zone_count):
                machine_id = request.form.get(f'machine{i}', f'{i+1:02d}')
                discovered_devices[mac]['machines'].append(machine_id)
        
        logger.info(f"Device registered via API: {name} at {ip}")
        
        # Check if any machines need to be updated in the database
        with self.app.app_context():
            for machine_id in discovered_devices[mac]['machines']:
                machine = Machine.query.filter_by(machine_id=machine_id).first()
                if machine:
                    # Update the machine's IP address
                    machine.ip_address = ip
                    db.session.commit()
                    logger.info(f"Updated database record for machine {machine_id}")
        
        return "Device registered successfully", 200

# Create the device manager instance
device_manager = DeviceManager()