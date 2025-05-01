// dashboard-charts.js - Charts for the dashboard

document.addEventListener('DOMContentLoaded', function() {
    if (!document.getElementById('machine-activity-chart')) {
        return; // Skip if the chart element isn't on the page
    }

    // Machine activity chart data
    const activityLabels = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday'];
    
    const activityData = {
        labels: activityLabels,
        datasets: [
            {
                label: 'Machine Activity (hours)',
                data: [5, 7, 6, 8, 9, 3, 1],
                borderColor: 'rgb(75, 192, 192)',
                backgroundColor: 'rgba(75, 192, 192, 0.2)',
                tension: 0.1,
                fill: true
            },
            {
                label: 'User Logins',
                data: [12, 15, 18, 14, 20, 8, 4],
                borderColor: 'rgb(54, 162, 235)',
                backgroundColor: 'rgba(54, 162, 235, 0.2)',
                tension: 0.1,
                fill: true
            }
        ]
    };

    const activityConfig = {
        type: 'line',
        data: activityData,
        options: {
            responsive: true,
            plugins: {
                legend: {
                    position: 'top',
                },
                title: {
                    display: true,
                    text: 'Weekly Machine Activity'
                }
            },
            scales: {
                y: {
                    beginAtZero: true
                }
            }
        }
    };

    // Machine usage chart data by zone
    const zoneData = {
        labels: ['Shop Floor', 'Assembly Area', 'Finishing Area', 'Quality Control'],
        datasets: [{
            label: 'Usage Hours',
            data: [24, 16, 12, 8],
            backgroundColor: [
                'rgba(255, 99, 132, 0.2)',
                'rgba(54, 162, 235, 0.2)',
                'rgba(255, 206, 86, 0.2)',
                'rgba(75, 192, 192, 0.2)'
            ],
            borderColor: [
                'rgba(255, 99, 132, 1)',
                'rgba(54, 162, 235, 1)',
                'rgba(255, 206, 86, 1)',
                'rgba(75, 192, 192, 1)'
            ],
            borderWidth: 1
        }]
    };

    const zoneConfig = {
        type: 'bar',
        data: zoneData,
        options: {
            responsive: true,
            plugins: {
                legend: {
                    position: 'top',
                },
                title: {
                    display: true,
                    text: 'Machine Usage by Zone'
                }
            },
            scales: {
                y: {
                    beginAtZero: true
                }
            }
        }
    };

    // Create charts if Chart.js is loaded
    if (typeof Chart !== 'undefined') {
        const activityChart = new Chart(
            document.getElementById('machine-activity-chart'),
            activityConfig
        );

        const zoneChart = new Chart(
            document.getElementById('zone-usage-chart'),
            zoneConfig
        );
    } else {
        console.error('Chart.js not loaded');
        
        // Display error message in chart containers
        const chartContainers = document.querySelectorAll('.chart-container');
        chartContainers.forEach(container => {
            container.innerHTML = '<div class="alert alert-warning">Charts unavailable. Chart.js library not loaded.</div>';
        });
    }
});

// Function to load machine activity data from server
function loadMachineActivityData() {
    fetch('/api/machine-activity')
        .then(response => response.json())
        .then(data => {
            console.log('Activity data loaded:', data);
            // Update charts with real data (implement when API is ready)
        })
        .catch(error => {
            console.error('Error loading machine activity data:', error);
        });
}

// Load real data when API is ready
// loadMachineActivityData();