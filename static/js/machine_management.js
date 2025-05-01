// Machine and Zone Management functionality for NooyenUSATracker

document.addEventListener('DOMContentLoaded', function() {
    // Event handlers for edit zone modal
    document.querySelectorAll('.edit-zone-btn').forEach(button => {
        button.addEventListener('click', function() {
            const zoneId = this.getAttribute('data-zone-id');
            const zoneName = this.getAttribute('data-zone-name');
            const zoneDescription = this.getAttribute('data-zone-description');
            
            // Populate form fields
            document.getElementById('edit_zone_name').value = zoneName;
            document.getElementById('edit_zone_description').value = zoneDescription;
            
            // Set form action URL
            document.getElementById('editZoneForm').action = `/zones/edit/${zoneId}`;
        });
    });
    
    // Event handlers for delete zone modal
    document.querySelectorAll('.delete-zone-btn').forEach(button => {
        button.addEventListener('click', function() {
            const zoneId = this.getAttribute('data-zone-id');
            const zoneName = this.getAttribute('data-zone-name');
            
            // Populate modal text
            document.getElementById('delete_zone_name').textContent = zoneName;
            
            // Set form action URL
            document.getElementById('deleteZoneForm').action = `/zones/delete/${zoneId}`;
        });
    });
    
    // Event handlers for edit machine modal
    document.querySelectorAll('.edit-machine-btn').forEach(button => {
        button.addEventListener('click', function() {
            const machineId = this.getAttribute('data-machine-id');
            const machineMid = this.getAttribute('data-machine-mid');
            const machineName = this.getAttribute('data-machine-name');
            const machineDescription = this.getAttribute('data-machine-description');
            const machineZoneId = this.getAttribute('data-machine-zone-id');
            const machineIpAddress = this.getAttribute('data-machine-ip-address');
            
            // Populate form fields
            document.getElementById('edit_machine_id').value = machineMid;
            document.getElementById('edit_machine_name').value = machineName;
            document.getElementById('edit_machine_description').value = machineDescription;
            document.getElementById('edit_machine_zone_id').value = machineZoneId;
            document.getElementById('edit_machine_ip_address').value = machineIpAddress;
            
            // Set form action URL
            document.getElementById('editMachineForm').action = `/machines/edit/${machineId}`;
        });
    });
    
    // Event handlers for delete machine modal
    document.querySelectorAll('.delete-machine-btn').forEach(button => {
        button.addEventListener('click', function() {
            const machineId = this.getAttribute('data-machine-id');
            const machineName = this.getAttribute('data-machine-name');
            
            // Populate modal text
            document.getElementById('delete_machine_name').textContent = machineName;
            
            // Set form action URL
            document.getElementById('deleteMachineForm').action = `/machines/delete/${machineId}`;
        });
    });
    
    // Validate machine ID input for 2-digit format
    const machineIdInput = document.getElementById('machine_id');
    if (machineIdInput) {
        machineIdInput.addEventListener('input', function() {
            // Remove any non-digit characters
            this.value = this.value.replace(/\D/g, '');
            
            // Limit to 2 digits
            if (this.value.length > 2) {
                this.value = this.value.substring(0, 2);
            }
        });
    }
    
    // IP address validation
    const ipAddressInputs = document.querySelectorAll('input[name="ip_address"]');
    ipAddressInputs.forEach(input => {
        input.addEventListener('blur', function() {
            if (this.value && !isValidIpAddress(this.value)) {
                this.classList.add('is-invalid');
                
                // Add invalid feedback if it doesn't exist
                let feedbackElement = this.nextElementSibling;
                if (!feedbackElement || !feedbackElement.classList.contains('invalid-feedback')) {
                    feedbackElement = document.createElement('div');
                    feedbackElement.className = 'invalid-feedback';
                    feedbackElement.textContent = 'Please enter a valid IP address (e.g., 192.168.1.100)';
                    this.parentNode.appendChild(feedbackElement);
                }
            } else {
                this.classList.remove('is-invalid');
                
                // Remove invalid feedback if it exists
                const feedbackElement = this.nextElementSibling;
                if (feedbackElement && feedbackElement.classList.contains('invalid-feedback')) {
                    feedbackElement.remove();
                }
            }
        });
    });
});

/**
 * Validates an IP address string
 * @param {string} ip - The IP address to validate
 * @returns {boolean} True if the IP address is valid
 */
function isValidIpAddress(ip) {
    const ipRegex = /^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
    return ipRegex.test(ip);
}
