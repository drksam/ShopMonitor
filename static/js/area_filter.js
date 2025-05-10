/**
 * Area Filtering Module
 * 
 * This module handles area-based filtering throughout the application.
 * It loads the user's accessible areas into the area selector dropdown
 * and applies filtering based on the selected area.
 */
document.addEventListener('DOMContentLoaded', function() {
    // Initialize the area selector
    initAreaSelector();
    
    // Apply area filtering to tables or other elements with the data-area-filterable attribute
    setupAreaFiltering();
});

/**
 * Initialize the area selector dropdown in the navbar
 */
function initAreaSelector() {
    const areaSelector = document.getElementById('area-selector-container');
    const areaDropdownMenu = document.getElementById('area-dropdown-menu');
    
    if (!areaSelector || !areaDropdownMenu) return;
    
    // Fetch areas the current user has access to
    fetch('/api/areas')
        .then(response => response.json())
        .then(areas => {
            if (areas && areas.length > 0) {
                // Show the area selector if areas are available
                areaSelector.style.display = 'block';
                
                // Add each area to the dropdown
                areas.forEach(area => {
                    const li = document.createElement('li');
                    li.innerHTML = `<a class="dropdown-item area-filter-option" href="#" 
                        data-area-id="${area.id}" data-area-name="${area.name}">${area.name}</a>`;
                    areaDropdownMenu.appendChild(li);
                });
                
                // Setup event listeners for area selection
                document.querySelectorAll('.area-filter-option').forEach(option => {
                    option.addEventListener('click', function(e) {
                        e.preventDefault();
                        const areaId = this.getAttribute('data-area-id');
                        const areaName = this.getAttribute('data-area-name');
                        
                        // Update the current area name display
                        document.getElementById('current-area-name').textContent = areaName;
                        
                        // Set the area ID in localStorage
                        localStorage.setItem('selectedAreaId', areaId);
                        localStorage.setItem('selectedAreaName', areaName);
                        
                        // Apply filtering
                        applyAreaFilter(areaId);
                        
                        // Dispatch a custom event for area change
                        document.dispatchEvent(new CustomEvent('areaFilterChanged', {
                            detail: { areaId: areaId, areaName: areaName }
                        }));
                    });
                });
                
                // Check if there's a stored selection
                const storedAreaId = localStorage.getItem('selectedAreaId');
                const storedAreaName = localStorage.getItem('selectedAreaName');
                
                if (storedAreaId && storedAreaName) {
                    document.getElementById('current-area-name').textContent = storedAreaName;
                    applyAreaFilter(storedAreaId);
                }
            }
        })
        .catch(error => {
            console.error('Error loading areas:', error);
        });
}

/**
 * Apply area filtering to filterable elements
 */
function applyAreaFilter(areaId) {
    // If "All Areas" is selected (areaId = 0), show everything
    const showAll = areaId === '0';
    
    // Apply filtering to tables
    document.querySelectorAll('table[data-area-filterable] tbody tr').forEach(row => {
        const rowAreaId = row.getAttribute('data-area-id');
        
        if (showAll || !rowAreaId || rowAreaId === areaId) {
            row.style.display = '';  // Show the row
        } else {
            row.style.display = 'none';  // Hide the row
        }
    });
    
    // Apply filtering to card elements
    document.querySelectorAll('.card[data-area-filterable]').forEach(card => {
        const cardAreaId = card.getAttribute('data-area-id');
        
        if (showAll || !cardAreaId || cardAreaId === areaId) {
            card.style.display = '';  // Show the card
        } else {
            card.style.display = 'none';  // Hide the card
        }
    });
    
    // Apply filtering to list items
    document.querySelectorAll('.list-group-item[data-area-filterable]').forEach(item => {
        const itemAreaId = item.getAttribute('data-area-id');
        
        if (showAll || !itemAreaId || itemAreaId === areaId) {
            item.style.display = '';  // Show the item
        } else {
            item.style.display = 'none';  // Hide the item
        }
    });
}

/**
 * Setup area filtering for all filterable elements
 */
function setupAreaFiltering() {
    // Check if there's a stored area selection
    const storedAreaId = localStorage.getItem('selectedAreaId');
    const storedAreaName = localStorage.getItem('selectedAreaName');
    
    if (storedAreaId && storedAreaName) {
        document.getElementById('current-area-name').textContent = storedAreaName;
        applyAreaFilter(storedAreaId);
    }
    
    // Listen for the areaFilterChanged event
    document.addEventListener('areaFilterChanged', function(e) {
        applyAreaFilter(e.detail.areaId);
    });
}