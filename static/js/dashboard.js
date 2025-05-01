// Dashboard functionality for NooyenUSATracker

/**
 * Updates the dashboard with current machine statuses
 */
function updateDashboard() {
    fetch('/api/machines/status')
        .then(response => response.json())
        .then(machines => {
            machines.forEach(machine => {
                const machineElement = document.getElementById(`machine-${machine.machine_id}`);
                if (machineElement) {
                    // Update machine card border based on status
                    const card = machineElement.querySelector('.machine-card');
                    card.className = 'card h-100 machine-card';
                    
                    // Add border color based on status
                    if (machine.status === 'active') {
                        card.classList.add('border-success');
                    } else if (machine.status === 'warning' || machine.warning_status === 'warning') {
                        card.classList.add('border-warning');
                    } else if (machine.status === 'offline') {
                        card.classList.add('border-secondary');
                    } else {
                        card.classList.add('border-primary');
                    }
                    
                    // Update status indicator
                    const statusIndicator = machineElement.querySelector('.machine-status');
                    statusIndicator.className = 'machine-status';
                    statusIndicator.classList.add(`status-${machine.status}`);
                    
                    // Update status text
                    const statusText = machineElement.querySelector('.machine-status-text');
                    if (statusText) {
                        // Check if warning status should override machine status
                        if (machine.warning_status === 'warning') {
                            statusText.textContent = 'Warning';
                            card.classList.add('border-warning');
                            statusIndicator.className = 'machine-status status-warning';
                        } else if (machine.warning_status === 'timeout') {
                            statusText.textContent = 'Timeout';
                            card.classList.add('border-warning');
                            statusIndicator.className = 'machine-status status-warning';
                        } else {
                            statusText.textContent = machine.status.charAt(0).toUpperCase() + machine.status.slice(1);
                        }
                    }
                    
                    // Update user information
                    const cardBody = machineElement.querySelector('.card-body');
                    if (machine.current_user) {
                        // Check if user element already exists
                        let userElement = cardBody.querySelector('.user-info');
                        if (!userElement) {
                            // Create user element if it doesn't exist
                            userElement = document.createElement('p');
                            userElement.className = 'user-info';
                            userElement.innerHTML = '<i class="fas fa-user me-2"></i>User: <span class="current-user"></span>';
                            cardBody.appendChild(userElement);
                        }
                        
                        // Update user name
                        const userSpan = userElement.querySelector('.current-user');
                        userSpan.textContent = machine.current_user;
                        
                        // Add or update last activity
                        if (machine.last_activity) {
                            let activityElement = cardBody.querySelector('.activity-info');
                            if (!activityElement) {
                                activityElement = document.createElement('p');
                                activityElement.className = 'activity-info';
                                activityElement.innerHTML = '<i class="fas fa-clock me-2"></i>Last Activity: <span class="last-activity"></span>';
                                cardBody.appendChild(activityElement);
                            }
                            
                            const lastActivity = new Date(machine.last_activity);
                            const formattedTime = lastActivity.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
                            activityElement.querySelector('.last-activity').textContent = formattedTime;
                        }
                    } else {
                        // Remove user info if no user is logged in
                        const userElement = cardBody.querySelector('.user-info');
                        const activityElement = cardBody.querySelector('.activity-info');
                        
                        if (userElement) {
                            cardBody.removeChild(userElement);
                        }
                        
                        if (activityElement) {
                            cardBody.removeChild(activityElement);
                        }
                        
                        // Add no user message if it doesn't exist
                        let noUserElement = cardBody.querySelector('.no-user-info');
                        if (!noUserElement) {
                            noUserElement = document.createElement('p');
                            noUserElement.className = 'no-user-info';
                            noUserElement.innerHTML = '<i class="fas fa-user-slash me-2"></i>No active user';
                            cardBody.appendChild(noUserElement);
                        }
                    }
                }
            });
            
            // Update dashboard statistics
            updateStatistics(machines);
        })
        .catch(error => {
            console.error('Error updating dashboard:', error);
        });
}

/**
 * Updates dashboard statistics based on machine data
 * @param {Array} machines - Array of machine objects
 */
function updateStatistics(machines) {
    let activeCount = 0;
    let idleCount = 0;
    let warningCount = 0;
    let offlineCount = 0;
    let activeUsers = 0;
    
    machines.forEach(machine => {
        // Count by status
        if (machine.status === 'active') {
            activeCount++;
        } else if (machine.status === 'warning' || machine.warning_status === 'warning' || machine.warning_status === 'timeout') {
            warningCount++;
        } else if (machine.status === 'offline') {
            offlineCount++;
        } else {
            idleCount++;
        }
        
        // Count active users
        if (machine.current_user) {
            activeUsers++;
        }
    });
    
    // Update machine status chart if it exists
    if (window.statusChart) {
        window.statusChart.data.datasets[0].data = [idleCount, activeCount, warningCount, offlineCount];
        window.statusChart.update();
    }
    
    // Update active users counter if it exists
    const activeUsersElement = document.querySelector('.dashboard-stats h2');
    if (activeUsersElement) {
        activeUsersElement.textContent = activeUsers;
    }
    
    // Update active machines counter if it exists
    const activeCountElement = document.querySelectorAll('.dashboard-stats h2')[1];
    if (activeCountElement) {
        activeCountElement.textContent = activeCount;
    }
    
    // Update warnings counter if it exists
    const warningCountElement = document.querySelectorAll('.dashboard-stats h2')[2];
    if (warningCountElement) {
        warningCountElement.textContent = warningCount;
    }
}

/**
 * Formats relative timestamps on the page
 */
function formatTimestamps() {
    document.querySelectorAll('.last-activity').forEach(element => {
        const timestamp = element.getAttribute('data-timestamp');
        if (timestamp) {
            const date = new Date(timestamp);
            const formattedTime = date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
            element.textContent = formattedTime;
        }
    });
}
