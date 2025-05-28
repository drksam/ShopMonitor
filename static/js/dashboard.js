// Dashboard functionality for Shop Suite

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
                    
                    // Handle lead operator status - highlight machines without a lead operator
                    if (machine.status !== 'offline' && !machine.lead_operator_id) {
                        card.classList.add('border-danger');
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
                        } else if (machine.status !== 'offline' && !machine.lead_operator_id) {
                            statusText.textContent = 'No Lead';
                            statusIndicator.className = 'machine-status status-no-lead';
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
                        
                        // Add lead operator badge and information
                        if (machine.lead_operator_id) {
                            // Remove existing badge if it exists
                            const existingBadge = card.querySelector('.lead-operator-badge');
                            if (!existingBadge) {
                                const leadBadge = document.createElement('div');
                                leadBadge.className = 'lead-operator-badge';
                                leadBadge.innerHTML = '<i class="fas fa-star lead-operator-icon"></i>LEAD';
                                card.appendChild(leadBadge);
                            }
                            
                            // Add lead operator name if not already shown
                            let leadInfoElement = cardBody.querySelector('.lead-info');
                            if (!leadInfoElement) {
                                leadInfoElement = document.createElement('p');
                                leadInfoElement.className = 'lead-info mt-1';
                                leadInfoElement.innerHTML = '<i class="fas fa-user-check me-2 text-warning"></i>Lead: <span class="lead-name"></span>';
                                cardBody.appendChild(leadInfoElement);
                            }
                            
                            // Update lead operator name
                            const leadNameSpan = leadInfoElement.querySelector('.lead-name');
                            leadNameSpan.textContent = machine.lead_operator_name || 'Assigned';
                            
                        } else {
                            // Remove badge if exists and user is not lead
                            const existingBadge = card.querySelector('.lead-operator-badge');
                            const leadInfoElement = cardBody.querySelector('.lead-info');
                            
                            if (existingBadge) {
                                card.removeChild(existingBadge);
                            }
                            
                            if (leadInfoElement) {
                                cardBody.removeChild(leadInfoElement);
                            }
                            
                            // Add warning about no lead operator
                            let noLeadWarning = cardBody.querySelector('.no-lead-warning');
                            if (!noLeadWarning && machine.status !== 'offline') {
                                noLeadWarning = document.createElement('div');
                                noLeadWarning.className = 'no-lead-warning mt-2 small text-danger';
                                noLeadWarning.innerHTML = '<i class="fas fa-exclamation-triangle me-1"></i>No lead operator assigned';
                                cardBody.appendChild(noLeadWarning);
                            }
                        }
                    } else {
                        // Remove user info if no user is logged in
                        const userElement = cardBody.querySelector('.user-info');
                        const activityElement = cardBody.querySelector('.activity-info');
                        const leadBadge = card.querySelector('.lead-operator-badge');
                        const leadInfoElement = cardBody.querySelector('.lead-info');
                        const noLeadWarning = cardBody.querySelector('.no-lead-warning');
                        
                        if (userElement) {
                            cardBody.removeChild(userElement);
                        }
                        
                        if (activityElement) {
                            cardBody.removeChild(activityElement);
                        }
                        
                        if (leadBadge) {
                            card.removeChild(leadBadge);
                        }
                        
                        if (leadInfoElement) {
                            cardBody.removeChild(leadInfoElement);
                        }
                        
                        if (noLeadWarning) {
                            cardBody.removeChild(noLeadWarning);
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
                    
                    // Display active sessions for this machine if any
                    if (machine.active_sessions && machine.active_sessions.length > 0) {
                        // Add sessions count badge
                        let sessionsBadge = card.querySelector('.sessions-count-badge');
                        if (!sessionsBadge) {
                            sessionsBadge = document.createElement('div');
                            sessionsBadge.className = 'sessions-count-badge';
                            card.appendChild(sessionsBadge);
                        }
                        sessionsBadge.textContent = `${machine.active_sessions.length} ${machine.active_sessions.length === 1 ? 'session' : 'sessions'}`;
                        
                        // Create or update sessions list
                        let sessionsListElement = cardBody.querySelector('.session-user-list');
                        if (!sessionsListElement) {
                            sessionsListElement = document.createElement('div');
                            sessionsListElement.className = 'session-user-list';
                            cardBody.appendChild(sessionsListElement);
                        }
                        
                        // Clear existing sessions and add new ones
                        sessionsListElement.innerHTML = '';
                        if (machine.active_sessions.length > 0) {
                            const sessionHeader = document.createElement('p');
                            sessionHeader.className = 'mb-1 fw-bold';
                            sessionHeader.innerHTML = '<i class="fas fa-users me-2"></i>Active Sessions:';
                            sessionsListElement.appendChild(sessionHeader);
                            
                            machine.active_sessions.forEach(session => {
                                const sessionElement = document.createElement('div');
                                sessionElement.className = 'session-user';
                                sessionElement.innerHTML = `
                                    <small class="me-1">•</small>
                                    <small>${session.user_name}${session.is_lead ? ' <i class="fas fa-star lead-operator-icon"></i>' : ''}</small>
                                    <small class="ms-auto text-muted">${formatTimeSince(new Date(session.start_time))}</small>
                                `;
                                sessionsListElement.appendChild(sessionElement);
                            });
                        }
                    } else {
                        // Remove sessions badge and list if no active sessions
                        const sessionsBadge = card.querySelector('.sessions-count-badge');
                        const sessionsListElement = cardBody.querySelector('.session-user-list');
                        
                        if (sessionsBadge) {
                            card.removeChild(sessionsBadge);
                        }
                        
                        if (sessionsListElement) {
                            cardBody.removeChild(sessionsListElement);
                        }
                    }
                }
            });
            
            // Update dashboard statistics
            updateStatistics(machines);

            // Update area and zone summary
            updateAreaZoneSummary(machines);
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
    let leadOperators = 0;
    
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
        
        // Count active users and lead operators
        if (machine.current_user) {
            activeUsers++;
            if (machine.is_lead_operator) {
                leadOperators++;
            }
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
    
    // Update lead operators count if element exists
    const leadOpsElement = document.getElementById('lead-operators-count');
    if (leadOpsElement) {
        leadOpsElement.textContent = leadOperators;
    }
}

/**
 * Updates the area and zone summary section with active machines and sessions
 * @param {Array} machines - Array of machine objects
 */
function updateAreaZoneSummary(machines) {
    // First, fetch the area and zone hierarchy data
    fetch('/api/areas/hierarchy')
        .then(response => response.json())
        .then(hierarchy => {
            const summaryContainer = document.getElementById('area-zone-summary');
            if (!summaryContainer) return;
            
            // Clear existing summary
            summaryContainer.innerHTML = '';
            
            // Create summary for each area
            hierarchy.forEach(area => {
                const areaCard = document.createElement('div');
                areaCard.className = 'card mb-3';
                
                // Count active machines and sessions in this area
                const areaMachines = machines.filter(machine => machine.area_id === area.id);
                const activeCount = areaMachines.filter(machine => machine.status === 'active').length;
                const totalSessions = areaMachines.reduce((sum, machine) => 
                    sum + (machine.active_sessions ? machine.active_sessions.length : 0), 0);
                
                // Create area header with counts
                areaCard.innerHTML = `
                    <div class="card-header d-flex justify-content-between align-items-center">
                        <h5 class="mb-0">${area.name}</h5>
                        <div>
                            <span class="badge bg-success me-2">${activeCount} Active</span>
                            <span class="badge bg-primary">${totalSessions} Sessions</span>
                        </div>
                    </div>
                    <div class="card-body" id="area-${area.id}-body"></div>
                `;
                
                summaryContainer.appendChild(areaCard);
                const areaBody = document.getElementById(`area-${area.id}-body`);
                
                // Add zone details within this area
                if (area.zones && area.zones.length > 0) {
                    const zonesList = document.createElement('div');
                    zonesList.className = 'list-group';
                    
                    area.zones.forEach(zone => {
                        const zoneMachines = machines.filter(machine => machine.zone_id === zone.id);
                        const zoneActiveCount = zoneMachines.filter(machine => machine.status === 'active').length;
                        const zoneTotalSessions = zoneMachines.reduce((sum, machine) => 
                            sum + (machine.active_sessions ? machine.active_sessions.length : 0), 0);
                        
                        // Only show zones with active machines or sessions
                        if (zoneActiveCount > 0 || zoneTotalSessions > 0) {
                            const zoneItem = document.createElement('div');
                            zoneItem.className = 'list-group-item d-flex justify-content-between align-items-center';
                            zoneItem.innerHTML = `
                                <div>
                                    <i class="fas fa-map-marker-alt me-2 text-primary"></i>
                                    ${zone.name}
                                </div>
                                <div>
                                    <span class="badge bg-success me-2">${zoneActiveCount} Active</span>
                                    <span class="badge bg-primary">${zoneTotalSessions} Sessions</span>
                                </div>
                            `;
                            zonesList.appendChild(zoneItem);
                        }
                    });
                    
                    if (zonesList.children.length > 0) {
                        areaBody.appendChild(zonesList);
                    } else {
                        areaBody.innerHTML = '<p class="text-muted">No active machines in this area</p>';
                    }
                } else {
                    areaBody.innerHTML = '<p class="text-muted">No zones defined in this area</p>';
                }
            });
        })
        .catch(error => {
            console.error('Error updating area/zone summary:', error);
        });
}

/**
 * Formats a timestamp as a "time ago" string
 * @param {Date} date - The date to format
 * @returns {string} - Formatted time string (e.g. "5m ago")
 */
function formatTimeSince(date) {
    const seconds = Math.floor((new Date() - date) / 1000);
    
    let interval = Math.floor(seconds / 31536000);
    if (interval > 1) return interval + 'y';
    
    interval = Math.floor(seconds / 2592000);
    if (interval > 1) return interval + 'mo';
    
    interval = Math.floor(seconds / 86400);
    if (interval > 1) return interval + 'd';
    
    interval = Math.floor(seconds / 3600);
    if (interval > 1) return interval + 'h';
    
    interval = Math.floor(seconds / 60);
    if (interval > 1) return interval + 'm';
    
    return 'just now';
}

/**
 * Format all timestamps on the page
 */
function formatTimestamps() {
    document.querySelectorAll('.last-activity').forEach(element => {
        if (element.dataset.timestamp) {
            const timestamp = new Date(element.dataset.timestamp);
            element.textContent = timestamp.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
        }
    });
}
