#include <Arduino.h>
#include "ConfigManager.h"
#include "NetworkManager.h" 
#include "RFIDHandler.h"
#include "WebUI.h"
#include "constants.h"

// Define the HTML content in the CPP file instead of header
const char* DASHBOARD_HTML = R"rawliteral(<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Shop Node Configuration</title>
  <style>
    body { 
      font-family: Arial, sans-serif; 
      margin: 0; 
      padding: 20px; 
      color: #333; 
      background-color: #f8f9fa; 
    }
    
    h1, h2 { 
      color: #0066cc; 
    }
    
    .container { 
      max-width: 800px; 
      margin: 0 auto; 
      background: white; 
      padding: 20px; 
      border-radius: 8px; 
      box-shadow: 0 2px 4px rgba(0,0,0,0.1); 
    }
    
    .header { 
      text-align: center; 
      margin-bottom: 20px; 
      padding-bottom: 10px; 
      border-bottom: 1px solid #eee; 
    }
    
    .logo { 
      font-size: 24px; 
      font-weight: bold; 
    }
    
    .status { 
      background: #e9f7ef; 
      padding: 15px; 
      border-radius: 5px; 
      margin-bottom: 20px; 
    }
    
    .status.error { 
      background: #f8d7da; 
    }
    
    .form-group { 
      margin-bottom: 15px; 
    }
    
    label { 
      display: block; 
      margin-bottom: 5px; 
      font-weight: bold; 
    }
    
    select, input { 
      width: 100%; 
      padding: 8px; 
      border: 1px solid #ddd; 
      border-radius: 4px; 
      box-sizing: border-box; 
    }
    
    button { 
      background-color: #0066cc; 
      color: white; 
      border: none; 
      padding: 10px 15px; 
      border-radius: 4px; 
      cursor: pointer; 
    }
    
    button:hover { 
      background-color: #0056b3; 
    }
    
    .footer { 
      margin-top: 30px; 
      text-align: center; 
      font-size: 0.8em; 
      color: #777; 
    }
    
    .networks { 
      margin-top: 10px; 
      max-height: 200px; 
      overflow-y: auto; 
      border: 1px solid #ddd; 
      border-radius: 4px; 
    }
    
    .network-item { 
      padding: 8px; 
      cursor: pointer; 
    }
    
    .network-item:hover { 
      background-color: #f5f5f5; 
    }
    
    .network-item.selected { 
      background-color: #e6f2ff; 
    }
    
    table { 
      width: 100%; 
      border-collapse: collapse; 
      margin: 20px 0; 
    }
    
    th, td { 
      padding: 10px; 
      border: 1px solid #ddd; 
      text-align: left; 
    }
    
    th { 
      background: #f2f2f2; 
    }
    
    .tab {
      overflow: hidden;
      border: 1px solid #ccc;
      background-color: #f1f1f1;
      border-radius: 4px 4px 0 0;
    }
    
    .tab button {
      background-color: inherit;
      float: left;
      border: none;
      outline: none;
      cursor: pointer;
      padding: 14px 16px;
      transition: 0.3s;
      color: #333;
    }
    
    .tab button:hover {
      background-color: #ddd;
    }
    
    .tab button.active {
      background-color: #0066cc;
      color: white;
    }
    
    .tabcontent {
      display: none;
      padding: 20px;
      border: 1px solid #ccc;
      border-top: none;
      border-radius: 0 0 4px 4px;
    }
    
    #configTab {
      display: block;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div class="logo">Shop Node Configuration</div>
    </div>
    
    <div class="status" id="statusBox">
      <h2>Node Status</h2>
      <p><strong>Node ID:</strong> <span id="nodeId">Loading...</span></p>
      <p><strong>Node Type:</strong> <span id="nodeType">Loading...</span></p>
      <p><strong>IP Address:</strong> <span id="ipAddress">Loading...</span></p>
      <p><strong>WiFi Signal:</strong> <span id="wifiSignal">Loading...</span></p>
      <p><strong>Uptime:</strong> <span id="uptime">Loading...</span></p>
      <p><strong>Server Connection:</strong> <span id="serverConnection">Loading...</span></p>
    </div>
    
    <div class="tab">
      <button class="tablinks active" onclick="openTab(event, 'configTab')">Configuration</button>
      <button class="tablinks" onclick="openTab(event, 'statusTab')">Status</button>
      <button class="tablinks" onclick="openTab(event, 'maintenanceTab')">Maintenance</button>
    </div>
    
    <div id="configTab" class="tabcontent" style="display: block;">
      <h2>Node Configuration</h2>
      <form id="configForm">
        <div class="form-group">
          <label for="nodeName">Node Name:</label>
          <input type="text" id="nodeName" name="nodeName" required>
        </div>
        
        <div class="form-group">
          <label for="nodeType">Node Type:</label>
          <select id="nodeType" name="nodeType" onchange="updateFormFields()">
            <option value="0">Machine Monitor</option>
            <option value="1">Office RFID Reader</option>
            <option value="2">Accessory IO Controller</option>
          </select>
        </div>
        
        <div class="form-group">
          <label for="serverUrl">Server URL:</label>
          <input type="url" id="serverUrl" name="serverUrl" required placeholder="http://server-ip:port">
        </div>
        
        <div class="form-group">
          <label for="wifiSSID">WiFi Network:</label>
          <input type="text" id="wifiSSID" name="wifiSSID" required>
          <button type="button" onclick="scanWiFi()" style="margin-top: 5px; width: auto;">Scan Networks</button>
          <div class="networks" id="networkList" style="display: none;"></div>
        </div>
        
        <div class="form-group">
          <label for="wifiPass">WiFi Password:</label>
          <input type="password" id="wifiPass" name="wifiPass">
        </div>
        
        <div id="machineConfig">
          <h3>Machine Configuration</h3>
          <div class="form-group">
            <label for="machine0">Machine ID (Zone 0):</label>
            <input type="text" id="machine0" name="machine0" maxlength="2" placeholder="01">
          </div>
          
          <div class="form-group">
            <label for="machine1">Machine ID (Zone 1):</label>
            <input type="text" id="machine1" name="machine1" maxlength="2" placeholder="02">
          </div>
          
          <div class="form-group">
            <label for="machine2">Machine ID (Zone 2):</label>
            <input type="text" id="machine2" name="machine2" maxlength="2" placeholder="03">
          </div>
          
          <div class="form-group">
            <label for="machine3">Machine ID (Zone 3):</label>
            <input type="text" id="machine3" name="machine3" maxlength="2" placeholder="04">
          </div>
        </div>
        
        <button type="submit">Save Configuration</button>
      </form>
    </div>
    
    <div id="statusTab" class="tabcontent">
      <h2>Detailed Status</h2>
      
      <div id="machineStatus" style="display:none;">
        <h3>Machine Status</h3>
        <table id="machineTable">
          <thead>
            <tr>
              <th>Zone</th>
              <th>Machine ID</th>
              <th>Status</th>
              <th>Current User</th>
              <th>Activity Count</th>
            </tr>
          </thead>
          <tbody id="machineTableBody">
          </tbody>
        </table>
      </div>
      
      <div id="officeReaderStatus" style="display:none;">
        <h3>Office Reader Status</h3>
        <p><strong>Last Scanned Tag:</strong> <span id="lastTag">None</span></p>
        <p><strong>Scan Time:</strong> <span id="scanTime">N/A</span></p>
      </div>
      
      <div id="accessoryIOStatus" style="display:none;">
        <h3>Accessory IO Status</h3>
        <table id="ioTable">
          <thead>
            <tr>
              <th>IO</th>
              <th>Type</th>
              <th>State</th>
              <th>Control</th>
            </tr>
          </thead>
          <tbody id="ioTableBody">
          </tbody>
        </table>
      </div>
    </div>
    
    <div id="maintenanceTab" class="tabcontent">
      <h2>System Maintenance</h2>
      <div style="margin-bottom: 20px;">
        <h3>Updates & Reboot</h3>
        <button id="rebootBtn" onclick="rebootDevice()">Reboot Device</button>
        <button id="updateBtn" onclick="window.location.href='/wiring'">Wiring Diagram</button>
      </div>
      
      <div style="margin-bottom: 20px;">
        <h3>Factory Reset</h3>
        <p>This will erase all configuration and return the device to initial setup state.</p>
        <button id="resetBtn" onclick="return confirm('WARNING: This will reset all settings to default. Continue?') && factoryReset()" style="background-color: #dc3545;">Factory Reset</button>
      </div>
    </div>
    
    <div class="footer">
      <p>ESP32 Multi-Function Node v1.0.0 | &copy; 2025 Shop</p>
    </div>
  </div>

  <script>
    // Load configuration on page load
    window.onload = function() {
      fetchStatus();
      fetchConfig();
      
      // Setup form submission
      document.getElementById('configForm').addEventListener('submit', function(e) {
        e.preventDefault();
        saveConfig();
      });
      
      // Refresh status every 5 seconds
      setInterval(fetchStatus, 5000);
    };
    
    // Fetch node status from API
    function fetchStatus() {
      fetch('/api/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('nodeId').textContent = data.node_id;
          document.getElementById('nodeType').textContent = getNodeTypeName(data.node_type);
          document.getElementById('ipAddress').textContent = data.ip_address;
          document.getElementById('wifiSignal').textContent = data.wifi_signal + ' dBm';
          document.getElementById('uptime').textContent = formatUptime(data.uptime);
          document.getElementById('serverConnection').textContent = data.server_connected ? 'Connected' : 'Disconnected';
          
          // Node-specific status
          updateNodeSpecificStatus(data);
        })
        .catch(error => {
          console.error('Error fetching status:', error);
        });
    }
    
    // Fetch configuration from API
    function fetchConfig() {
      fetch('/api/config')
        .then(response => response.json())
        .then(data => {
          document.getElementById('nodeName').value = data.nodeName;
          document.getElementById('nodeType').value = data.nodeType;
          document.getElementById('serverUrl').value = data.serverUrl;
          document.getElementById('wifiSSID').value = data.wifiSSID;
          document.getElementById('machine0').value = data.machine0;
          document.getElementById('machine1').value = data.machine1;
          document.getElementById('machine2').value = data.machine2;
          document.getElementById('machine3').value = data.machine3;
          
          updateFormFields();
        })
        .catch(error => {
          console.error('Error fetching config:', error);
        });
    }
    
    // Save configuration
    function saveConfig() {
      const formData = new FormData(document.getElementById('configForm'));
      const jsonData = {};
      
      formData.forEach((value, key) => {
        jsonData[key] = value;
      });
      
      fetch('/api/config', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(jsonData)
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          alert('Configuration saved successfully. The device will now reboot.');
          setTimeout(() => {
            window.location.href = '/api/reboot';
          }, 1000);
        } else {
          alert('Error: ' + data.message);
        }
      })
      .catch(error => {
        alert('Error saving configuration: ' + error);
      });
    }
    
    // Reboot device
    function rebootDevice() {
      if (confirm('Are you sure you want to reboot the device?')) {
        fetch('/api/reboot')
          .then(() => {
            alert('Device is rebooting. This page will refresh in 30 seconds.');
            setTimeout(() => {
              window.location.reload();
            }, 30000);
          })
          .catch(error => {
            console.error('Error rebooting device:', error);
          });
      }
    }
    
    // Factory reset
    function factoryReset() {
      fetch('/api/reset')
        .then(() => {
          alert('Device is resetting to factory defaults. This page will refresh in 30 seconds.');
          setTimeout(() => {
            window.location.reload();
          }, 30000);
        })
        .catch(error => {
          console.error('Error resetting device:', error);
        });
      return false;
    }
    
    // Open tab
    function openTab(evt, tabName) {
      const tabcontent = document.getElementsByClassName("tabcontent");
      for (let i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.display = "none";
      }
      
      const tablinks = document.getElementsByClassName("tablinks");
      for (let i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
      }
      
      document.getElementById(tabName).style.display = "block";
      evt.currentTarget.className += " active";
    }
    
    // Update form fields based on node type
    function updateFormFields() {
      const nodeType = document.getElementById('nodeType').value;
      const machineConfig = document.getElementById('machineConfig');
      
      if (nodeType === '0') { // Machine Monitor
        machineConfig.style.display = 'block';
      } else {
        machineConfig.style.display = 'none';
      }
    }
    
    // Scan WiFi networks
    function scanWiFi() {
      const networkList = document.getElementById('networkList');
      networkList.innerHTML = 'Scanning...';
      networkList.style.display = 'block';
      
      fetch('/api/wifi/scan')
        .then(response => response.json())
        .then(data => {
          if (data.networks && data.networks.length > 0) {
            networkList.innerHTML = '';
            data.networks.forEach(network => {
              const secured = network.secured ? '[Secured]' : '[Open]';
              const item = document.createElement('div');
              item.className = 'network-item';
              item.textContent = `${network.ssid} (${network.rssi} dBm) ${secured}`;
              item.onclick = function() { selectNetwork(network.ssid); };
              networkList.appendChild(item);
            });
          } else {
            networkList.innerHTML = 'No networks found';
          }
        })
        .catch(error => {
          console.error('Error scanning networks:', error);
          networkList.innerHTML = 'Error scanning networks';
        });
    }
    
    // Select network
    function selectNetwork(ssid) {
      document.getElementById('wifiSSID').value = ssid;
      
      const items = document.getElementsByClassName('network-item');
      for (let i = 0; i < items.length; i++) {
        items[i].classList.remove('selected');
        if (items[i].innerText.includes(ssid)) {
          items[i].classList.add('selected');
        }
      }
    }
    
    // Get node type name
    function getNodeTypeName(nodeType) {
      switch (parseInt(nodeType)) {
        case 0: return 'Machine Monitor';
        case 1: return 'Office RFID Reader';
        case 2: return 'Accessory IO Controller';
        default: return 'Unknown';
      }
    }
    
    // Format uptime
    function formatUptime(seconds) {
      const days = Math.floor(seconds / 86400);
      const hours = Math.floor((seconds % 86400) / 3600);
      const minutes = Math.floor((seconds % 3600) / 60);
      const secs = seconds % 60;
      
      return `${days}d ${hours}h ${minutes}m ${secs}s`;
    }
    
    // Update node-specific status
    function updateNodeSpecificStatus(data) {
      const machineStatus = document.getElementById('machineStatus');
      const officeReaderStatus = document.getElementById('officeReaderStatus');
      const accessoryIOStatus = document.getElementById('accessoryIOStatus');
      
      machineStatus.style.display = 'none';
      officeReaderStatus.style.display = 'none';
      accessoryIOStatus.style.display = 'none';
      
      if (data.node_type === 0) { // Machine Monitor
        machineStatus.style.display = 'block';
        
        const tableBody = document.getElementById('machineTableBody');
        tableBody.innerHTML = '';
        
        data.machines.forEach((machine, index) => {
          const row = tableBody.insertRow();
          row.insertCell(0).textContent = index;
          row.insertCell(1).textContent = machine.machine_id;
          row.insertCell(2).textContent = machine.status;
          row.insertCell(3).textContent = machine.rfid || 'None';
          row.insertCell(4).textContent = machine.activity_count;
        });
      } else if (data.node_type === 1) { // Office Reader
        officeReaderStatus.style.display = 'block';
        
        document.getElementById('lastTag').textContent = data.last_tag || 'None';
        document.getElementById('scanTime').textContent = data.scan_time ? new Date(data.scan_time).toLocaleString() : 'N/A';
      } else if (data.node_type === 2) { // Accessory IO
        accessoryIOStatus.style.display = 'block';
        
        const tableBody = document.getElementById('ioTableBody');
        tableBody.innerHTML = '';
        
        // Inputs
        data.inputs.forEach((input, index) => {
          const row = tableBody.insertRow();
          row.insertCell(0).textContent = index;
          row.insertCell(1).textContent = 'Input';
          row.insertCell(2).textContent = input ? 'HIGH' : 'LOW';
          row.insertCell(3).textContent = 'N/A';
        });
        
        // Outputs
        data.outputs.forEach((output, index) => {
          const row = tableBody.insertRow();
          row.insertCell(0).textContent = index;
          row.insertCell(1).textContent = 'Output';
          row.insertCell(2).textContent = output ? 'HIGH' : 'LOW';
          
          const controlCell = row.insertCell(3);
          const toggleBtn = document.createElement('button');
          toggleBtn.innerText = output ? 'Turn OFF' : 'Turn ON';
          toggleBtn.onclick = function() {
            toggleRelay(index, !output);
          };
          toggleBtn.style.width = 'auto';
          controlCell.appendChild(toggleBtn);
        });
      }
    }
    
    // Toggle relay
    function toggleRelay(relay, state) {
      fetch(`/api/relay?relay=${relay + 1}&state=${state ? 1 : 0}`)
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            fetchStatus(); // Refresh status to show new state
          } else {
            alert('Error: ' + data.message);
          }
        })
        .catch(error => {
          console.error('Error toggling relay:', error);
        });
    }
  </script>
</body>
</html>)rawliteral";

const char* WIRING_DIAGRAM_HTML = R"rawliteral(<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 RFID Node Wiring Diagram</title>
  <style>
    body { 
      font-family: Arial, sans-serif; 
      margin: 0; 
      padding: 20px; 
      color: #333; 
      background-color: #f8f9fa; 
    }
    
    h1, h2 { 
      color: #0066cc; 
    }
    
    .container { 
      max-width: 900px; 
      margin: 0 auto; 
      background: white; 
      padding: 20px; 
      border-radius: 8px; 
      box-shadow: 0 2px 4px rgba(0,0,0,0.1); 
    }
    
    .header { 
      text-align: center; 
      margin-bottom: 20px; 
      padding-bottom: 10px; 
      border-bottom: 1px solid #eee; 
    }
    
    .wiring-section {
      margin-bottom: 30px;
      padding: 15px;
      border: 1px solid #ddd;
      border-radius: 8px;
    }
    
    .pinout-table {
      width: 100%;
      border-collapse: collapse;
      margin: 15px 0;
    }
    
    .pinout-table th, .pinout-table td {
      border: 1px solid #ddd;
      padding: 8px;
      text-align: left;
    }
    
    .pinout-table th {
      background-color: #f2f2f2;
    }
    
    .wiring-diagram {
      width: 100%;
      max-width: 600px;
      margin: 20px auto;
      display: block;
      border: 1px solid #ddd;
      border-radius: 4px;
      padding: 10px;
    }
    
    .footer { 
      margin-top: 30px; 
      text-align: center; 
      font-size: 0.8em; 
      color: #777; 
    }
    
    .tab {
      overflow: hidden;
      border: 1px solid #ccc;
      background-color: #f1f1f1;
      border-radius: 4px 4px 0 0;
    }
    
    .tab button {
      background-color: inherit;
      float: left;
      border: none;
      outline: none;
      cursor: pointer;
      padding: 14px 16px;
      transition: 0.3s;
      color: #333;
    }
    
    .tab button:hover {
      background-color: #ddd;
    }
    
    .tab button.active {
      background-color: #0066cc;
      color: white;
    }
    
    .tabcontent {
      display: none;
      padding: 20px;
      border: 1px solid #ccc;
      border-top: none;
      border-radius: 0 0 4px 4px;
    }
    
    #machineWiring {
      display: block;
    }
    
    .note {
      background-color: #fff3cd;
      padding: 10px;
      border-left: 4px solid #ffc107;
      margin-bottom: 15px;
    }
    
    .warning {
      background-color: #f8d7da;
      padding: 10px;
      border-left: 4px solid #dc3545;
      margin-bottom: 15px;
    }
    
    pre {
      background-color: #f5f5f5;
      padding: 10px;
      border-radius: 4px;
      overflow-x: auto;
    }
    
    .ascii-diagram {
      font-family: monospace;
      white-space: pre;
      font-size: 12px;
      line-height: 1.2;
      background-color: #f5f5f5;
      padding: 15px;
      border-radius: 4px;
      overflow-x: auto;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>ESP32 RFID Node Wiring Diagram</h1>
      <p>Follow these instructions to correctly wire your ESP32 node based on its configuration type.</p>
    </div>
    
    <div class="tab">
      <button class="tablinks active" onclick="openTab(event, 'machineWiring')">Machine Monitor</button>
      <button class="tablinks" onclick="openTab(event, 'officeWiring')">Office Reader</button>
      <button class="tablinks" onclick="openTab(event, 'accessoryWiring')">Accessory IO</button>
    </div>
    
    <div id="machineWiring" class="tabcontent">
      <h2>Machine Monitor Wiring</h2>
      
      <div class="note">
        <strong>Note:</strong> Machine Monitor nodes connect to up to 4 RFID readers and 4 relay outputs for machine control.
      </div>
      
      <div class="wiring-section">
        <h3>ESP32 to RFID Reader Connections</h3>
        
        <table class="pinout-table">
          <tr>
            <th>RFID Reader Pin</th>
            <th>ESP32 Pin (Zone 0)</th>
            <th>ESP32 Pin (Zone 1)</th>
            <th>ESP32 Pin (Zone 2)</th>
            <th>ESP32 Pin (Zone 3)</th>
          </tr>
          <tr>
            <td>SDA (SS)</td>
            <td>5</td>
            <td>17</td>
            <td>16</td>
            <td>4</td>
          </tr>
          <tr>
            <td>SCK</td>
            <td colspan="4" style="text-align: center;">18 (Shared)</td>
          </tr>
          <tr>
            <td>MOSI</td>
            <td colspan="4" style="text-align: center;">23 (Shared)</td>
          </tr>
          <tr>
            <td>MISO</td>
            <td colspan="4" style="text-align: center;">19 (Shared)</td>
          </tr>
          <tr>
            <td>RST</td>
            <td>27</td>
            <td>25</td>
            <td>33</td>
            <td>32</td>
          </tr>
          <tr>
            <td>3.3V</td>
            <td colspan="4" style="text-align: center;">3.3V</td>
          </tr>
          <tr>
            <td>GND</td>
            <td colspan="4" style="text-align: center;">GND</td>
          </tr>
        </table>
        
        <h3>ESP32 to Relay Module Connections</h3>
        
        <table class="pinout-table">
          <tr>
            <th>Function</th>
            <th>ESP32 Pin</th>
            <th>Connection</th>
          </tr>
          <tr>
            <td>Relay 1 (Zone 0)</td>
            <td>26</td>
            <td>IN1 on Relay Module</td>
          </tr>
          <tr>
            <td>Relay 2 (Zone 1)</td>
            <td>14</td>
            <td>IN2 on Relay Module</td>
          </tr>
          <tr>
            <td>Relay 3 (Zone 2)</td>
            <td>12</td>
            <td>IN3 on Relay Module</td>
          </tr>
          <tr>
            <td>Relay 4 (Zone 3)</td>
            <td>13</td>
            <td>IN4 on Relay Module</td>
          </tr>
          <tr>
            <td>VCC</td>
            <td>5V</td>
            <td>VCC on Relay Module</td>
          </tr>
          <tr>
            <td>GND</td>
            <td>GND</td>
            <td>GND on Relay Module</td>
          </tr>
        </table>
        
        <h3>ESP32 to Activity Sensor Connections</h3>
        
        <table class="pinout-table">
          <tr>
            <th>Function</th>
            <th>ESP32 Pin</th>
            <th>Connection</th>
          </tr>
          <tr>
            <td>Activity Sensor 1 (Zone 0)</td>
            <td>34</td>
            <td>Signal from Sensor</td>
          </tr>
          <tr>
            <td>Activity Sensor 2 (Zone 1)</td>
            <td>35</td>
            <td>Signal from Sensor</td>
          </tr>
          <tr>
            <td>Activity Sensor 3 (Zone 2)</td>
            <td>36</td>
            <td>Signal from Sensor</td>
          </tr>
          <tr>
            <td>Activity Sensor 4 (Zone 3)</td>
            <td>39</td>
            <td>Signal from Sensor</td>
          </tr>
        </table>
        
        <div class="ascii-diagram">
+---------------+     +----------------+     +----------------+
|               |     |                |     |                |
|  RFID Reader  |-----|    ESP32       |-----|  Relay Module  |
|  (up to 4)    |     |                |     |  (4 channel)   |
|               |     |                |     |                |
+---------------+     +----------------+     +----------------+
                            |
                            |
                      +----------------+
                      |                |
                      | Activity Sensor |
                      | (up to 4)      |
                      |                |
                      +----------------+
        </div>
      </div>
    </div>
    
    <div id="officeWiring" class="tabcontent">
      <h2>Office RFID Reader Wiring</h2>
      
      <div class="note">
        <strong>Note:</strong> Office Reader nodes use a single RFID reader and can include an optional buzzer for feedback.
      </div>
      
      <div class="wiring-section">
        <h3>ESP32 to RFID Reader Connections</h3>
        
        <table class="pinout-table">
          <tr>
            <th>RFID Reader Pin</th>
            <th>ESP32 Pin</th>
          </tr>
          <tr>
            <td>SDA (SS)</td>
            <td>5</td>
          </tr>
          <tr>
            <td>SCK</td>
            <td>18</td>
          </tr>
          <tr>
            <td>MOSI</td>
            <td>23</td>
          </tr>
          <tr>
            <td>MISO</td>
            <td>19</td>
          </tr>
          <tr>
            <td>RST</td>
            <td>27</td>
          </tr>
          <tr>
            <td>3.3V</td>
            <td>3.3V</td>
          </tr>
          <tr>
            <td>GND</td>
            <td>GND</td>
          </tr>
        </table>
        
        <h3>ESP32 to Buzzer Connection</h3>
        
        <table class="pinout-table">
          <tr>
            <th>Function</th>
            <th>ESP32 Pin</th>
            <th>Connection</th>
          </tr>
          <tr>
            <td>Buzzer</td>
            <td>15</td>
            <td>Buzzer + (Positive)</td>
          </tr>
          <tr>
            <td>GND</td>
            <td>GND</td>
            <td>Buzzer - (Negative)</td>
          </tr>
        </table>
        
        <h3>ESP32 to LED Indicator Connection</h3>
        
        <table class="pinout-table">
          <tr>
            <th>Function</th>
            <th>ESP32 Pin</th>
            <th>Connection</th>
          </tr>
          <tr>
            <td>Status LED</td>
            <td>2</td>
            <td>LED + (with 220Ω resistor)</td>
          </tr>
          <tr>
            <td>GND</td>
            <td>GND</td>
            <td>LED -</td>
          </tr>
        </table>
      </div>
    </div>
    
    <div id="accessoryWiring" class="tabcontent">
      <h2>Accessory IO Controller Wiring</h2>
      
      <div class="note">
        <strong>Note:</strong> Accessory IO nodes control external devices (like fans and lights) and can read input sensors.
      </div>
      
      <div class="wiring-section">
        <h3>ESP32 to Relay Module Connections</h3>
        
        <table class="pinout-table">
          <tr>
            <th>Function</th>
            <th>ESP32 Pin</th>
            <th>Connection</th>
          </tr>
          <tr>
            <td>Output 1</td>
            <td>26</td>
            <td>IN1 on Relay Module</td>
          </tr>
          <tr>
            <td>Output 2</td>
            <td>14</td>
            <td>IN2 on Relay Module</td>
          </tr>
          <tr>
            <td>Output 3</td>
            <td>12</td>
            <td>IN3 on Relay Module</td>
          </tr>
          <tr>
            <td>Output 4</td>
            <td>13</td>
            <td>IN4 on Relay Module</td>
          </tr>
          <tr>
            <td>VCC</td>
            <td>5V</td>
            <td>VCC on Relay Module</td>
          </tr>
          <tr>
            <td>GND</td>
            <td>GND</td>
            <td>GND on Relay Module</td>
          </tr>
        </table>
        
        <h3>ESP32 to Input Sensor Connections</h3>
        
        <table class="pinout-table">
          <tr>
            <th>Function</th>
            <th>ESP32 Pin</th>
            <th>Connection</th>
          </tr>
          <tr>
            <td>Input 1</td>
            <td>34</td>
            <td>Signal from Sensor 1</td>
          </tr>
          <tr>
            <td>Input 2</td>
            <td>35</td>
            <td>Signal from Sensor 2</td>
          </tr>
          <tr>
            <td>Input 3</td>
            <td>36</td>
            <td>Signal from Sensor 3</td>
          </tr>
          <tr>
            <td>Input 4</td>
            <td>39</td>
            <td>Signal from Sensor 4</td>
          </tr>
        </table>
        
        <div class="warning">
          <strong>Warning:</strong> Input pins on ESP32 are 3.3V tolerant ONLY. For 5V sensors, use a voltage divider or level shifter.
        </div>
      </div>
    </div>
    
    <div class="wiring-section">
      <h3>General Notes</h3>
      <ul>
        <li>The ESP32 operates at 3.3V logic level. All connections should respect this voltage level.</li>
        <li>Use adequate power supply. When multiple relays are engaged, power requirements increase.</li>
        <li>For reliable operation, keep wires as short as possible, especially for SPI connections to RFID readers.</li>
        <li>Add a 0.1μF ceramic capacitor between VCC and GND of each RFID reader to reduce noise.</li>
        <li>If using long wires for activity sensors, consider using shielded cable to prevent false triggers.</li>
      </ul>
    </div>
    
    <div class="footer">
      <p>ESP32 Multi-Function Node v1.0.0 | &copy; 2025 Shop</p>
    </div>
  </div>
  
  <script>
    function openTab(evt, tabName) {
      const tabcontent = document.getElementsByClassName("tabcontent");
      for (let i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.display = "none";
      }
      
      const tablinks = document.getElementsByClassName("tablinks");
      for (let i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
      }
      
      document.getElementById(tabName).style.display = "block";
      evt.currentTarget.className += " active";
    }
  </script>
</body>
</html>)rawliteral";

// Define global objects
ConfigManager configManager;
NetworkManager networkManager;
RFIDHandler rfidHandler;
WebUI webUI(configManager, networkManager);

// State variables
String currentTags[4] = {"", "", "", ""};
unsigned long lastActivityTime[4] = {0, 0, 0, 0};
int activityCounts[4] = {0, 0, 0, 0};
unsigned long lastReportTime = 0;
bool relayStates[4] = {false, false, false, false};
bool inputStates[4] = {false, false, false, false};

// Forward declarations
void checkActivity();
void reportActivity();
void toggleRelay(int relayNum, bool state);
void factoryReset();
void blinkLED(int times, int delayMS);

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nStarting ESP32 Multi-Function Node...");
  
  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize relay pins
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(RELAY3_PIN, LOW);
  digitalWrite(RELAY4_PIN, LOW);
  
  // Initialize activity sensor pins
  pinMode(ACTIVITY1_PIN, INPUT);
  pinMode(ACTIVITY2_PIN, INPUT);
  pinMode(ACTIVITY3_PIN, INPUT);
  pinMode(ACTIVITY4_PIN, INPUT);
  
  // Initialize buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Initialize buttons
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  
  // Signal startup
  blinkLED(3, 200);
  
  // Load configuration
  bool configLoaded = configManager.load();
  
  // Initialize RFID handler
  rfidHandler.begin(configManager.getNodeType());
  
  // Setup networking
  bool wifiConnected = networkManager.begin(
    configManager.getWifiSSID(),
    configManager.getWifiPassword(),
    configManager.getNodeName()
  );
  
  // Setup web server
  webUI.begin();
  
  // Update WebUI with initial states
  webUI.setCurrentTags(currentTags);
  webUI.setActivityCounts(activityCounts);
  webUI.setRelayStates(relayStates);
  webUI.setInputStates(inputStates);
  
  Serial.println("Setup complete!");
  Serial.print("Node Type: ");
  Serial.println(configManager.getNodeType() == NODE_TYPE_MACHINE_MONITOR ? "Machine Monitor" : 
                configManager.getNodeType() == NODE_TYPE_OFFICE_READER ? "Office RFID Reader" : "Accessory IO Controller");
  Serial.print("Node Name: ");
  Serial.println(configManager.getNodeName());
  
  // Signal ready
  blinkLED(5, 100);
}

void loop() {
  // Handle RFID readers
  if (configManager.getNodeType() == NODE_TYPE_MACHINE_MONITOR || 
      configManager.getNodeType() == NODE_TYPE_OFFICE_READER) {
    
    bool tagDetected = rfidHandler.checkReaders(currentTags, configManager.getNodeType());
    
    // If tag detected, update WebUI
    if (tagDetected) {
      webUI.setCurrentTags(currentTags);
      
      // If machine monitor, toggle relays based on current tags
      if (configManager.getNodeType() == NODE_TYPE_MACHINE_MONITOR) {
        for (int i = 0; i < 4; i++) {
          toggleRelay(i + 1, currentTags[i] != "");
        }
      }
    }
  }
  
  // Check activity sensors and update relay states
  checkActivity();
  
  // Sync relay states with hardware (in case they were changed via Web UI)
  for (int i = 0; i < 4; i++) {
    int relayPin = 0;
    switch (i) {
      case 0: relayPin = RELAY1_PIN; break;
      case 1: relayPin = RELAY2_PIN; break;
      case 2: relayPin = RELAY3_PIN; break;
      case 3: relayPin = RELAY4_PIN; break;
    }
    
    // Only update if state has changed
    if (digitalRead(relayPin) == HIGH != relayStates[i]) {
      digitalWrite(relayPin, relayStates[i] ? HIGH : LOW);
    }
  }
  
  // Report activity periodically
  if (millis() - lastReportTime > REPORT_INTERVAL) {
    if (networkManager.isConnected()) {
      reportActivity();
    }
    lastReportTime = millis();
  }
  
  // Check for factory reset (both buttons pressed for 5 seconds)
  if (digitalRead(BUTTON1_PIN) == LOW && digitalRead(BUTTON2_PIN) == LOW) {
    static unsigned long buttonTime = 0;
    if (buttonTime == 0) {
      buttonTime = millis();
    } else if (millis() - buttonTime > 5000) {
      Serial.println("Factory reset triggered by buttons...");
      factoryReset();
      buttonTime = 0;
    }
  } else {
    static unsigned long buttonTime = 0;
  }
  
  delay(50); // Small delay to prevent CPU overload
}

void checkActivity() {
  // Check activity pins
  for (int i = 0; i < 4; i++) {
    // Skip check if no user is active
    if (currentTags[i] == "" && configManager.getNodeType() == NODE_TYPE_MACHINE_MONITOR) {
      continue;
    }
    
    // Read activity pin
    int activityPin;
    switch (i) {
      case 0: activityPin = ACTIVITY1_PIN; break;
      case 1: activityPin = ACTIVITY2_PIN; break;
      case 2: activityPin = ACTIVITY3_PIN; break;
      case 3: activityPin = ACTIVITY4_PIN; break;
    }
    
    // Update state
    bool currentState = (digitalRead(activityPin) == HIGH);
    inputStates[i] = currentState;
    
    // Update activity count if pin is HIGH and debounce
    if (currentState) {
      if (millis() - lastActivityTime[i] > 1000) { // Debounce 1 second
        activityCounts[i]++;
        lastActivityTime[i] = millis();
        
        // Log activity
        Serial.print("Activity detected on pin ");
        Serial.print(activityPin);
        Serial.print(" (zone ");
        Serial.print(i);
        Serial.println(")");
        
        // Update WebUI
        webUI.setActivityCounts(activityCounts);
        webUI.setInputStates(inputStates);
      }
    }
  }
}

void reportActivity() {
  if (!networkManager.isConnected()) return;
  
  Serial.println("Reporting activity to server...");
  
  // Construct report data
  DynamicJsonDocument doc(1024);
  doc["node_id"] = configManager.getNodeName();
  doc["node_type"] = configManager.getNodeType();
  
  JsonArray machineData = doc.createNestedArray("machines");
  for (int i = 0; i < 4; i++) {
    if (configManager.getMachineID(i).length() > 0) {
      JsonObject machine = machineData.createNestedObject();
      machine["machine_id"] = configManager.getMachineID(i);
      machine["activity_count"] = activityCounts[i];
      machine["status"] = currentTags[i] != "" ? "active" : "idle";
      machine["rfid"] = currentTags[i];
    }
  }
  
  // Send report to server
  String reportUrl = configManager.getServerUrl() + "/api/activity";
  String payload;
  serializeJson(doc, payload);
  
  String response;
  bool success = networkManager.sendPOST(reportUrl, payload, response);
  
  if (success) {
    // Reset activity counts after successful report
    for (int i = 0; i < 4; i++) {
      activityCounts[i] = 0;
    }
    
    // Update WebUI
    webUI.setActivityCounts(activityCounts);
  }
}

void toggleRelay(int relayNum, bool state) {
  switch (relayNum) {
    case 1:
      digitalWrite(RELAY1_PIN, state ? HIGH : LOW);
      relayStates[0] = state;
      break;
    case 2:
      digitalWrite(RELAY2_PIN, state ? HIGH : LOW);
      relayStates[1] = state;
      break;
    case 3:
      digitalWrite(RELAY3_PIN, state ? HIGH : LOW);
      relayStates[2] = state;
      break;
    case 4:
      digitalWrite(RELAY4_PIN, state ? HIGH : LOW);
      relayStates[3] = state;
      break;
  }
  
  // Update WebUI
  webUI.setRelayStates(relayStates);
  
  // Log relay action
  Serial.print("Relay ");
  Serial.print(relayNum);
  Serial.print(" set to ");
  Serial.println(state ? "ON" : "OFF");
}

void factoryReset() {
  Serial.println("Performing factory reset...");
  
  // Reset configuration
  configManager.reset();
  
  // Reset hardware
  for (int i = 1; i <= 4; i++) {
    toggleRelay(i, false);
  }
  
  // Signal reset complete
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
    delay(100);
  }
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Restart
  ESP.restart();
}

void blinkLED(int times, int delayMS) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayMS);
    digitalWrite(LED_PIN, LOW);
    delay(delayMS);
  }
}