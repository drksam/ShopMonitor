#ifndef DASHBOARD_H
#define DASHBOARD_H

const char* dashboard_html = R"rawliteral(
<!DOCTYPE html>
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
    
    #machineConfig {
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
        <button id="updateBtn">Check for Update</button>
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
      // In the actual implementation, this would send a request to scan networks
      // For this example, we'll just show some dummy networks
      const networkList = document.getElementById('networkList');
      networkList.style.display = 'block';
      networkList.innerHTML = 'Scanning networks...';
      
      // Request network scan
      fetch('/api/scan_wifi')
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            let networks = '';
            data.networks.forEach(network => {
              networks += `<div class="network-item" onclick="selectNetwork('${network.ssid}')">${network.ssid} (${network.rssi} dBm) [${network.encrypted ? 'Secured' : 'Open'}]</div>`;
            });
            networkList.innerHTML = networks;
          } else {
            networkList.innerHTML = 'Error scanning networks: ' + data.message;
          }
        })
        .catch(error => {
          networkList.innerHTML = 'Error scanning networks';
          console.error('Error scanning networks:', error);
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
</html>
)rawliteral";

#endif // DASHBOARD_H