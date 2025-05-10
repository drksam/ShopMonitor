# ESP32 Node Firmware Installation Guide

This guide provides step-by-step instructions for installing and configuring the RFID Machine Monitor firmware on ESP32 devices.

## Prerequisites

### Hardware Requirements
- ESP32 development board (recommended: ESP32-WROOM-32)
- ESP32-CYD boards for touch-enabled nodes (Location and Machine nodes)
- MFRC522 RFID reader modules (1-4 depending on configuration)
- Connecting wires
- Micro USB cable
- Optional components:
  - Relay modules (for machine control)
  - LED indicators
  - Buzzer (for audio feedback)
  - Status LEDs
  - Emergency stop buttons (for area controllers)
  - Bluetooth audio devices (for CYD nodes)

### Software Requirements
- Visual Studio Code
- PlatformIO extension
- ESP32 Arduino core
- Network with DHCP
- For ESP32-CYD nodes:
  - LVGL library
  - TFT_eSPI library

## Wiring Diagram

### Machine Monitor Mode
```
ESP32               MFRC522 (1)     MFRC522 (2-4, optional)
------               --------        -----------------------
3.3V       ------->  VCC             VCC
GND        ------->  GND             GND
GPIO23 (MOSI) ---->  MOSI            MOSI
GPIO19 (MISO) ---->  MISO            MISO
GPIO18 (SCK) ----->  SCK             SCK
GPIO5  (SS1) ----->  SDA (1)
GPIO17 (SS2) ----->  SDA (2)
GPIO16 (SS3) ----->  SDA (3)
GPIO4  (SS4) ----->  SDA (4)
GPIO2        -----> LED1 (Machine 1 status)
GPIO15       -----> LED2 (Machine 2 status)
GPIO13       -----> LED3 (Machine 3 status)
GPIO12       -----> LED4 (Machine 4 status)
GPIO14       -----> BUZZER
```

### Accessory IO Mode
```
ESP32               Relays/Outputs   Inputs
------              --------------   ------
3.3V       ------->  VCC             VCC (via pull-up)
GND        ------->  GND             GND
GPIO26     ------->  Relay 1
GPIO25     ------->  Relay 2
GPIO33     ------->  Relay 3
GPIO32     ------->  Relay 4
GPIO35     <-------  Input 1
GPIO34     <-------  Input 2
GPIO39     <-------  Input 3
GPIO36     <-------  Input 4
GPIO2      ------->  Status LED
```

### ESP32-CYD Location Node Mode
```
ESP32-CYD           MFRC522           E-STOP
---------           -------           ------
3.3V       ------->  VCC
GND        ------->  GND              GND
GPIO23 (MOSI) ---->  MOSI
GPIO19 (MISO) ---->  MISO
GPIO18 (SCK) ----->  SCK
GPIO5  (SS) ------>  SDA
GPIO26     <----------------------->  E-STOP INPUT
GPIO27     ------------------------>  E-STOP OUTPUT
GPIO14     ------------------------>  BUZZER
GPIO32     <----------------------->  External Alert LED
GPIO33     <----------------------->  Machine Status LED
```

### ESP32-CYD Machine Node Mode
```
ESP32-CYD           MFRC522           Machine Control
---------           -------           --------------
3.3V       ------->  VCC
GND        ------->  GND              GND
GPIO23 (MOSI) ---->  MOSI
GPIO19 (MISO) ---->  MISO
GPIO18 (SCK) ----->  SCK
GPIO5  (SS) ------>  SDA
GPIO26     <----------------------->  Machine Status Input
GPIO27     ------------------------>  Machine Enable Output
GPIO14     ------------------------>  BUZZER
GPIO25     ------------------------>  E-STOP Relay
```

### Area Controller Mode
```
ESP32               E-STOP Master     Area Signals
------              ------------      ------------
3.3V       ------->  VCC
GND        ------->  GND              GND
GPIO26     <-------   E-STOP Status Input
GPIO25     -------->  E-STOP Activation Output
GPIO33     <-------   Area Alert Input
GPIO32     -------->  Area Alert Output
GPIO23     <------->  Location Node Communication
GPIO22     <------->  Building System Integration
GPIO21     -------->  Status LED Array
GPIO19     -------->  Audio Alert
```

## Installation Steps

### 1. Prepare the Development Environment
1. Install Visual Studio Code from https://code.visualstudio.com/
2. Open VS Code and install the PlatformIO extension from the marketplace
3. Restart VS Code after installation

### 2. Download the Firmware
1. Clone the repository or download the firmware directory
2. Open the firmware directory in VS Code with PlatformIO

### 3. Configure the Firmware
1. Open `include/constants.h` to review default settings
2. Modify configuration parameters as needed:
   - `WIFI_AP_NAME`: Default access point name
   - `WIFI_AP_PASSWORD`: Default access point password
   - `SERVER_ADDRESS`: Default address of the central server
   - `DEFAULT_NODE_TYPE`: Default node type (MACHINE_MONITOR, OFFICE_READER, ACCESSORY_IO, LOCATION_NODE, MACHINE_NODE, AREA_CONTROLLER)
   - `DEFAULT_AREA_ID`: Default area ID (for hierarchical system)
   - `DEFAULT_LOCATION_ID`: Default location ID (for hierarchical system)
   - `ESTOP_GROUP`: Emergency stop group identifier

### 4. Build and Upload the Firmware
1. Connect the ESP32 to your computer via USB
2. In PlatformIO, select the correct COM port for your device
3. Click the "Build" button (checkmark icon) to compile the firmware
4. Click the "Upload" button (right arrow icon) to flash the firmware to the ESP32

### 5. Initial Configuration
1. After flashing, the ESP32 will create a WiFi access point named "RFID-Monitor-XXXXXX"
2. Connect to this access point using the default password "setupdevice"
3. Open a web browser and navigate to `192.168.4.1`
4. The configuration portal will appear:
   - Set your WiFi network credentials
   - Configure the node type and name
   - Set the central server address
   - Configure RFID readers and I/O pins
   - Select Area and Location assignments
   - Configure E-STOP group membership
   - Set up Lead Operator permissions
   - Configure multi-user settings
   - Set up inspection system parameters
5. Click "Save" to apply the configuration
6. The ESP32 will restart and connect to your WiFi network

### 6. Verifying Installation
1. The ESP32 will register with the central server automatically
2. On the central server, go to "Nodes" and look for your new device
3. Verify the device appears in the correct mode
4. Test RFID card reading or IO functionality depending on your configuration

### 7. Area-Based Hierarchical Configuration
1. After nodes are connected to the server, log in to the administration panel
2. Navigate to "Areas" > "Area Management"
3. Create areas that represent building-level containers
4. For each area, create locations within them
5. Assign nodes to appropriate locations
6. Configure hierarchical permissions for users
7. Set up E-STOP propagation rules by area and location
8. Configure any area-specific settings

### 8. Multi-User & Lead Operator Setup
1. Navigate to "Users" > "Lead Operator Management"
2. Designate which users can be lead operators
3. Set up lead operator assignments for areas and locations
4. Configure lead operator transfer rules
5. Set up multi-user access permissions
6. Test lead operator workflows on machine and location nodes

### 9. ESP32-CYD Touch Configuration
1. For ESP32-CYD nodes, additional display configuration is required
2. In the node configuration portal, navigate to "Display Settings"
3. Calibrate the touch screen
4. Select UI layout and color scheme
5. Configure dashboard widgets
6. Set up user interface for lead operator management
7. Configure inspection interface parameters
8. Set audio alert preferences

## Updating Firmware

### Over-the-Air (OTA) Updates
1. On the central server, go to "Nodes" > "Device Management"
2. Find your device in the list
3. Click "Update Firmware"
4. Upload the new firmware binary file
5. Click "Deploy" to send the update to the device

### Area-Based Updates
1. On the central server, go to "Areas" > "Area Management"
2. Select the area containing nodes you want to update
3. Click "Node Management" > "Update Firmware"
4. Select nodes to update by location or type
5. Upload the new firmware binary file
6. Schedule the update (immediately or during maintenance window)
7. Click "Deploy" to send the update to the selected devices

### Manual Updates
1. Connect the ESP32 to your computer via USB
2. Open the firmware directory in VS Code with PlatformIO
3. Make your modifications to the code
4. Click the "Upload" button to flash the updated firmware

## Troubleshooting

### Device Not Creating Access Point
- Verify power supply is adequate (min 500mA)
- Try pressing the reset button
- Reflash the firmware using USB

### Cannot Connect to WiFi
- Verify WiFi credentials are correct
- Ensure ESP32 is within range of the WiFi router
- Check if the WiFi network has MAC filtering enabled

### RFID Reader Not Detecting Cards
- Verify wiring connections to the MFRC522 module
- Check that the correct SPI pins are configured
- Try a different RFID card to rule out card issues

### Device Not Appearing in Central Server
- Verify network connectivity
- Check that the server address is configured correctly
- Ensure the server is running and accessible
- Check for firewall rules blocking mDNS or HTTP traffic

### LED Indicators
- **Solid Green**: Connected to WiFi and server
- **Blinking Green**: Connected to WiFi, server connection issues
- **Solid Blue**: Access point mode active (configuration mode)
- **Blinking Red**: Hardware/initialization error
- **Rapid Blue Flash**: RFID card detected
- **Rapid Green Flash**: Successful authorization
- **Rapid Red Flash**: Access denied

### Area-Based System Issues
- **Areas Not Appearing**: Verify database migrations have run successfully
- **Location Assignment Fails**: Check that the area exists and is active
- **Hierarchical Permissions Not Working**: Verify role assignments and permission cascade
- **E-STOP Propagation Issues**: Check E-STOP group configurations and network connectivity

### Lead Operator System Issues
- **Lead Transfer Fails**: Verify the new user has lead operator permissions
- **Multi-User Login Issues**: Check machine's max user count setting
- **Lead Required Error**: Ensure a lead operator is assigned before other users try to access
- **Lead Status Not Updating**: Check websocket connectivity for real-time updates

### ESP32-CYD Display Issues
- **Screen Not Responding**: Recalibrate touch interface
- **UI Elements Misaligned**: Check screen resolution configuration
- **Slow UI Response**: Verify memory usage and reduce UI complexity
- **Display Flickering**: Update TFT_eSPI configuration for proper refresh rate

## Advanced Configuration

For advanced customization, edit these files before building:

- `config.cpp`: Configuration storage and management
- `RFIDHandler.cpp`: RFID reading and processing
- `WebUI.cpp`: Web interface components
- `NetworkManager.cpp`: WiFi and network communication
- `AreaController.cpp`: Area-based management functions
- `LeadOperator.cpp`: Lead operator and multi-user functionality
- `DisplayUI.cpp`: ESP32-CYD UI implementation
- `InspectionSystem.cpp`: Quality inspection framework

## Hierarchical System Architecture

The area-based hierarchical system organizes the machinery in three tiers:

1. **Area**: Building or facility level (e.g., "Manufacturing Building")
2. **Location**: Room or zone level (e.g., "Assembly Line 1")
3. **Machine**: Individual equipment (e.g., "Welding Station 2")

This structure enables:
- Granular permission management
- Localized alert propagation
- Targeted E-STOP activation
- Efficient resource allocation
- Simplified reporting and analytics

## Multi-User & Lead Operator System

The multi-user system allows multiple operators to work on a single machine simultaneously, with one designated as the lead operator:

- **Lead Operator**: Has primary responsibility and authority over the machine/location/area
- **Standard Operators**: Can use the machine under lead supervision
- **Trainees**: Limited access requiring lead approval for operations

Lead operators can:
- Approve/deny machine access requests
- Transfer lead status to another qualified operator
- Override certain machine restrictions
- Approve quality inspections
- Respond to E-STOP events
- Generate performance reports

## Support

For additional support, please contact the development team or refer to the online documentation.

# Installation Guide
## Shop Monitor v1.0.1 (Part of ShopSuite v1.0.1)

## System Requirements

- Python 3.8 or higher
- SQLite (development) or PostgreSQL (production)
- 2GB RAM minimum (4GB recommended)
- 1GB disk space (excluding database growth)

## Quick Setup (Development)

1. Clone the repository:
```bash
git clone https://github.com/your-username/ShopMonitor.git
cd Shop
```

2. Create a virtual environment:
```bash
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
```

3. Install dependencies:
```bash
pip install -r requirements.txt
```

4. Initialize the database:
```bash
flask db upgrade
```

5. Run the development server:
```bash
flask run --host=0.0.0.0
```

The application will be available at http://localhost:5000

## Production Deployment

### Using Docker (Recommended)

1. Build the Docker image:
```bash
docker build -t shop-monitor .
```

2. Run the container:
```bash
docker run -d --name shop-monitor-app -p 8000:5000 \
  -e "DATABASE_URL=postgresql://username:password@db:5432/shop_monitor" \
  -e "SECRET_KEY=your-secret-key" \
  -e "DEBUG=False" \
  -e "CSRF_PROTECTION=True" \
  -e "SECURITY_HEADERS=True" \
  shop-monitor
```

### Manual Setup

1. Set up a PostgreSQL database server
2. Configure environment variables (see Configuration section)
3. Set up a reverse proxy (Nginx, Apache, etc.)
4. Configure SSL certificates
5. Set up uWSGI or Gunicorn as the application server

Example uWSGI configuration:
```ini
[uwsgi]
module = main:app
master = true
processes = 4
socket = shop-monitor.sock
chmod-socket = 660
vacuum = true
die-on-term = true
```

## Configuration

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `DATABASE_URL` | Database connection URL | `sqlite:///instance/shop_tracker.db` |
| `SECRET_KEY` | Flask application secret key | Random (development only) |
| `DEBUG` | Enable debug mode | `True` (development), `False` (production) |
| `LOG_LEVEL` | Application log level | `INFO` |
| `CSRF_PROTECTION` | Enable CSRF protection | `True` |
| `SECURITY_HEADERS` | Enable security headers | `True` |
| `TOKEN_EXPIRATION_SECONDS` | API token expiration | `86400` (24 hours) |
| `BRUTE_FORCE_MAX_ATTEMPTS` | Failed attempts before lockout | `5` |
| `BRUTE_FORCE_LOCKOUT_SECONDS` | Lockout period after failed attempts | `900` (15 minutes) |

### Configuration Files

The application can be configured through environment variables or configuration files:

1. `config.py` - Main configuration file
2. `instance/config.py` - Instance-specific overrides (not in version control)
3. Environment variables - Highest precedence

## Security Configuration

### API Security Setup

The application includes enhanced API security with the following features:

1. **API Token Authentication**
   - Bearer token authentication with persistent storage
   - Token encryption for secure storage across server restarts
   - Expiration and automatic refreshing
   - Scope-based access control with granular permissions
   - Last-used timestamp tracking for audit purposes

2. **Security Headers**
   - Content Security Policy (CSP) with strict directives
   - X-Content-Type-Options: nosniff
   - X-Frame-Options: SAMEORIGIN
   - X-XSS-Protection: 1; mode=block
   - Referrer-Policy: strict-origin-when-cross-origin
   - Comprehensive protection against XSS, clickjacking, and MIME confusion attacks

3. **Brute Force Protection**
   - IP-based rate limiting with exponential backoff strategy
   - Intelligent lockout system with progressive timing
   - Configurable failed attempt thresholds (5/10/20)
   - Lockout durations that increase with repeated failures (5min/30min/24hr)
   - Comprehensive audit logging of blocked access attempts

To configure these settings, update your environment variables or instance/config.py:

```python
# Security settings
SECURITY_HEADERS = True  # Enable security headers
CSRF_PROTECTION = True   # Enable CSRF protection
TOKEN_EXPIRATION_SECONDS = 86400  # API token validity period
TOKEN_PERSISTENCE = True  # Enable persistent token storage
TOKEN_ENCRYPTION_KEY = os.environ.get("TOKEN_ENCRYPTION_KEY", generate_key())
BRUTE_FORCE_MAX_ATTEMPTS = 5      # Maximum failed attempts
BRUTE_FORCE_LOCKOUT_SECONDS = 300 # Initial lockout period in seconds
BRUTE_FORCE_MULTIPLIER = 6        # Lockout time multiplier for repeated failures
SECURITY_AUDIT_LOGGING = True     # Enable detailed security event logging
```

### Enhanced Input Sanitization

Configure the enhanced input validation and sanitization system:

```python
# Input validation settings
MAX_CONTENT_LENGTH = 16 * 1024 * 1024  # 16MB max upload size
ALLOWED_EXTENSIONS = {'txt', 'pdf', 'png', 'jpg', 'jpeg', 'gif'}
XSS_PROTECTION_LEVEL = "aggressive"  # Options: "standard", "aggressive", "paranoid"
RECURSIVE_SANITIZATION = True  # Enable deep sanitization of nested structures
INPUT_VALIDATION_STRICTNESS = "high"  # Options: "low", "medium", "high"
```

## Error Handling & Logging

The application includes a comprehensive error handling and logging system with:

1. **Structured Logging**
   - Non-volatile storage with daily rotation
   - Microsecond timestamp precision
   - Different log levels (DEBUG, INFO, WARNING, ERROR, CRITICAL)
   - JSON log format with rich contextual information
   - Severity classification for better filtering

2. **Error Management**
   - Centralized error handling with @handle_api_errors decorator
   - Structured application-specific error codes (1xxx-5xxx)
   - Consistent JSON error responses with traceability
   - Error reference IDs for support and debugging
   - Comprehensive error details with sanitized request data

3. **Resilience Patterns**
   - Exponential backoff for network operations
   - Circuit breaker pattern for external service calls
   - Graceful degradation of non-critical features
   - Automatic recovery mechanisms
   - Request retry with configurable parameters

Configure the enhanced error handling and logging system:

```python
# Logging configuration
LOG_LEVEL = "INFO"  # DEBUG, INFO, WARNING, ERROR, CRITICAL
LOG_FORMAT = "json"  # "text" or "json"
LOG_FILE = "logs/application.log"
LOG_ROTATION = True
LOG_MAX_SIZE = 10 * 1024 * 1024  # 10MB
LOG_BACKUP_COUNT = 30  # Keep 30 days of logs
LOG_MICROSECOND_PRECISION = True  # Enable microsecond timestamp precision
LOG_REQUEST_CONTEXT = True  # Include request context in logs

# Error handling configuration
ERROR_INCLUDE_TRACEBACK = False  # Don't include tracebacks in responses
ERROR_NOTIFICATION_EMAIL = "admin@example.com"
EXPONENTIAL_BACKOFF_INITIAL = 1.0  # Initial retry delay in seconds
EXPONENTIAL_BACKOFF_MAX = 60.0  # Maximum retry delay in seconds
EXPONENTIAL_BACKOFF_FACTOR = 2.0  # Multiplier for each retry attempt
MAX_RETRY_ATTEMPTS = 5  # Maximum number of retry attempts
CIRCUIT_BREAKER_THRESHOLD = 5  # Failures before circuit opens
CIRCUIT_BREAKER_TIMEOUT = 60  # Seconds before circuit half-opens
```

## Database Setup

### Initialize the Database

```bash
# Apply migrations to create tables
flask db upgrade

# [Optional] Populate with sample data
flask seed-db
```

### Database Migrations

To create new migrations after model changes:

```bash
flask db migrate -m "Description of changes"
flask db upgrade
```

## Integration Configuration

### ShopTracker Integration

To connect to ShopTracker, configure the following:

```python
# ShopTracker integration
SHOP_API_BASE_URL = "https://tracker.example.com/api"
SHOP_API_KEY = "your-api-key"
SYNC_INTERVAL = 3600  # Sync every hour
```

### Node Device Configuration

For ESP32 nodes to connect to the server, they need:

1. The server URL
2. An API key with appropriate permissions

Generate an API key for nodes:

```bash
flask generate-node-api-key --name "Shop Floor Node" --type "machine_monitor"
```

## Setting up RFID Readers

1. Configure RFID card format:
```python
RFID_FORMAT = "wiegand26"  # Options: wiegand26, wiegand34, mifare
```

2. Connect hardware readers:
   - See [hardware documentation](arduino/README.md) for details

## First-time Setup

After installation, follow these steps:

1. Open http://localhost:5000 (development) or your production URL
2. Login with default credentials:
   - Username: `admin`
   - Password: `admin123` (change immediately)
3. Update admin password
4. Configure general settings
5. Add users
6. Create areas and zones
7. Add machines
8. Set up nodes

## Upgrading

To upgrade the application:

1. Back up your database:
```bash
pg_dump -U username shop_monitor > backup.sql
```

2. Pull the latest changes:
```bash
git pull origin main
```

3. Install new dependencies:
```bash
pip install -r requirements.txt
```

4. Apply database migrations:
```bash
flask db upgrade
```

5. Restart the application server

## Backup and Recovery

### Database Backup

Regular backups are crucial. Set up a cron job to run:

```bash
pg_dump -U username shop_monitor > /path/to/backups/shop_monitor_$(date +%Y%m%d).sql
```

### Application Backup

Regularly backup:
1. Database
2. Configuration files
3. Uploaded files
4. Custom templates

## Troubleshooting

### Common Issues

1. **Database connection errors**
   - Check DATABASE_URL is correct
   - Verify database server is running
   - Test connectivity with `psql` or `sqlitebrowser`

2. **Permission errors**
   - Check file permissions for the application directory
   - Verify database user permissions

3. **Server errors**
   - Check logs at `logs/application.log`
   - Verify environment variables
   - Check disk space

### Debug Mode

To enable detailed debugging information:

```bash
export DEBUG=True
export LOG_LEVEL=DEBUG
flask run
```

### Security Diagnostics

To run a security diagnostic check:

```bash
flask security-check
```

This will verify:
- Proper security headers configuration
- CSRF protection status
- API token security settings
- Rate limiting configuration
- Database connection security
- Password policies

## Support

For help with installation or configuration:
1. Check the [documentation](https://github.com/your-username/ShopMonitor/docs)
2. Open an issue on GitHub
3. Contact support at support@example.com