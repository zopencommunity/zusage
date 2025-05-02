document.addEventListener('DOMContentLoaded', function() {
    // --- Set default date to current date ---
    const dailyDateInput = document.getElementById('daily-data-date');
    const today = new Date();
    const year = today.getFullYear();
    const month = String(today.getMonth() + 1).padStart(2, '0');
    const day = String(today.getDate()).padStart(2, '0');
    const currentDate = `${year}-${month}-${day}`;
    dailyDateInput.value = currentDate;

    // --- Fetch data and create charts ---
    createUsageOverTimeChart();
    createAppPopularityChart();
    createOSDistributionChart();
    createCPUDistributionChart();
    createHostnameUsageChart();

    // --- Chart Creation Functions ---
    let usageOverTimeChart; // Declare chart variables outside functions to manage destruction
    let appPopularityChart;
    let osDistributionChart;
    let cpuDistributionChart;
    let hostnameUsageChart;


    function createUsageOverTimeChart() {
        fetch('/api/usage-over-time')
        .then(response => response.json())
        .then(data => {
            const labels = data.map(item => item.usage_date);
            const usageCounts = data.map(item => item.usage_count);

            const ctx = document.getElementById('usageOverTimeChart').getContext('2d');
            if (usageOverTimeChart) {
                usageOverTimeChart.destroy(); // Destroy existing chart if it exists
            }
            usageOverTimeChart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'Daily Usage Count',
                        data: usageCounts,
                        borderColor: 'rgba(54, 162, 235, 1)',
                        borderWidth: 2,
                        fill: false
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        y: {
                            type: 'logarithmic', // Use logarithmic scale for y-axis
                            position: 'left',
                            title: {
                                display: true,
                                text: 'Usage Count' // Removed "(Logarithmic Scale)" from title
                            }
                        },
                        x: {
                            title: {
                                display: true,
                                text: 'Date'
                            }
                        }
                    },
                    plugins: {
                        legend: {
                            display: true,
                            position: 'top',
                        },
                        title: {
                            display: true,
                            text: 'Daily Usage Over Time' // Removed "(Logarithmic Y-Axis)" from chart title
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
        if (appPopularityChart) {
            appPopularityChart.destroy(); // Destroy existing chart if it exists
        }
        appPopularityChart = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Usage Count',
                    data: usageCounts,
                    backgroundColor: 'rgba(255, 99, 132, 0.7)',
                    borderColor: 'rgba(255, 99, 132, 1)',
                    borderWidth: 1
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        type: 'logarithmic', // Use logarithmic scale for y-axis
                        position: 'left',
                        title: {
                            display: true,
                            text: 'Usage Count'
                        }
                    },
                    x: {
                        title: {
                            display: true,
                            text: 'Application Name'
                        },
                        ticks: { // Add ticks configuration for x-axis
                            font: {
                                weight: 'bold' // Set font weight to bold for x-axis ticks
                            }
                        }
                    }
                },
                plugins: {
                    legend: {
                        display: false
                    },
                    title: {
                        display: true,
                        text: 'Application Popularity'
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
            if (osDistributionChart) {
                osDistributionChart.destroy(); // Destroy existing chart if it exists
            }
            osDistributionChart = new Chart(ctx, {
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
                    plugins: {
                        legend: {
                            position: 'right'
                        },
                        title: {
                            display: true,
                            text: 'OS Distribution'
                        },
                        tooltip: { // Customize tooltip to show logarithmic context
                            callbacks: {
                                label: (context) => {
                                    let label = context.label || '';
                                    if (label) {
                                        label += ': ';
                                    }
                                    if (context.parsed && context.parsed.valueOf() != null) {
                                        label += 'Count: ' + context.parsed.valueOf();
                                    }
                                    return label;
                                }
                            }
                        }
                    }
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
            if (cpuDistributionChart) {
                cpuDistributionChart.destroy(); // Destroy existing chart if it exists
            }
            cpuDistributionChart = new Chart(ctx, {
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
                    plugins: {
                        legend: {
                            position: 'right'
                        },
                        title: {
                            display: true,
                            text: 'CPU Architecture Distribution'
                        },
                        tooltip: { // Customize tooltip to show logarithmic context
                            callbacks: {
                                label: (context) => {
                                    let label = context.label || '';
                                    if (label) {
                                        label += ': ';
                                    }
                                    if (context.parsed && context.parsed.valueOf() != null) {
                                        label += 'Count: ' + context.parsed.valueOf();
                                    }
                                    return label;
                                }
                            }
                        }
                    }
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
            if (hostnameUsageChart) {
                hostnameUsageChart.destroy(); // Destroy existing chart if it exists
            }
            hostnameUsageChart = new Chart(ctx, {
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
                            type: 'logarithmic', // Use logarithmic scale for y-axis
                            position: 'left',
                            title: {
                                display: true,
                                text: 'Usage Count' // Removed "(Logarithmic Scale)" from title
                            }
                        },
                        x: {
                            title: {
                                display: true,
                                text: 'Hostname (FQDN)'
                            }
                        }
                    },
                    plugins: {
                        legend: {
                            display: false
                        },
                        title: {
                            display: true,
                            text: 'Hostname Usage Distribution' // Removed "(Logarithmic Y-Axis)" from chart title
                        }
                    }
                }
            });
        })
        .catch(error => console.error('Error fetching hostname usage data:', error));
    }

    // --- Daily Raw Data Fetching ---
    const fetchDailyDataButton = document.getElementById('fetch-daily-data');
    const dailyDataTableBody = document.querySelector('#daily-usage-table tbody');

    fetchDailyDataButton.addEventListener('click', () => {
        const selectedDate = dailyDateInput.value;
        if (!selectedDate) {
            alert('Please select a date.');
            return;
        }

        // Clear previous data and show loading message
        dailyDataTableBody.innerHTML = '<tr><td colspan="9" style="text-align:center;">Loading data...</td></tr>';

        fetch(`/usage/daily-raw/${selectedDate}`)
            .then(response => response.json())
            .then(data => {
                dailyDataTableBody.innerHTML = ''; // Clear loading message

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
                        dailyDataTableBody.appendChild(tr);
                    });
                } else {
                    dailyDataTableBody.innerHTML = '<tr><td colspan="9" style="text-align:center;">No usage data available for this date.</td></tr>';
                }
            })
            .catch(error => {
                console.error('Error fetching daily usage data:', error);
                dailyDataTableBody.innerHTML = '<tr><td colspan="9" style="text-align:center; color: red;">Failed to load data.</td></tr>';
            });
    });

    // --- Custom SQL Query Handling ---
    const executeSqlQueryButton = document.getElementById('execute-sql-query');
    const customSqlQueryTextarea = document.getElementById('custom-sql-query');
    const customSqlResultsTableBody = document.querySelector('#custom-sql-results-table tbody');
    const customSqlResultsTableHeaderRow = document.querySelector('#custom-sql-results-table thead tr');

    executeSqlQueryButton.addEventListener('click', () => {
        const sqlQuery = customSqlQueryTextarea.value;

        if (!sqlQuery.trim()) {
            alert('Please enter an SQL query.');
            return;
        }

        customSqlResultsTableBody.innerHTML = '<tr><td colspan="99" style="text-align:center;">Loading query results...</td></tr>'; // Clear and show loading
        customSqlResultsTableHeaderRow.innerHTML = ''; // Clear previous headers

        fetch(`/usage/custom-query?sql=${encodeURIComponent(sqlQuery)}`) // Send SQL query to the server
            .then(response => response.json())
            .then(data => {
                customSqlResultsTableBody.innerHTML = ''; // Clear loading msg
                customSqlResultsTableHeaderRow.innerHTML = ''; // Ensure headers are clear before re-populating

                if (data && data.length > 0) {
                    // Create table headers dynamically
                    const headers = Object.keys(data[0]); // Get column names from the first row
                    headers.forEach(headerText => {
                        const headerElement = document.createElement('th');
                        headerElement.textContent = headerText;
                        customSqlResultsTableHeaderRow.appendChild(headerElement);
                    });

                    // Populate table rows
                    data.forEach(row => {
                        const dataRow = document.createElement('tr');
                        headers.forEach(header => {
                            const cell = document.createElement('td');
                            cell.textContent = row[header];
                            dataRow.appendChild(cell);
                        });
                        customSqlResultsTableBody.appendChild(dataRow);
                    });
                } else {
                    customSqlResultsTableBody.innerHTML = '<tr><td colspan="99" style="text-align:center;">No results found or query returned no data.</td></tr>';
                }
            })
            .catch(error => {
                console.error('Error executing custom SQL query:', error);
                customSqlResultsTableBody.innerHTML = '<tr><td colspan="99" style="text-align:center; color: red;">Failed to execute query. Check console for details.</td></tr>';
            });
    });

});

function populateToolDropdown() {
    const toolSelect = document.getElementById('tool-select');
    fetch('/api/tools') // New API endpoint to get list of tools
        .then(response => response.json())
        .then(tools => {
            tools.forEach(tool => {
                const option = document.createElement('option');
                option.value = tool.app_name; // Use app_name as value
                option.textContent = tool.app_name; // Display app_name
                toolSelect.appendChild(option);
            });
        })
        .catch(error => console.error('Error fetching tool list:', error));
}

function fetchUserUsageForTool(toolName) {
    const userUsageTableBody = document.querySelector('#tool-user-usage-table tbody'); // Target table body now
    const loadingMessage = document.getElementById('tool-usage-loading-message');

    userUsageTableBody.innerHTML = ''; // Clear previous table rows
    loadingMessage.style.display = 'block'; // Show loading message

    fetch(`/api/tool-user-usage?toolName=${encodeURIComponent(toolName)}`)
        .then(response => response.json())
        .then(usageData => {
            loadingMessage.style.display = 'none'; // Hide loading message

            if (usageData && usageData.length > 0) {
                usageData.forEach(item => {
                    const tableRow = document.createElement('tr'); // Create a table row 'tr'
                    tableRow.innerHTML = `
                        <td>${item.username}</td>
                        <td>${item.fqdn}</td>
                        <td>${item.usage_count}</td>
                    `; // Create table data cells 'td' for each item
                    userUsageTableBody.appendChild(tableRow); // Append row to table body
                });
            } else {
                userUsageTableBody.innerHTML = '<tr><td colspan="3" style="text-align:center;">No usage data found for this tool.</td></tr>'; // No data message in a table row
            }
        })
        .catch(error => {
            loadingMessage.style.display = 'none'; // Ensure loading message is hidden on error
            userUsageTableBody.innerHTML = '<tr><td colspan="3" style="text-align:center; color: red;">Error fetching user usage data.</td></tr>'; // Error message in a table row
            console.error('Error fetching user usage data:', error);
        });
}

// --- Event listener for tool dropdown ---
document.getElementById('tool-select').addEventListener('change', function() {
    const selectedTool = this.value;
    if (selectedTool) {
        fetchUserUsageForTool(selectedTool);
    } else {
        document.getElementById('user-usage-list').innerHTML = ''; // Clear list if no tool selected
    }
});

// --- Call populateToolDropdown on page load (after DOM is ready) ---
populateToolDropdown();
