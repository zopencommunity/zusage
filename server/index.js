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
const db = new sqlite3.Database(dbFilePath);

// Middleware
app.use(bodyParser.json());

// Create the database schema
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
            timestamp TEXT NOT NULL
        )
    `);
    console.log('Database schema initialized.');
});

// Function to create a backup of the SQLite database
function backupDatabase() {
    try {
        if (fs.existsSync(dbFilePath)) {
            fs.copyFileSync(dbFilePath, backupFilePath);
            console.log(`Database backup created at: ${backupFilePath}`);
        } else {
            console.warn('No database file found to back up.');
        }
    } catch (error) {
        console.error('Error creating database backup:', error);
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
        'app_version',
        'timestamp'
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

    // Insert the data into the database
    const query = `
        INSERT INTO usage (app_name, fqdn, local_ip, os_release, cpu_arch, app_version, timestamp)
        VALUES (?, ?, ?, ?, ?, ?, ?)
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
            data.timestamp
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

// Optional: Endpoint to view all stored data
app.get('/usage', (req, res) => {
    db.all('SELECT * FROM usage', [], (err, rows) => {
        if (err) {
            console.error('Database error:', err);
            return res.status(500).json({ error: 'Failed to retrieve data.' });
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

// Start the server and display the connection info
const PORT = 3000;
app.listen(PORT, () => {
    const ipAddress = getLocalIpAddress();
    console.log(`Server is running! Access it via the following:`);
    console.log(`- Local:   http://localhost:${PORT}`);
    console.log(`- Network: http://${ipAddress}:${PORT}`);

    // Create a backup when the server starts
    backupDatabase();
});

// Backup database on process exit or errors
process.on('exit', backupDatabase);
process.on('SIGINT', () => {
    console.log('\nGracefully shutting down...');
    backupDatabase();
    process.exit();
});
process.on('uncaughtException', (err) => {
    console.error('Uncaught exception:', err);
    backupDatabase();
    process.exit(1);
});

