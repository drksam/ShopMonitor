// User Management functionality for NooyenUSATracker

document.addEventListener('DOMContentLoaded', function() {
    // Event handlers for edit RFID user modal
    document.querySelectorAll('.edit-rfid-user-btn').forEach(button => {
        button.addEventListener('click', function() {
            const userId = this.getAttribute('data-user-id');
            const userName = this.getAttribute('data-user-name');
            const userEmail = this.getAttribute('data-user-email');
            const userActive = this.getAttribute('data-user-active') === 'True';
            
            // Populate form fields
            document.getElementById('edit_name').value = userName;
            document.getElementById('edit_email').value = userEmail;
            document.getElementById('edit_active').checked = userActive;
            
            // Set form action URL
            document.getElementById('editRfidUserForm').action = `/users/edit_rfid/${userId}`;
        });
    });
    
    // Event handlers for delete RFID user modal
    document.querySelectorAll('.delete-rfid-user-btn').forEach(button => {
        button.addEventListener('click', function() {
            const userId = this.getAttribute('data-user-id');
            const userName = this.getAttribute('data-user-name');
            
            // Populate modal text
            document.getElementById('delete_user_name').textContent = userName;
            
            // Set form action URL
            document.getElementById('deleteRfidUserForm').action = `/users/delete_rfid/${userId}`;
        });
    });
    
    // Validate RFID tag input
    const rfidTagInput = document.getElementById('rfid_tag');
    if (rfidTagInput) {
        rfidTagInput.addEventListener('input', function() {
            // Allow only alphanumeric characters
            this.value = this.value.replace(/[^a-zA-Z0-9]/g, '');
        });
    }
    
    // Add authorization tooltips
    const authButtons = document.querySelectorAll('a[href*="authorizations"]');
    authButtons.forEach(button => {
        button.setAttribute('title', 'Manage machine authorizations');
    });
    
    // Password strength validation for admin users
    const passwordInput = document.getElementById('password');
    if (passwordInput) {
        passwordInput.addEventListener('input', function() {
            const password = this.value;
            const strength = calculatePasswordStrength(password);
            
            // Remove existing feedback
            const existingFeedback = this.nextElementSibling;
            if (existingFeedback && existingFeedback.classList.contains('password-strength')) {
                existingFeedback.remove();
            }
            
            // Add new feedback
            const feedbackElement = document.createElement('div');
            feedbackElement.className = 'password-strength form-text mt-1';
            
            if (strength === 0) {
                feedbackElement.textContent = 'Password is too weak';
                feedbackElement.style.color = '#dc3545'; // Bootstrap danger color
            } else if (strength === 1) {
                feedbackElement.textContent = 'Password strength: Moderate';
                feedbackElement.style.color = '#ffc107'; // Bootstrap warning color
            } else {
                feedbackElement.textContent = 'Password strength: Strong';
                feedbackElement.style.color = '#198754'; // Bootstrap success color
            }
            
            this.parentNode.appendChild(feedbackElement);
        });
    }
});

/**
 * Calculates password strength
 * @param {string} password - The password to evaluate
 * @returns {number} Strength level: 0 (weak), 1 (moderate), or 2 (strong)
 */
function calculatePasswordStrength(password) {
    if (password.length < 6) {
        return 0; // Weak
    }
    
    let strength = 0;
    
    // Check for different character types
    if (/[A-Z]/.test(password)) strength++; // Has uppercase
    if (/[a-z]/.test(password)) strength++; // Has lowercase
    if (/[0-9]/.test(password)) strength++; // Has number
    if (/[^A-Za-z0-9]/.test(password)) strength++; // Has special character
    
    if (password.length >= 8 && strength >= 3) {
        return 2; // Strong
    } else if (password.length >= 6 && strength >= 2) {
        return 1; // Moderate
    } else {
        return 0; // Weak
    }
}
