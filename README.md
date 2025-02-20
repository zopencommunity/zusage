# Usage Analytics Dashboard

**Important: IBM Internal Use Only**

**This usage analytics solution is designed for internal use within the IBM domain and network environment only.**  It is not intended for external or production deployments outside of IBM's internal infrastructure.

## Description

This project provides a simple usage analytics solution to track application usage within an IBM domain environment. It consists of three main components:

1.  **C Client Library (`zusage.c`):** A lightweight C library that can be integrated into applications. It collects usage data (application name, hostname, IP address, OS release, CPU architecture, application version, and username) and sends it to a central Node.js server.
2.  **Node.js Server (`app.js`):**  A backend server built with Node.js, Express.js, and SQLite. It receives usage data from clients, stores it in an SQLite database, and provides API endpoints to retrieve aggregated data for dashboard visualizations.
3.  **Frontend Dashboard (`public` directory):** A web-based dashboard built with HTML, CSS, and JavaScript (Chart.js). It fetches data from the Node.js server's API endpoints and displays interactive charts and a raw data table to visualize application usage.

## Features

*   **Automated Usage Data Collection:**  C client library automatically collects usage data upon application initialization using a constructor function.
*   **Data Points:** Collects:
    *   Application Name
    *   Fully Qualified Domain Name (FQDN)
    *   Local IP Address
    *   Operating System Release
    *   CPU Architecture
    *   Application Version (reads from `/.version` file if available)
    *   Username
    *   Timestamp (server-side)
*   **Centralized Data Storage:** Usage data is stored in an SQLite database on the Node.js server.
*   **Web-based Dashboard:** Provides a user-friendly web interface to visualize usage patterns.
*   **Interactive Charts:**  Includes charts for:
    *   Usage Over Time
    *   Application Popularity
    *   OS Distribution
    *   CPU Architecture Distribution
    *   Hostname Usage
*   **Raw Data Table:** Displays the raw usage data for debugging and detailed analysis.
*   **Weekly Database Backups:**  Automatically performs weekly timestamped backups of the SQLite database and stores them in a `weekly_backups` directory.
*   **Regular Database Backups:** Creates regular backups on server start, shutdown, and uncaught exceptions.
*   **Debug Logging:**  Detailed debug logging can be enabled via an environment variable, writing logs to `/tmp/zusagedebug-*.log`.
*   **Disable Usage Collection:** **Usage data collection can be completely disabled by setting the environment variable `ZUSAGE_DISABLE`.**
*   **Schema Migration:** The Node.js server automatically handles schema migration to add new columns like `username` without requiring database deletion.

## Components

### 1. C Client Library (`zusage.c`)

*   **Language:** C
*   **Functionality:**
    *   Collects system information (hostname, IP, OS, CPU architecture, username).
    *   Retrieves application name and version.
    *   Constructs a JSON payload with usage data.
    *   Sends an HTTP POST request to the Node.js server.
    *   Includes debug logging and **a mechanism to disable usage collection via the `ZUSAGE_DISABLE` environment variable.**
*   **Integration:**  Intended to be compiled as a shared library or statically linked into C/C++ applications.

### 2. Node.js Server (`app.js`)

*   **Language:** JavaScript (Node.js)
*   **Frameworks/Libraries:** Express.js, SQLite3, Body-parser
*   **Functionality:**
    *   Receives usage data via HTTP POST requests at `/usage` endpoint.
    *   Validates incoming data.
    *   Stores data in an SQLite database (`usage_data.db`).
    *   Provides API endpoints for data retrieval and aggregation for charts:
        *   `/usage/raw` - Raw table data (for debugging).
        *   `/api/usage-over-time` - Usage count over time.
        *   `/api/app-popularity` - Application popularity ranking.
        *   `/api/os-distribution` - OS distribution.
        *   `/api/cpu-distribution` - CPU architecture distribution.
        *   `/api/hostname-usage` - Hostname usage count.
    *   Implements regular and weekly database backup mechanisms.
    *   Serves the frontend dashboard files from the `public` directory.

### 3. Frontend Dashboard (`public/`)

*   **Languages/Libraries:** HTML, CSS, JavaScript, Chart.js
*   **Functionality:**
    *   Provides a web interface to visualize usage analytics data.
    *   Fetches data from the Node.js server's API endpoints using JavaScript.
    *   Uses Chart.js library to render interactive charts (line, bar, pie).
    *   Displays a table of raw usage data.
    *   Styled with basic CSS.
