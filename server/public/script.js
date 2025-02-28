document.addEventListener('DOMContentLoaded', function() {
    // --- Fetch and display raw usage data (table) ---
    fetch('/usage/raw') // Fetch raw data from /usage/raw endpoint
        .then(response => response.json())
        .then(data => {
            const tableBody = document.querySelector('#usage-table tbody');
            if (data && data.length > 0) {
                data.forEach(row => {
                    let tr = document.createElement('tr');
                    tr.innerHTML = `
                        <td>${row.id}</td>
                        <td>${row.app_name}</td>
                        <td>${row.fqdn}</td>
                        <td>${row.local_ip}</td>
                        <td>${row.os_release}</td>
                        <td>${row.cpu_arch}</td>
                        <td>${row.app_version}</td>
                        <td>${row.timestamp}</td>
                        <td>${row.username}</td>
                    `;
                    tableBody.appendChild(tr);
                });
            } else {
                tableBody.innerHTML = '<tr><td colspan="9" style="text-align:center;">No usage data available.</td></tr>';
            }
        })
        .catch(error => {
            console.error('Error fetching raw usage data:', error);
            const tableBody = document.querySelector('#usage-table tbody');
            tableBody.innerHTML = '<tr><td colspan="9" style="text-align:center; color: red;">Failed to load usage data.</td></tr>';
        });

    // --- Fetch data and create charts ---
    createUsageOverTimeChart();
    createAppPopularityChart();
    createOSDistributionChart();
    createCPUDistributionChart();
    createHostnameUsageChart(); // <-- NEW CHART CREATION FUNCTION CALL

    // --- Chart Creation Functions ---

    function createUsageOverTimeChart() {
        fetch('/api/usage-over-time')
        .then(response => response.json())
        .then(data => {
            const labels = data.map(item => item.usage_date);
            const usageCounts = data.map(item => item.usage_count);

            const ctx = document.getElementById('usageOverTimeChart').getContext('2d');
            new Chart(ctx, {
                type: 'line',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'Usage Count',
                        data: usageCounts,
                        borderColor: 'rgb(75, 192, 192)',
                        tension: 0.1
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        y: {
                            beginAtZero: true,
                            title: {
                                display: true,
                                text: 'Usage Count'
                            }
                        },
                        x: {
                            title: {
                                display: true,
                                text: 'Date'
                            }
                        }
                    }
                }
            });
        })
        .catch(error => console.error('Error fetching usage over time data:', error));
    }

    function createAppPopularityChart() {
    fetch('/api/app-popularity')
    .then(response => response.json())
    .then(data => {
        const labels = data.map(item => item.app_name);
        const usageCounts = data.map(item => item.usage_count);

        const ctx = document.getElementById('appPopularityChart').getContext('2d');
        new Chart(ctx, {
            type: 'bar',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Usage Count',
                    data: usageCounts,
                    backgroundColor: 'rgba(153, 102, 255, 0.7)',
                    borderColor: 'rgba(153, 102, 255, 1)',
                    borderWidth: 1
                }]
            },
            options: { // Line 122 is likely pointing here
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        beginAtZero: true,
                        title: {
                            display: true,
                            text: 'Usage Count'
                        }
                    },
                    x: {
                        title: {
                            display: true,
                            text: 'Application Name'
                        }
                    }
                }
            }
        });
    })
    .catch(error => console.error('Error fetching app popularity data:', error));
}

    function createOSDistributionChart() {
        fetch('/api/os-distribution')
        .then(response => response.json())
        .then(data => {
            // OS Release Mapping (same as server-side)
            const osReleaseMap = {
                "26.00": "v2r3",
                "27.00": "v2r4",
                "28.00": "v2r5",
                "29.00": "v3r1",
                "30.00":  "v3r2"
            };

            const labels = data.map(item => osReleaseMap[item.os_release] || item.os_release);
            const usageCounts = data.map(item => item.usage_count);

            const ctx = document.getElementById('osDistributionChart').getContext('2d');
            new Chart(ctx, {
                type: 'pie',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'OS Distribution',
                        data: usageCounts,
                        backgroundColor: [
                            'rgba(255, 99, 132, 0.7)',
                            'rgba(54, 162, 235, 0.7)',
                            'rgba(255, 206, 86, 0.7)',
                            'rgba(75, 192, 192, 0.7)',
                            'rgba(153, 102, 255, 0.7)',
                            'rgba(255, 159, 64, 0.7)',
                            // Add more colors if needed
                        ],
                        borderColor: 'rgba(255, 255, 255, 1)',
                        borderWidth: 1
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                }
            });
        })
        .catch(error => console.error('Error fetching OS distribution data:', error));
    }

    function createCPUDistributionChart() {
        fetch('/api/cpu-distribution')
        .then(response => response.json())
        .then(data => {
            const labels = data.map(item => item.cpu_arch);
            const usageCounts = data.map(item => item.usage_count);

            const ctx = document.getElementById('cpuDistributionChart').getContext('2d');
            new Chart(ctx, {
                type: 'pie',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'CPU Arch Distribution',
                        data: usageCounts,
                        backgroundColor: [
                            'rgba(240, 80, 80, 0.7)',
                            'rgba(80, 240, 80, 0.7)',
                            'rgba(80, 80, 240, 0.7)',
                            'rgba(240, 240, 80, 0.7)',
                            'rgba(240, 80, 240, 0.7)',
                            'rgba(80, 240, 240, 0.7)',
                            // Add more colors if needed
                        ],
                        borderColor: 'rgba(255, 255, 255, 1)',
                        borderWidth: 1
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                }
            });
        })
        .catch(error => console.error('Error fetching CPU distribution data:', error));
    }

    // --- NEW CHART CREATION FUNCTION for Hostname Usage ---
    function createHostnameUsageChart() {
        fetch('/api/hostname-usage') // Fetch data from the new hostname usage endpoint
        .then(response => response.json())
        .then(data => {
            const labels = data.map(item => item.fqdn); // Use fqdn as labels
            const usageCounts = data.map(item => item.usage_count);

            const ctx = document.getElementById('hostnameUsageChart').getContext('2d');
            new Chart(ctx, {
                type: 'bar', // Or 'pie' if you prefer a pie chart for hostnames
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'Hostname Usage Count',
                        data: usageCounts,
                        backgroundColor: 'rgba(255, 159, 64, 0.7)', // Example color
                        borderColor: 'rgba(255, 159, 64, 1)',
                        borderWidth: 1
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        y: {
                            beginAtZero: true,
                            title: {
                                display: true,
                                text: 'Usage Count'
                            }
                        },
                        x: {
                            title: {
                                display: true,
                                text: 'Hostname (FQDN)'
                            }
                        }
                    }
                }
            });
        })
        .catch(error => console.error('Error fetching hostname usage data:', error));
    }

});
