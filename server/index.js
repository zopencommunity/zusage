const express = require('express');
const bodyParser = require('body-parser');
const sqlite3 = require('sqlite3').verbose();
const os = require('os');
const fs = require('fs');
const path = require('path');

const app = express();
const dbFilePath = path.join(__dirname, 'usage_data.db');
const backupFilePath = `${dbFilePath}.bak`;
const db = new sqlite3.Database(dbFilePath);

// Set view engine to EJS
app.set('view engine', 'ejs');
app.set('views', path.join(__dirname, 'views'));

// Middleware
app.use(bodyParser.json());
app.use(express.static('public'));

// Initialize database
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

// Backup function
function backupDatabase() {
    try {
        if (fs.existsSync(dbFilePath)) {
            fs.copyFileSync(dbFilePath, backupFilePath);
            console.log(`Database backup created at: ${backupFilePath}`);
        }
    } catch (error) {
        console.error('Error creating database backup:', error);
    }
}

// Validate data before inserting
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
    return requiredFields.every(field => typeof data[field] === 'string' && data[field].trim() !== '');
}

// Endpoint to receive and store usage data
app.post('/usage', (req, res) => {
    const data = req.body;

    if (!validateData(data)) {
        return res.status(400).json({ error: 'Invalid data format.' });
    }

    const query = `
        INSERT INTO usage (app_name, fqdn, local_ip, os_release, cpu_arch, app_version, timestamp)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    `;

    db.run(query, Object.values(data), function (err) {
        if (err) {
            console.error('Database error:', err);
            return res.status(500).json({ error: 'Failed to save data.' });
        }
        res.status(201).json({ success: true, id: this.lastID });
    });
});

// Endpoint to get raw data
app.get('/usage', (req, res) => {
    db.all('SELECT * FROM usage', [], (err, rows) => {
        if (err) {
            console.error('Database error:', err);
            return res.status(500).json({ error: 'Failed to retrieve data.' });
        }
        res.json(rows);
    });
});

// Endpoint to display monthly usage in a chart
app.get('/charts', (req, res) => {
    const query = `
        SELECT 
            strftime('%Y-%m', timestamp) AS month, 
            app_name, 
            fqdn,
            COUNT(*) AS total 
        FROM usage 
        GROUP BY month, app_name, fqdn 
        ORDER BY month DESC, total DESC
    `;

    db.all(query, [], (err, rows) => {
        if (err) {
            console.error('Database error:', err);
            return res.status(500).send('Database error.');
        }

        let consolidatedData = [];

        rows.forEach(row => {
            let monthEntry = consolidatedData.find(m => m.month === row.month);
            if (!monthEntry) {
                monthEntry = {
                    month: row.month,
                    total: 0,
                    tools: []
                };
                consolidatedData.push(monthEntry);
            }
            monthEntry.total += row.total;
            monthEntry.tools.push({
                name: row.app_name,
                hostname: row.fqdn,
                usage: row.total
            });
        });

        res.render('charts', { consolidatedData });
    });
});

// Function to get local IP
function getLocalIpAddress() {
    const interfaces = os.networkInterfaces();
    for (const name of Object.keys(interfaces)) {
        for (const iface of interfaces[name]) {
            if (iface.family === 'IPv4' && !iface.internal) {
                return iface.address;
            }
        }
    }
    return '127.0.0.1';
}

// Start server
const PORT = 3000;
app.listen(PORT, () => {
    console.log(`Server running at http://localhost:${PORT}`);
    console.log(`Access from network: http://${getLocalIpAddress()}:${PORT}`);
    backupDatabase();
});

// Graceful shutdown
process.on('exit', backupDatabase);
process.on('SIGINT', () => {
    console.log('\nShutting down...');
    backupDatabase();
    process.exit();
});
process.on('uncaughtException', (err) => {
    console.error('Uncaught exception:', err);
    backupDatabase();
    process.exit(1);
});

