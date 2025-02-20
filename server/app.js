const express = require('express');
const bodyParser = require('body-parser');
const sqlite3 = require('sqlite3').verbose();
const os = require('os');
const fs = require('fs');
const path = require('path');

// Initialize the app and database
const app = express();
const dbFilePath = path.join(__dirname, 'usage_data.db');
const backupFilePath = `${dbFilePath}.bak`;
const weeklyBackupDir = path.join(__dirname, 'weekly_backups');

// Ensure weekly backup directory exists
if (!fs.existsSync(weeklyBackupDir)) {
    fs.mkdirSync(weeklyBackupDir, { recursive: true });
}

const db = new sqlite3.Database(dbFilePath);

// Middleware
app.use(bodyParser.json());
app.use(express.static(path.join(__dirname, 'public'))); // Serve static files from 'public' directory

// Create or migrate the database schema
db.serialize(() => {
    db.run(`
        CREATE TABLE IF NOT EXISTS usage (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            app_name TEXT NOT NULL,
            fqdn TEXT NOT NULL,
            local_ip TEXT NOT NULL,
            os_release TEXT NOT NULL,
            cpu_arch TEXT NOT NULL,
            app_version TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            username TEXT NOT NULL
        )
    `);
    console.log('Database schema initialized/verified.');
});


// Function to create a regular backup of the SQLite database (on server events)
function backupDatabaseOnEvent() {
    try {
        if (fs.existsSync(dbFilePath)) {
            fs.copyFileSync(dbFilePath, backupFilePath);
            console.log(`Regular database backup created at: ${backupFilePath}`);
        } else {
            console.warn('No database file found to back up (regular event backup).');
        }
    } catch (error) {
        console.error('Error creating regular database backup:', error);
    }
}

// Function to create a timestamped weekly backup of the SQLite database
function backupDatabaseWeekly() {
    try {
        if (fs.existsSync(dbFilePath)) {
            const now = new Date();
            const timestamp = now.toISOString().split('T')[0]; // CHART-MM-DD format
            const timestampedBackupFilePath = path.join(weeklyBackupDir, `usage_data_backup_${timestamp}.db.bak`);
            fs.copyFileSync(dbFilePath, timestampedBackupFilePath);
            console.log(`Weekly database backup created at: ${timestampedBackupFilePath}`);
        } else {
            console.warn('No database file found to back up (weekly backup).');
        }
    } catch (error) {
        console.error('Error creating weekly database backup:', error);
    }
}

// Validate incoming data
function validateData(data) {
    const requiredFields = [
        'app_name',
        'fqdn',
        'local_ip',
        'os_release',
        'cpu_arch',
        'app_version'
    ];

    for (const field of requiredFields) {
        if (!data[field] || typeof data[field] !== 'string') {
            return false;
        }
    }
    return true;
}

// Endpoint to receive usage data
app.post('/usage', (req, res) => {
    const data = req.body;

    // Validate the data
    if (!validateData(data)) {
        return res.status(400).json({ error: 'Invalid data format.' });
    }

    // Generate server-side timestamp in UTC format (like 'YYYY-MM-DDTHH:MM:SSZ')
    const now = new Date();
    const timestamp = now.toISOString().replace(/\..+/, 'Z'); // Format to match C-code style

    // Insert the data into the database, including server-generated timestamp and username (if provided)
    const query = `
        INSERT INTO usage (app_name, fqdn, local_ip, os_release, cpu_arch, app_version, timestamp, username)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    `;

    db.run(
        query,
        [
            data.app_name,
            data.fqdn,
            data.local_ip,
            data.os_release,
            data.cpu_arch,
            data.app_version,
            timestamp,
            data.username || 'unknown' // Use provided username or default to 'unknown' if not present
        ],
        function (err) {
            if (err) {
                console.error('Database error:', err);
                return res.status(500).json({ error: 'Failed to save data.' });
            }

            console.log('Data inserted with ID:', this.lastID);
            res.status(201).json({ success: true, id: this.lastID });
        }
    );
});

// Endpoint to view all stored data (raw table data - may be used for debugging)
app.get('/usage/raw', (req, res) => {
    db.all('SELECT * FROM usage', [], (err, rows) => {
        if (err) {
            console.error('Database error:', err);
            return res.status(500).json({ error: 'Failed to retrieve data.' });
        }
        res.json(rows);
    });
});

// --- API Endpoints for Charts (Aggregated Data) ---

// OS Release Mapping
const osReleaseMap = {
    "27.00": "v2r4",
    "28.00": "v2r5",
    "29.00": "v3r1",
    "30":    "v3r2"
};

// Endpoint for Usage Over Time chart
app.get('/api/usage-over-time', (req, res) => {
    const query = `
        SELECT DATE(timestamp) AS usage_date, COUNT(*) AS usage_count
        FROM usage
        GROUP BY usage_date
        ORDER BY usage_date
    `;
    db.all(query, [], (err, rows) => {
        if (err) {
            console.error('Database error:', err);
            return res.status(500).json({ error: 'Failed to retrieve usage over time data.' });
        }
        res.json(rows);
    });
});

// Endpoint for Application Popularity chart
app.get('/api/app-popularity', (req, res) => {
    const query = `
        SELECT app_name, COUNT(*) AS usage_count
        FROM usage
        GROUP BY app_name
        ORDER BY usage_count DESC
    `;
    db.all(query, [], (err, rows) => {
        if (err) {
            console.error('Database error:', err);
            return res.status(500).json({ error: 'Failed to retrieve app popularity data.' });
        }
        res.json(rows);
    });
});

// Endpoint for OS Distribution chart with friendly names
app.get('/api/os-distribution', (req, res) => {
    const query = `
        SELECT os_release, COUNT(*) AS usage_count
        FROM usage
        GROUP BY os_release
        ORDER BY usage_count DESC
    `;
    db.all(query, [], (err, rows) => {
        if (err) {
            console.error('Database error:', err);
            return res.status(500).json({ error: 'Failed to retrieve OS distribution data.' });
        }
        // Apply OS Release mapping to labels
        const mappedRows = rows.map(row => ({
            os_release: osReleaseMap[row.os_release] || row.os_release, // Use map or original if not found
            usage_count: row.usage_count
        }));
        res.json(mappedRows);
    });
});

// Endpoint for CPU Architecture Distribution chart
app.get('/api/cpu-distribution', (req, res) => {
    const query = `
        SELECT cpu_arch, COUNT(*) AS usage_count
        FROM usage
        GROUP BY cpu_arch
        ORDER BY usage_count DESC
    `;
    db.all(query, [], (err, rows) => {
        if (err) {
            console.error('Database error:', err);
            return res.status(500).json({ error: 'Failed to retrieve CPU architecture distribution data.' });
        }
        res.json(rows);
    });
});

// --- NEW API Endpoint for Hostname Usage Chart ---
app.get('/api/hostname-usage', (req, res) => {
    const query = `
        SELECT fqdn, COUNT(*) AS usage_count
        FROM usage
        GROUP BY fqdn
        ORDER BY usage_count DESC
    `;
    db.all(query, [], (err, rows) => {
        if (err) {
            console.error('Database error:', err);
            return res.status(500).json({ error: 'Failed to retrieve hostname usage data.' });
        }
        res.json(rows);
    });
});


// Get the local IP address of the machine
function getLocalIpAddress() {
    const interfaces = os.networkInterfaces();
    for (const name of Object.keys(interfaces)) {
        for (const iface of interfaces[name]) {
            if (iface.family === 'IPv4' && !iface.internal) {
                return iface.address;
            }
        }
    }
    return '127.0.0.1'; // Fallback to localhost
}

// Function to schedule weekly backups, running every Sunday at midnight
function scheduleWeeklyBackup() {
    const now = new Date();
    const dayOfWeek = now.getDay(); // 0 for Sunday, 1 for Monday, ..., 6 for Saturday
    const daysUntilSunday = (7 - dayOfWeek) % 7; // Days until next Sunday
    const nextSundayMidnight = new Date(now);
    nextSundayMidnight.setDate(now.getDate() + daysUntilSunday);
    nextSundayMidnight.setHours(0, 0, 0, 0); // Set to midnight

    const timeUntilNextBackup = nextSundayMidnight.getTime() - now.getTime();

    setTimeout(() => {
        backupDatabaseWeekly(); // Run initial weekly backup

        // Set interval for weekly backups (7 days in milliseconds)
        setInterval(backupDatabaseWeekly, 7 * 24 * 60 * 60 * 1000);
        console.log('Weekly database backups scheduled to run every Sunday at midnight.');

    }, timeUntilNextBackup);
}


// Start the server and display the connection info
const PORT = 3000;
app.listen(PORT, () => {
    const ipAddress = getLocalIpAddress();
    console.log(`Server is running! Access it via the following:`);
    console.log(`- Local:   http://localhost:${PORT}`);
    console.log(`- Network: http://${ipAddress}:${PORT}`);

    // Create a regular backup when the server starts
    backupDatabaseOnEvent();

    // Schedule weekly backups
    scheduleWeeklyBackup();
});

// Backup database on process exit or errors (using regular backup function)
process.on('exit', backupDatabaseOnEvent);
process.on('SIGINT', () => {
    console.log('\nGracefully shutting down...');
    backupDatabaseOnEvent();
    process.exit();
});
process.on('uncaughtException', (err) => {
    console.error('Uncaught exception:', err);
    backupDatabaseOnEvent();
    process.exit(1);
});
