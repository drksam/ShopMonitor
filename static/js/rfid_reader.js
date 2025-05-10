/**
 * RFID Reader Testing Tool for Shop Suite
 * 
 * This JavaScript file provides a simple web-based interface for simulating
 * RFID tag scans for testing purposes. It allows administrators to:
 * 1. Test RFID tags with different machines
 * 2. Simulate activity on machines
 * 3. Test system behavior without physical hardware
 */

// Global variables
let currentMachine = null;
let currentRfid = null;
let activityInterval = null;

document.addEventListener('DOMContentLoaded', function() {
    // Initialize UI elements
    initializeRfidTester();
    
    // Set up event listeners
    setupEventListeners();
});

/**
 * Initializes the RFID tester UI
 */
function initializeRfidTester() {
    const rfidTesterDiv = document.getElementById('rfid-tester');
    
    if (!rfidTesterDiv) {
        console.warn('RFID tester container not found in page');
        return;
    }
    
    // Load available machines from the server
    fetch('/api/machines/status')
        .then(response => response.json())
        .then(machines => {
            // Create machine selector
            const machineSelect = document.createElement('select');
            machineSelect.id = 'machine-select';
            machineSelect.className = 'form-select mb-3';
            
            const defaultOption = document.createElement('option');
            defaultOption.value = '';
            defaultOption.textContent = 'Select a machine...';
            machineSelect.appendChild(defaultOption);
            
            machines.forEach(machine => {
                const option = document.createElement('option');
                option.value = machine.machine_id;
                option.textContent = `${machine.name} (#${machine.machine_id})`;
                machineSelect.appendChild(option);
            });
            
            rfidTesterDiv.appendChild(machineSelect);
            
            // Create RFID input
            const rfidInput = document.createElement('div');
            rfidInput.className = 'input-group mb-3';
            rfidInput.innerHTML = `
                <span class="input-group-text"><i class="fas fa-id-card"></i></span>
                <input type="text" id="rfid-input" class="form-control" placeholder="Enter RFID tag ID">
                <button id="scan-rfid" class="btn btn-primary">Scan Tag</button>
            `;
            
            rfidTesterDiv.appendChild(rfidInput);
            
            // Create actions section
            const actionsDiv = document.createElement('div');
            actionsDiv.className = 'card mb-3';
            actionsDiv.innerHTML = `
                <div class="card-header">
                    <h5 class="mb-0">Actions</h5>
                </div>
                <div class="card-body">
                    <div class="mb-3">
                        <button id="logout-btn" class="btn btn-warning me-2" disabled>
                            <i class="fas fa-sign-out-alt me-1"></i> Logout User
                        </button>
                        <button id="simulate-activity-btn" class="btn btn-success me-2" disabled>
                            <i class="fas fa-cog me-1"></i> Simulate Activity
                        </button>
                        <button id="stop-activity-btn" class="btn btn-danger" disabled>
                            <i class="fas fa-stop me-1"></i> Stop Activity
                        </button>
                    </div>
                </div>
            `;
            
            rfidTesterDiv.appendChild(actionsDiv);
            
            // Create status display
            const statusDiv = document.createElement('div');
            statusDiv.className = 'card';
            statusDiv.innerHTML = `
                <div class="card-header">
                    <h5 class="mb-0">Status</h5>
                </div>
                <div class="card-body">
                    <p><strong>Current Machine:</strong> <span id="current-machine">None</span></p>
                    <p><strong>Current RFID:</strong> <span id="current-rfid">None</span></p>
                    <p><strong>Last Action:</strong> <span id="last-action">None</span></p>
                    <p><strong>Status:</strong> <span id="status-message">Ready</span></p>
                </div>
            `;
            
            rfidTesterDiv.appendChild(statusDiv);
        })
        .catch(error => {
            console.error('Error loading machines:', error);
            rfidTesterDiv.innerHTML = `
                <div class="alert alert-danger">
                    <i class="fas fa-exclamation-triangle me-2"></i>
                    Error loading machines. Please refresh the page or check server connectivity.
                </div>
            `;
        });
}

/**
 * Sets up event listeners for the UI
 */
function setupEventListeners() {
    // Add listener for machine select
    document.addEventListener('change', function(e) {
        if (e.target && e.target.id === 'machine-select') {
            currentMachine = e.target.value;
            document.getElementById('current-machine').textContent = currentMachine || 'None';
            
            // Update button states
            updateButtonStates();
        }
    });
    
    // Add listener for RFID scan button
    document.addEventListener('click', function(e) {
        if (e.target && e.target.id === 'scan-rfid') {
            const rfidInput = document.getElementById('rfid-input');
            const rfidValue = rfidInput.value.trim();
            
            if (rfidValue && currentMachine) {
                scanRfid(rfidValue);
            } else {
                updateStatus('Please select a machine and enter an RFID tag ID', 'warning');
            }
        }
    });
    
    // Add listener for logout button
    document.addEventListener('click', function(e) {
        if (e.target && e.target.id === 'logout-btn') {
            if (currentMachine && currentRfid) {
                logoutUser();
            }
        }
    });
    
    // Add listener for simulate activity button
    document.addEventListener('click', function(e) {
        if (e.target && e.target.id === 'simulate-activity-btn') {
            if (currentMachine) {
                startActivitySimulation();
            }
        }
    });
    
    // Add listener for stop activity button
    document.addEventListener('click', function(e) {
        if (e.target && e.target.id === 'stop-activity-btn') {
            stopActivitySimulation();
        }
    });
}

/**
 * Simulates scanning an RFID tag
 */
function scanRfid(rfidValue) {
    if (!currentMachine) {
        updateStatus('Please select a machine first', 'warning');
        return;
    }
    
    updateStatus('Sending RFID scan...', 'info');
    
    fetch(`/api/check_user?rfid=${rfidValue}&machine_id=${currentMachine}`)
        .then(response => {
            if (response.ok) {
                return response.text().then(text => {
                    if (text.includes('ALLOW')) {
                        currentRfid = rfidValue;
                        document.getElementById('current-rfid').textContent = currentRfid;
                        document.getElementById('last-action').textContent = 'RFID scan - Access granted';
                        updateStatus('Access granted', 'success');
                    } else if (text.includes('DENY')) {
                        updateStatus('Access denied', 'danger');
                        document.getElementById('last-action').textContent = 'RFID scan - Access denied';
                    } else {
                        updateStatus('Unknown response: ' + text, 'warning');
                        document.getElementById('last-action').textContent = 'RFID scan - Unknown response';
                    }
                    
                    updateButtonStates();
                });
            } else {
                throw new Error('Server returned status ' + response.status);
            }
        })
        .catch(error => {
            console.error('Error scanning RFID:', error);
            updateStatus('Error: ' + error.message, 'danger');
            document.getElementById('last-action').textContent = 'RFID scan - Error';
        });
}

/**
 * Simulates logging out a user
 */
function logoutUser() {
    if (!currentMachine || !currentRfid) {
        updateStatus('No active user to log out', 'warning');
        return;
    }
    
    updateStatus('Logging out user...', 'info');
    
    fetch(`/api/logout?rfid=${currentRfid}&machine_id=${currentMachine}`)
        .then(response => {
            if (response.ok) {
                return response.text().then(text => {
                    if (text.includes('LOGOUT')) {
                        document.getElementById('last-action').textContent = 'User logout';
                        updateStatus('User logged out successfully', 'success');
                        currentRfid = null;
                        document.getElementById('current-rfid').textContent = 'None';
                        
                        // Stop any ongoing activity simulation
                        stopActivitySimulation();
                    } else {
                        updateStatus('Unexpected response: ' + text, 'warning');
                        document.getElementById('last-action').textContent = 'Logout attempt - Unexpected response';
                    }
                    
                    updateButtonStates();
                });
            } else {
                throw new Error('Server returned status ' + response.status);
            }
        })
        .catch(error => {
            console.error('Error logging out:', error);
            updateStatus('Error: ' + error.message, 'danger');
            document.getElementById('last-action').textContent = 'Logout attempt - Error';
        });
}

/**
 * Starts simulating machine activity
 */
function startActivitySimulation() {
    if (!currentMachine) {
        updateStatus('Please select a machine first', 'warning');
        return;
    }
    
    // Clear any existing interval
    stopActivitySimulation();
    
    // Send activity heartbeat every 15 seconds
    activityInterval = setInterval(() => {
        sendActivityHeartbeat();
    }, 15000);
    
    // Send initial heartbeat immediately
    sendActivityHeartbeat();
    
    updateStatus('Activity simulation started', 'success');
    document.getElementById('last-action').textContent = 'Started activity simulation';
    document.getElementById('simulate-activity-btn').disabled = true;
    document.getElementById('stop-activity-btn').disabled = false;
}

/**
 * Stops simulating machine activity
 */
function stopActivitySimulation() {
    if (activityInterval) {
        clearInterval(activityInterval);
        activityInterval = null;
        
        updateStatus('Activity simulation stopped', 'info');
        document.getElementById('last-action').textContent = 'Stopped activity simulation';
        document.getElementById('simulate-activity-btn').disabled = false;
        document.getElementById('stop-activity-btn').disabled = true;
    }
}

/**
 * Sends a heartbeat to indicate machine activity
 */
function sendActivityHeartbeat() {
    if (!currentMachine) {
        stopActivitySimulation();
        return;
    }
    
    fetch(`/api/heartbeat?machine_id=${currentMachine}&activity=1`)
        .then(response => {
            if (!response.ok) {
                throw new Error('Server returned status ' + response.status);
            }
            // Success - no need to process response
        })
        .catch(error => {
            console.error('Error sending heartbeat:', error);
            updateStatus('Error sending activity heartbeat: ' + error.message, 'warning');
            // Don't stop simulation on error - it will retry
        });
}

/**
 * Updates the status message
 */
function updateStatus(message, type) {
    const statusElement = document.getElementById('status-message');
    if (statusElement) {
        statusElement.textContent = message;
        statusElement.className = ''; // Clear existing classes
        statusElement.classList.add(`text-${type}`);
    }
}

/**
 * Updates button states based on current status
 */
function updateButtonStates() {
    const logoutBtn = document.getElementById('logout-btn');
    const simulateActivityBtn = document.getElementById('simulate-activity-btn');
    const stopActivityBtn = document.getElementById('stop-activity-btn');
    
    if (currentMachine && currentRfid) {
        // Machine selected and user logged in
        logoutBtn.disabled = false;
        simulateActivityBtn.disabled = false;
    } else if (currentMachine) {
        // Machine selected but no user logged in
        logoutBtn.disabled = true;
        simulateActivityBtn.disabled = true;
    } else {
        // No machine selected
        logoutBtn.disabled = true;
        simulateActivityBtn.disabled = true;
        stopActivityBtn.disabled = true;
    }
}
