require('dotenv').config(); // Load environment variables from .env file
const express = require('express');
const bodyParser = require('body-parser');
const sqlite3 = require('sqlite3').verbose();
const os = require('os');
const fs = require('fs');
const path = require('path');
const passport = require('passport'); // Import passport
const OAuth2Strategy = require('passport-oauth2'); // Import OAuth2 strategy
const session = require('express-session'); // Import express-session
const https = require('https');
const http = require('http');

// Initialize the app and database
const app = express();
const httpApp = express(); // Separate Express app for HTTP /usage route
const dbFilePath = path.join(__dirname, 'usage_data.db');
const backupFilePath = `${dbFilePath}.bak`;
const weeklyBackupDir = path.join(__dirname, 'weekly_backups');

// Ensure weekly backup directory exists
if (!fs.existsSync(weeklyBackupDir)) {
    fs.mkdirSync(weeklyBackupDir, { recursive: true });
}

const db = new sqlite3.Database(dbFilePath);

// --- HTTPS Configuration using self-signed certificate ---
const privateKey = fs.readFileSync(path.join(__dirname, 'zusage1fyreibmcom.key'), 'utf8'); 
const certificate = fs.readFileSync(path.join(__dirname, 'zusage1fyreibmcom.pem'), 'utf8'); 
const credentials = {
    key: privateKey,
    cert: certificate
};

// Middleware
app.use(bodyParser.json());
app.use(express.static(path.join(__dirname, 'public'))); // Serve static files from 'public' directory

app.use(session({
    secret: process.env.SESSION_SECRET, 
    resave: false,
    saveUninitialized: false,
}));

app.set('view engine', 'ejs'); // Set EJS as the view engine
app.set('views', path.join(__dirname, 'public')); // Set views directory to 'public' (where index.html is)

// --- Passport Setup ---
app.use(passport.initialize());
app.use(passport.session());

app.use(bodyParser.json());
httpApp.use(bodyParser.json()); // Need body-parser for the httpApp too

app.use(express.static(path.join(__dirname, 'public')));
httpApp.use(express.static(path.join(__dirname, 'public'))); // Serve static files for httpApp as well (though mainly for /usage)


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

// --- OAuth 2.0 Strategy Configuration ---
passport.use('github-ibm-oauth2', new OAuth2Strategy({ // Named strategy 'github-ibm-oauth2'
        authorizationURL: process.env.OAUTH_AUTHORIZATION_URL, 
        tokenURL: process.env.OAUTH_TOKEN_URL,            
        clientID: process.env.OAUTH_CLIENT_ID,    
        clientSecret: process.env.OAUTH_CLIENT_SECRET,
        callbackURL: process.env.OAUTH_CALLBACK_URL,   
        scope: 'user:email', 
    },
    function(accessToken, refreshToken, profile, done) {
        return done(null, profile);
    }
));

// --- Passport Serialization and Deserialization ---
passport.serializeUser(function(user, done) {
    done(null, user); // For simplicity, serialize the entire user profile into the session
});

passport.deserializeUser(function(user, done) {
    done(null, user); // Deserialize by just passing the user object back
});


// --- Authentication Routes ---
app.get('/auth/github-ibm',
    passport.authenticate('github-ibm-oauth2')); // Trigger OAuth flow

app.get('/auth/github-ibm/callback',
    passport.authenticate('github-ibm-oauth2', { failureRedirect: '/login' }), // Handle callback
    function(req, res) {
        // Successful authentication, redirect to dashboard.
        res.redirect('/');
    });

app.get('/logout', function(req, res){
    req.logout(function(err) {
        if (err) { return next(err); }
        res.redirect('/login'); 
    });
});

app.get('/login', (req, res) => { 
    res.sendFile(path.join(__dirname, 'public', 'login.html'));
});


// --- Middleware to ensure user is authenticated ---
function ensureAuthenticated(req, res, next) {
    if (req.isAuthenticated()) { return next(); }
    res.redirect('/login'); 
}

// --- Protected Routes (apply ensureAuthenticated middleware) ---
app.get('/',
    (req, res, next) => { 
        if (!req.secure) { // Check if the request is NOT secure (HTTP)
            const httpsUrl = 'https://' + req.headers.host + req.url; // Construct HTTPS URL
            return res.redirect(httpsUrl); // Redirect to HTTPS
        }
        next(); // If already HTTPS, proceed to the next middleware/handler
    },
    ensureAuthenticated, // <--- Existing authentication middleware
    (req, res) => {      // <--- Your original route handler
        console.log('Logged in user object:', req.user);

        res.render('index', { user: req.user });
    }
);

httpApp.get('/', (req, res) => { 
    const httpsUrl = `https://${req.headers.host.split(':')[0]}:3443/`; // Construct HTTPS URL (assuming port 443 or default HTTPS)
    res.redirect(httpsUrl); // Redirect to HTTPS version of UI
});

app.use((req, res, next) => {
    if (req.url === '/usage') { // Skip HTTPS redirect for /usage route
        return next();
    }
    if (req.secure) {
        next(); // Already HTTPS
    } else {
        const httpsUrl = 'https://' + req.headers.host + req.url;
        res.redirect(httpsUrl); // Redirect to HTTPS for all other routes
    }
});


app.get('/usage/custom-query', ensureAuthenticated, (req, res) => {
    const sql = req.query.sql;

    if (!sql) {
        return res.status(400).json({ error: 'SQL query parameter is missing.' });
    }

    db.all(sql, [], (err, rows) => {
        if (err) {
            console.error('Database error executing custom query:', err);
            return res.status(500).json({ error: 'Failed to execute custom query.', details: err.message });
        }
        res.json(rows);
    });
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
httpApp.post('/usage', (req, res) => {
    const data = req.body;

    if (!validateData(data)) {
        return res.status(400).json({ error: 'Invalid data format.' });
    }

	    const now = new Date();
    const timestamp = now.toISOString();


    // Insert the data into the database, including server-generated timestamp and username (if provided)
    const query = `
        INSERT INTO usage (app_name, fqdn, local_ip, os_release, cpu_arch, app_version, timestamp, username)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    `;

	db.run(
    query,
    [
        data.app_name,
        data.fqdn.toLowerCase(),
        data.local_ip,
        data.os_release,
        data.cpu_arch,
        data.app_version,
        timestamp,
        data.username || 'unknown'
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
    "26.00": "v2r3",
    "27.00": "v2r4",
    "28.00": "v2r5",
    "29.00": "v3r1",
    "30.00": "v3r2"
}; 

// CPU Architecture Mapping
const cpuArchMap = {
    // IBM zSeries / IBM Z mappings
    "3932":    "IBM z16",        // z16
    "3931":    "IBM z16",        // z16
    "8561":    "IBM z15",        // z15
    "3907":    "IBM z14",        // z14
    "3906":    "IBM z14",        // z14
    "2964":    "IBM z13",        // z13
    "2965":    "IBM z13s",       // z13s
    "2827":    "IBM zEC12",      // zEnterprise EC12
    "2828":    "IBM zBC12",      // zEnterprise BC12
    "2817":    "IBM z196",       // zEnterprise 196
    "2818":    "IBM z114",       // zEnterprise 114
    "2097":    "IBM z10 EC",     // z10 Enterprise Class
    "2098":    "IBM z10 BC",     // z10 Business Class
    "2094":    "IBM z9 EC",      // z9 Enterprise Class
    "2096":    "IBM z9 BC",      // z9 Business Class
};

// Endpoint for Usage Over Time chart
app.get('/api/usage-over-time', ensureAuthenticated, (req, res) => {
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
app.get('/api/app-popularity', ensureAuthenticated, (req, res) => {
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
app.get('/api/os-distribution', ensureAuthenticated, (req, res) => {
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
app.get('/api/cpu-distribution', ensureAuthenticated, (req, res) => {
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
        // Apply CPU Architecture mapping to labels
        const mappedRows = rows.map(row => ({
            cpu_arch: cpuArchMap[row.cpu_arch] || row.cpu_arch, // Use map or original if not found
            usage_count: row.usage_count
        }));
        res.json(mappedRows); // Send the mapped data
    });
});

// --- NEW API Endpoint for Hostname Usage Chart ---
app.get('/api/hostname-usage', ensureAuthenticated, (req, res) => {
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

// Endpoint to view raw data for a specific day
app.get('/usage/daily-raw/:date', ensureAuthenticated, (req, res) => {
    const selectedDate = req.params.date; // Date from URL parameter (YYYY-MM-DD)

    if (!selectedDate) {
        return res.status(400).json({ error: 'Date parameter is required.' });
    }

    const query = `
        SELECT * FROM usage
        WHERE DATE(timestamp) = ?
        ORDER BY timestamp DESC
    `;

    db.all(query, [selectedDate], (err, rows) => {
        if (err) {
            console.error('Database error:', err);
            return res.status(500).json({ error: 'Failed to retrieve daily data.' });
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

app.get('/download-db', ensureAuthenticated, (req, res) => {
    const dbFilePath = path.join(__dirname, 'usage_data.db');

    // Check if the database file exists
    if (!fs.existsSync(dbFilePath)) {
        return res.status(404).send('Database file not found.');
    }

    // Set headers to force download
    res.setHeader('Content-Disposition', 'attachment; filename=usage_data.db');
    res.setHeader('Content-Type', 'application/octet-stream'); // Or 'application/x-sqlite3'

    // Stream the database file to the response
    const fileStream = fs.createReadStream(dbFilePath);
    fileStream.pipe(res);

    fileStream.on('error', (err) => {
        console.error('Error streaming database file for download:', err);
        res.status(500).send('Error serving database file.');
    });
});

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

const httpsServer = https.createServer(credentials, app); // Use 'app' (HTTPS-secured Express app)
const HTTPS_PORT = 3443; // Standard HTTPS port
httpsServer.listen(HTTPS_PORT, () => {
    const ipAddress = getLocalIpAddress();
    console.log(`HTTPS Server is running! UI and secured APIs accessed via:`);
    console.log(`- Local:   https://localhost:${HTTPS_PORT}`);
    console.log(`- Network: https://${ipAddress}:${HTTPS_PORT}`);
});


// --- HTTP Server Setup (ONLY for /usage route) ---
const httpServer = http.createServer(httpApp); // Use 'httpApp' (HTTP-only Express app)
const HTTP_PORT = 3000; // Choose a different port for HTTP, e.g., 3000
httpServer.listen(HTTP_PORT, () => {
    const ipAddress = getLocalIpAddress();
    console.log(`HTTP Server is running! ONLY /usage route accessible via HTTP at:`);
    console.log(`- Local:   http://localhost:${HTTP_PORT}/usage`); // Note: http and port 3000, /usage path
    console.log(`- Network: http://${ipAddress}:${HTTP_PORT}/usage`); // Note: http and port 3000, /usage path
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
