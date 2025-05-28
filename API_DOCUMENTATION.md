# API Documentation
## Shop Monitor v1.0.1 (Part of ShopSuite v1.0.1)

## Overview

The shop Monitor API provides programmatic access to machine monitoring, user management, and shop floor operations. This API allows third-party applications, hardware nodes, and custom integrations to interact with the system.

## Base URL

```
Production: https://Shop.example.com/api/v1
Development: http://localhost:5000/api/v1
```

## Authentication

### API Token Authentication

All API requests require authentication using Bearer token authentication:

```
Authorization: Bearer YOUR_API_TOKEN
```

### Token Types

The system supports multiple token types:

| Token Type | Purpose | Expiration | Scope Support |
|------------|---------|------------|--------------|
| User Token | User-based access | 24 hours | Yes |
| Node Token | Hardware node authentication | 30 days | Limited |
| Integration Token | Third-party integration | 1 year | Yes |
| Refresh Token | Token refreshing | 30 days | No |

### Token Scopes

API tokens use scopes to control permission granularity:

| Scope | Description |
|-------|-------------|
| `read:machines` | Read machine data |
| `write:machines` | Create/update machine data |
| `read:users` | Read user data |
| `write:users` | Create/update user data |
| `read:sessions` | Read machine session data |
| `write:sessions` | Create/update session data |
| `read:areas` | Read area and zone data |
| `write:areas` | Create/update area data |
| `node:report` | Submit node readings |
| `admin:full` | Full administrator access |

### Getting a Token

```
POST /auth/token
```

Request body:
```json
{
  "username": "user@example.com",
  "password": "your-password",
  "scope": "read:machines write:sessions"
}
```

Response:
```json
{
  "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "expires_in": 86400,
  "token_type": "Bearer",
  "scope": "read:machines write:sessions"
}
```

### Refreshing a Token

```
POST /auth/refresh
```

Request body:
```json
{
  "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

Response:
```json
{
  "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "expires_in": 86400,
  "token_type": "Bearer",
  "scope": "read:machines write:sessions"
}
```

### Revoking a Token

```
POST /auth/revoke
```

Request body:
```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

Response:
```json
{
  "success": true,
  "message": "Token has been revoked"
}
```

## Security Features

### Rate Limiting

API endpoints are subject to rate limiting to protect against abuse:

- Standard limit: 100 requests per minute
- Node endpoints: 300 requests per minute
- Authentication endpoints: 10 requests per minute

Rate limit information is included in response headers:

```
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 95
X-RateLimit-Reset: 1589547Nielsen
```

### Brute Force Protection

Failed authentication attempts are monitored and will trigger temporary IP blocks:

- 5 failed attempts: 5-minute lockout
- 10 failed attempts: 30-minute lockout
- 20+ failed attempts: 24-hour lockout

IP-based lockouts are implemented with an exponential backoff strategy to defend against persistent attackers. All blocked authentication attempts are logged to the security event system for audit purposes.

### API Token Persistence

API tokens are now persisted across server restarts, ensuring that device authentication remains valid even after system maintenance or unexpected shutdowns. Tokens are stored with the following security measures:

- Sensitive token data is encrypted
- Tokens maintain full expiration control
- Last used timestamp is tracked for audit purposes

### Enhanced Input Sanitization

All API inputs are sanitized with multiple layers of protection:

- XSS protection with pattern-based filtering
- Recursive sanitization for nested JSON structures
- Contextual encoding based on destination use
- Validation against injection patterns

### Security Headers

All API responses include enhanced security headers:

```
Content-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net
X-Content-Type-Options: nosniff
X-Frame-Options: SAMEORIGIN
X-XSS-Protection: 1; mode=block
Referrer-Policy: strict-origin-when-cross-origin
```

These headers protect against common web vulnerabilities including XSS, clickjacking, MIME-type confusion, and information leakage.

### HTTPS Enforcement

All production API requests must use HTTPS. Non-secure requests will be redirected to HTTPS with a 301 status.

## Error Handling

### Error Response Format

All API errors return a consistent JSON format with improved error codes and traceability:

```json
{
  "error": {
    "code": "invalid_token",
    "message": "The access token has expired",
    "status": 401,
    "details": {
      "token_expiry": "2023-05-08T14:30:00Z"
    },
    "reference": "ERR-12345"
  }
}
```

### Standard Error Codes

| HTTP Status | Error Code | Description |
|-------------|------------|-------------|
| 400 | `invalid_request` | Bad request format |
| 401 | `unauthorized` | Authentication required |
| 401 | `invalid_token` | Token validation failed |
| 403 | `forbidden` | Insufficient permissions |
| 404 | `not_found` | Resource not found |
| 409 | `conflict` | Resource conflict |
| 422 | `validation_error` | Input validation failed |
| 429 | `rate_limited` | Too many requests |
| 500 | `server_error` | Internal server error |
| 503 | `service_unavailable` | Service temporarily unavailable |

### Enhanced Application Error Codes

In addition to standard HTTP error codes, the system now includes a comprehensive set of application-specific error codes for more precise error handling:

| Error Code | Category | Description |
|------------|----------|-------------|
| 1xxx | Authentication | Authentication-related errors |
| 2xxx | Authorization | Permission and access control errors |
| 3xxx | Validation | Input validation errors |
| 4xxx | Resource | Resource-related errors (not found, conflict) |
| 5xxx | System | System and service errors |

### Exponential Backoff Support

The API now provides explicit guidance for implementing exponential backoff through response headers:

```
X-Retry-After: 30
X-Backoff-Strategy: exponential
```

For network or service errors, clients should implement an exponential backoff strategy:
- Initial retry delay: 1 second
- Maximum retry delay: 60 seconds
- Maximum attempts: 5

### Validation Errors

Validation errors include detailed information about each invalid field:

```json
{
  "error": {
    "code": "validation_error",
    "message": "Invalid input data",
    "status": 422,
    "details": {
      "fields": {
        "name": "Name is required",
        "ip_address": "Invalid IP address format"
      }
    }
  }
}
```

### Error Tracing and Logging

All errors are now comprehensively logged with enhanced context:
- Unique reference ID for customer support
- Full error context including request data (sanitized)
- Stack traces in development mode
- Severity level classification
- Timestamp with microsecond precision
- Non-volatile storage with daily rotation

### Error Tracing

For support and debugging, all errors include a unique reference ID that can be provided to support staff.

## Endpoints

### Machines

#### List Machines

```
GET /machines
```

Query parameters:
- `zone_id` (optional): Filter by zone
- `active` (optional): Filter by active status (true/false)
- `limit` (optional, default=20): Number of results to return
- `offset` (optional, default=0): Pagination offset

Response:
```json
{
  "machines": [
    {
      "id": 1,
      "name": "Mill #1",
      "machine_id": "MILL001",
      "description": "5-axis CNC mill",
      "zone_id": 2,
      "active": true,
      "created_at": "2023-01-15T10:30:00Z",
      "updated_at": "2023-04-28T14:20:00Z",
      "requires_training": true,
      "access_level": "operator",
      "max_users": 2
    },
    ...
  ],
  "total": 25,
  "limit": 20,
  "offset": 0
}
```

#### Get Machine

```
GET /machines/{id}
```

Response:
```json
{
  "id": 1,
  "name": "Mill #1",
  "machine_id": "MILL001",
  "description": "5-axis CNC mill",
  "zone_id": 2,
  "zone_name": "Milling Area",
  "area_id": 1,
  "area_name": "Production Floor",
  "active": true,
  "created_at": "2023-01-15T10:30:00Z",
  "updated_at": "2023-04-28T14:20:00Z",
  "requires_training": true,
  "access_level": "operator",
  "max_users": 2,
  "current_sessions": [
    {
      "id": 123,
      "user_id": 45,
      "user_name": "John Doe",
      "start_time": "2023-05-08T09:15:00Z",
      "is_lead": true
    }
  ]
}
```

#### Create Machine

```
POST /machines
```

Request body:
```json
{
  "name": "Lathe #2",
  "machine_id": "LATHE002",
  "description": "CNC lathe with live tooling",
  "zone_id": 3,
  "active": true,
  "requires_training": true,
  "access_level": "operator",
  "max_users": 1
}
```

Response:
```json
{
  "id": 26,
  "name": "Lathe #2",
  "machine_id": "LATHE002",
  "description": "CNC lathe with live tooling",
  "zone_id": 3,
  "active": true,
  "created_at": "2023-05-08T15:30:00Z",
  "updated_at": "2023-05-08T15:30:00Z",
  "requires_training": true,
  "access_level": "operator",
  "max_users": 1
}
```

#### Update Machine

```
PUT /machines/{id}
```

Request body:
```json
{
  "name": "Lathe #2A",
  "description": "CNC lathe with updated tooling",
  "zone_id": 3,
  "active": true,
  "requires_training": true,
  "access_level": "operator",
  "max_users": 2
}
```

Response:
```json
{
  "id": 26,
  "name": "Lathe #2A",
  "machine_id": "LATHE002",
  "description": "CNC lathe with updated tooling",
  "zone_id": 3,
  "active": true,
  "created_at": "2023-05-08T15:30:00Z",
  "updated_at": "2023-05-08T16:45:00Z",
  "requires_training": true,
  "access_level": "operator",
  "max_users": 2
}
```

#### Delete Machine

```
DELETE /machines/{id}
```

Response:
```json
{
  "success": true,
  "message": "Machine deleted successfully"
}
```

### Sessions

#### List Sessions

```
GET /sessions
```

Query parameters:
- `machine_id` (optional): Filter by machine
- `user_id` (optional): Filter by user
- `active` (optional): Filter active sessions (true/false)
- `start_date` (optional): Filter by start date (YYYY-MM-DD)
- `end_date` (optional): Filter by end date (YYYY-MM-DD)
- `limit` (optional, default=20): Number of results to return
- `offset` (optional, default=0): Pagination offset

Response:
```json
{
  "sessions": [
    {
      "id": 123,
      "user_id": 45,
      "user_name": "John Doe",
      "machine_id": 1,
      "machine_name": "Mill #1",
      "start_time": "2023-05-08T09:15:00Z",
      "end_time": null,
      "active": true,
      "is_lead": true,
      "created_at": "2023-05-08T09:15:00Z",
      "updated_at": "2023-05-08T09:15:00Z"
    },
    ...
  ],
  "total": 158,
  "limit": 20,
  "offset": 0
}
```

#### Get Session

```
GET /sessions/{id}
```

Response:
```json
{
  "id": 123,
  "user_id": 45,
  "user_name": "John Doe",
  "machine_id": 1,
  "machine_name": "Mill #1",
  "start_time": "2023-05-08T09:15:00Z",
  "end_time": null,
  "active": true,
  "is_lead": true,
  "created_at": "2023-05-08T09:15:00Z",
  "updated_at": "2023-05-08T09:15:00Z",
  "notes": "Setup for job #12345",
  "zone_id": 2,
  "zone_name": "Milling Area",
  "area_id": 1,
  "area_name": "Production Floor"
}
```

#### Create Session

```
POST /sessions
```

Request body:
```json
{
  "user_id": 45,
  "machine_id": 1,
  "is_lead": true,
  "notes": "Setup for job #12345"
}
```

Response:
```json
{
  "id": 123,
  "user_id": 45,
  "user_name": "John Doe",
  "machine_id": 1,
  "machine_name": "Mill #1",
  "start_time": "2023-05-08T09:15:00Z",
  "end_time": null,
  "active": true,
  "is_lead": true,
  "created_at": "2023-05-08T09:15:00Z",
  "updated_at": "2023-05-08T09:15:00Z",
  "notes": "Setup for job #12345"
}
```

#### End Session

```
PUT /sessions/{id}/end
```

Response:
```json
{
  "id": 123,
  "user_id": 45,
  "user_name": "John Doe",
  "machine_id": 1,
  "machine_name": "Mill #1",
  "start_time": "2023-05-08T09:15:00Z",
  "end_time": "2023-05-08T14:35:00Z",
  "active": false,
  "is_lead": true,
  "created_at": "2023-05-08T09:15:00Z",
  "updated_at": "2023-05-08T14:35:00Z",
  "notes": "Setup for job #12345",
  "duration_seconds": 19200
}
```

#### Update Session Notes

```
PATCH /sessions/{id}
```

Request body:
```json
{
  "notes": "Setup for job #12345 - completed rough machining"
}
```

Response:
```json
{
  "id": 123,
  "user_id": 45,
  "machine_id": 1,
  "notes": "Setup for job #12345 - completed rough machining",
  "updated_at": "2023-05-08T12:30:00Z"
}
```

### Users

#### List Users

```
GET /users
```

Query parameters:
- `active` (optional): Filter by active status (true/false)
- `role` (optional): Filter by role
- `limit` (optional, default=20): Number of results to return
- `offset` (optional, default=0): Pagination offset

Response:
```json
{
  "users": [
    {
      "id": 45,
      "username": "jdoe",
      "name": "John Doe",
      "role": "operator",
      "active": true,
      "rfid_tag": "12345678",
      "created_at": "2022-11-01T09:00:00Z",
      "updated_at": "2023-04-15T14:20:00Z",
      "last_login": "2023-05-08T09:00:00Z"
    },
    ...
  ],
  "total": 58,
  "limit": 20,
  "offset": 0
}
```

#### Get User

```
GET /users/{id}
```

Response:
```json
{
  "id": 45,
  "username": "jdoe",
  "name": "John Doe",
  "role": "operator",
  "active": true,
  "rfid_tag": "12345678",
  "created_at": "2022-11-01T09:00:00Z",
  "updated_at": "2023-04-15T14:20:00Z",
  "last_login": "2023-05-08T09:00:00Z",
  "machine_access": [
    {
      "machine_id": 1,
      "machine_name": "Mill #1",
      "access_level": "operator"
    },
    {
      "machine_id": 3,
      "machine_name": "Mill #3",
      "access_level": "trainee"
    }
  ]
}
```

#### Create User

```
POST /users
```

Request body:
```json
{
  "username": "asmith",
  "password": "secure-password",
  "name": "Alice Smith",
  "role": "manager",
  "active": true,
  "rfid_tag": "87654321"
}
```

Response:
```json
{
  "id": 59,
  "username": "asmith",
  "name": "Alice Smith",
  "role": "manager",
  "active": true,
  "rfid_tag": "87654321",
  "created_at": "2023-05-08T17:00:00Z",
  "updated_at": "2023-05-08T17:00:00Z"
}
```

#### Update User

```
PUT /users/{id}
```

Request body:
```json
{
  "name": "Alice J. Smith",
  "role": "admin",
  "active": true,
  "rfid_tag": "87654321"
}
```

Response:
```json
{
  "id": 59,
  "username": "asmith",
  "name": "Alice J. Smith",
  "role": "admin",
  "active": true,
  "rfid_tag": "87654321",
  "created_at": "2023-05-08T17:00:00Z",
  "updated_at": "2023-05-08T17:30:00Z"
}
```

#### Change Password

```
PUT /users/{id}/password
```

Request body:
```json
{
  "current_password": "secure-password",
  "new_password": "more-secure-password"
}
```

Response:
```json
{
  "success": true,
  "message": "Password changed successfully"
}
```

#### Delete User

```
DELETE /users/{id}
```

Response:
```json
{
  "success": true,
  "message": "User deleted successfully"
}
```

### Areas and Zones

#### List Areas

```
GET /areas
```

Response:
```json
{
  "areas": [
    {
      "id": 1,
      "name": "Production Floor",
      "description": "Main production area",
      "active": true,
      "created_at": "2022-10-01T09:00:00Z",
      "updated_at": "2022-10-01T09:00:00Z",
      "parent_id": null
    },
    ...
  ]
}
```

#### Get Area with Zones

```
GET /areas/{id}
```

Response:
```json
{
  "id": 1,
  "name": "Production Floor",
  "description": "Main production area",
  "active": true,
  "created_at": "2022-10-01T09:00:00Z",
  "updated_at": "2022-10-01T09:00:00Z",
  "parent_id": null,
  "zones": [
    {
      "id": 2,
      "name": "Milling Area",
      "description": "CNC milling machines",
      "area_id": 1,
      "active": true,
      "created_at": "2022-10-01T09:30:00Z",
      "updated_at": "2022-10-01T09:30:00Z",
      "parent_id": null
    },
    {
      "id": 3,
      "name": "Lathe Area",
      "description": "CNC lathes",
      "area_id": 1,
      "active": true,
      "created_at": "2022-10-01T09:45:00Z",
      "updated_at": "2022-10-01T09:45:00Z",
      "parent_id": null
    }
  ]
}
```

#### Create Area

```
POST /areas
```

Request body:
```json
{
  "name": "Assembly Floor",
  "description": "Final assembly area",
  "active": true,
  "parent_id": null
}
```

Response:
```json
{
  "id": 4,
  "name": "Assembly Floor",
  "description": "Final assembly area",
  "active": true,
  "created_at": "2023-05-08T18:00:00Z",
  "updated_at": "2023-05-08T18:00:00Z",
  "parent_id": null
}
```

#### Create Zone

```
POST /zones
```

Request body:
```json
{
  "name": "Quality Control",
  "description": "QC inspection area",
  "area_id": 4,
  "active": true,
  "parent_id": null
}
```

Response:
```json
{
  "id": 7,
  "name": "Quality Control",
  "description": "QC inspection area",
  "area_id": 4,
  "active": true,
  "created_at": "2023-05-08T18:15:00Z",
  "updated_at": "2023-05-08T18:15:00Z",
  "parent_id": null
}
```

### Nodes

#### List Nodes

```
GET /nodes
```

Response:
```json
{
  "nodes": [
    {
      "id": 5,
      "node_id": "ESP32-MILL001",
      "name": "Mill #1 Monitor",
      "node_type": "machine_monitor",
      "ip_address": "192.168.1.100",
      "zone_id": 2,
      "active": true,
      "created_at": "2023-01-15T10:35:00Z",
      "updated_at": "2023-05-01T08:20:00Z",
      "last_seen": "2023-05-08T18:30:00Z",
      "firmware_version": "1.2.3",
      "hardware_type": "esp32-standard",
      "status": "online"
    },
    ...
  ]
}
```

#### Register Node

```
POST /nodes
```

Request body:
```json
{
  "node_id": "ESP32-LATHE002",
  "name": "Lathe #2 Monitor",
  "node_type": "machine_monitor",
  "ip_address": "192.168.1.101",
  "zone_id": 3,
  "active": true,
  "firmware_version": "1.2.3",
  "hardware_type": "esp32-standard"
}
```

Response:
```json
{
  "id": 6,
  "node_id": "ESP32-LATHE002",
  "name": "Lathe #2 Monitor",
  "node_type": "machine_monitor",
  "ip_address": "192.168.1.101",
  "zone_id": 3,
  "active": true,
  "created_at": "2023-05-08T19:00:00Z",
  "updated_at": "2023-05-08T19:00:00Z",
  "last_seen": "2023-05-08T19:00:00Z",
  "firmware_version": "1.2.3",
  "hardware_type": "esp32-standard",
  "status": "online",
  "api_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

#### Submit Node Reading

```
POST /nodes/{node_id}/readings
```

Request body:
```json
{
  "readings": [
    {
      "reading_type": "temperature",
      "value": 24.5,
      "timestamp": "2023-05-08T19:05:00Z"
    },
    {
      "reading_type": "humidity",
      "value": 45.2,
      "timestamp": "2023-05-08T19:05:00Z"
    }
  ]
}
```

Response:
```json
{
  "success": true,
  "readings_accepted": 2,
  "timestamp": "2023-05-08T19:05:01Z"
}
```

#### Update Node Status

```
PUT /nodes/{node_id}/status
```

Request body:
```json
{
  "status": "online",
  "ip_address": "192.168.1.101",
  "firmware_version": "1.2.3"
}
```

Response:
```json
{
  "success": true,
  "node_id": "ESP32-LATHE002",
  "status": "online",
  "last_seen": "2023-05-08T19:10:00Z"
}
```

#### Get Available Firmware

```
GET /firmware
```

Query parameters:
- `hardware_type` (required): Hardware type ('esp32-standard' or 'esp32-cyd')
- `node_type` (optional): Node type ('machine_monitor', 'office_reader', or 'accessory_io')
- `current_version` (optional): Current firmware version

Response:
```json
{
  "firmware": [
    {
      "version": "1.3.0",
      "hardware_type": "esp32-standard",
      "file_url": "/static/downloads/esp32_shop_firmware.zip",
      "release_date": "2025-05-12T12:00:00Z",
      "release_notes": "Added support for area-based operations",
      "size_bytes": 1048576,
      "md5_hash": "a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6",
      "is_critical": false,
      "min_version_required": "1.0.0"
    },
    {
      "version": "1.3.0",
      "hardware_type": "esp32-cyd",
      "file_url": "/static/downloads/esp32_cyd_audio_firmware.zip",
      "release_date": "2025-05-12T12:00:00Z",
      "release_notes": "Added audio broadcasting and Bluetooth support",
      "size_bytes": 2097152,
      "md5_hash": "b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7",
      "is_critical": false,
      "min_version_required": "1.2.0"
    }
  ]
}
```

### Node Hardware Types

The system supports multiple hardware types for nodes:

#### ESP32 Standard

Standard ESP32 devices supporting the following node types:
- `machine_monitor`: Controls machine access with up to 4 RFID readers and activity monitoring
- `office_reader`: Registers new RFID cards into the system
- `accessory_io`: Controls auxiliary devices and sensors

#### ESP32-CYD with Audio

ESP32-CYD devices with color touchscreen display and audio capabilities:
- Supports all standard node types plus enhanced features
- Color TFT display with touchscreen interface
- Audio feedback with Bluetooth audio broadcasting
- Network audio streaming support
- E-STOP audio muting system

```
GET /nodes/types
```

Response:
```json
{
  "hardware_types": [
    {
      "id": "esp32-standard",
      "name": "ESP32 Standard",
      "description": "Standard ESP32 node with support for multiple RFID readers",
      "capabilities": ["rfid", "gpio", "relay_control", "activity_monitoring", "e_stop"]
    },
    {
      "id": "esp32-cyd",
      "name": "ESP32-CYD with Audio",
      "description": "ESP32-CYD with touchscreen display and audio capabilities",
      "capabilities": ["rfid", "gpio", "relay_control", "activity_monitoring", "e_stop", "tft_display", "touch_input", "audio", "bluetooth"]
    }
  ],
  "node_types": [
    {
      "id": "machine_monitor",
      "name": "Machine Monitor",
      "description": "Controls machine access with RFID and monitors activity"
    },
    {
      "id": "office_reader",
      "name": "Office RFID Reader",
      "description": "Registers new RFID cards into the system"
    },
    {
      "id": "accessory_io",
      "name": "Accessory IO Controller",
      "description": "Manages external devices like fans, lights, etc."
    }
  ]
}
```

### Audio Broadcasting Configuration (ESP32-CYD only)

```
GET /nodes/{node_id}/audio
```

Response:
```json
{
  "audio_enabled": true,
  "bluetooth_enabled": true,
  "built_in_volume": 180,
  "bluetooth_volume": 200,
  "connected_devices": [
    {
      "name": "Shop Speaker",
      "address": "AA:BB:CC:DD:EE:FF",
      "connected_since": "2025-05-12T08:30:00Z"
    }
  ],
  "discovered_devices": [
    {
      "name": "Shop Speaker",
      "address": "AA:BB:CC:DD:EE:FF"
    },
    {
      "name": "Office Audio",
      "address": "11:22:33:44:55:66"
    }
  ]
}
```

```
PUT /nodes/{node_id}/audio
```

Request body:
```json
{
  "audio_enabled": true,
  "bluetooth_enabled": true,
  "built_in_volume": 200,
  "bluetooth_volume": 180
}
```

Response:
```json
{
  "success": true,
  "audio_enabled": true,
  "bluetooth_enabled": true,
  "built_in_volume": 200,
  "bluetooth_volume": 180
}
```

```
POST /nodes/{node_id}/audio/bluetooth/scan
```

Response:
```json
{
  "success": true,
  "discovered_devices": [
    {
      "name": "Shop Speaker",
      "address": "AA:BB:CC:DD:EE:FF"
    },
    {
      "name": "Office Audio",
      "address": "11:22:33:44:55:66"
    },
    {
      "name": "Personal Headphones",
      "address": "A1:B2:C3:D4:E5:F6"
    }
  ]
}
```

```
POST /nodes/{node_id}/audio/bluetooth/connect
```

Request body:
```json
{
  "device_address": "11:22:33:44:55:66"
}
```

Response:
```json
{
  "success": true,
  "device": {
    "name": "Office Audio",
    "address": "11:22:33:44:55:66",
    "connected": true
  }
}
```

```
POST /nodes/{node_id}/audio/bluetooth/disconnect
```

Request body:
```json
{
  "device_address": "11:22:33:44:55:66"
}
```

Response:
```json
{
  "success": true,
  "device": {
    "name": "Office Audio",
    "address": "11:22:33:44:55:66",
    "connected": false
  }
}
```

### Alert Sounds (ESP32-CYD only)

```
POST /nodes/{node_id}/audio/alert
```

Request body:
```json
{
  "alert_type": 2,
  "message": "User access granted"
}
```

Response:
```json
{
  "success": true,
  "alert_played": true
}
```

Alert types:
- Type 1: Simple beep
- Type 2: Double beep (typically for access granted)
- Type 3: Warning alert (typically for errors or issues)

### RFID Actions

#### RFID Tag Read

```
POST /rfid/read
```

Request body:
```json
{
  "node_id": "ESP32-MILL001",
  "rfid_tag": "12345678",
  "timestamp": "2023-05-08T19:15:00Z",
  "reader_id": 1
}
```

Response:
```json
{
  "success": true,
  "action": "session_start",
  "user": {
    "id": 45,
    "name": "John Doe"
  },
  "machine": {
    "id": 1,
    "name": "Mill #1"
  },
  "session_id": 124
}
```

### System Status

#### System Health Check

```
GET /status/health
```

Response:
```json
{
  "status": "healthy",
  "version": "1.5.2",
  "uptime": 345600,
  "database": "connected",
  "active_sessions": 15,
  "active_nodes": 8,
  "timestamp": "2023-05-08T19:30:00Z"
}
```

## WebSocket API

The WebSocket API provides real-time updates for machine status, sessions, and system events.

### Connection

Connect to the WebSocket server at:

```
wss://Shop.example.com/ws/v1
```

Authentication is required via query parameter:

```
wss://Shop.example.com/ws/v1?token=YOUR_API_TOKEN
```

### Events

The WebSocket API sends and receives JSON messages:

```json
{
  "type": "event_type",
  "data": {
    // Event-specific data
  },
  "timestamp": "2023-05-08T19:35:00Z"
}
```

#### Event Types

| Event Type | Description | Direction |
|------------|-------------|-----------|
| `session_start` | New machine session started | Server → Client |
| `session_end` | Machine session ended | Server → Client |
| `machine_status` | Machine status change | Server → Client |
| `node_status` | Node status change | Server → Client |
| `rfid_read` | RFID tag read | Client → Server |
| `error` | Error notification | Server → Client |
| `subscribe` | Subscribe to specific events | Client → Server |
| `unsubscribe` | Unsubscribe from events | Client → Server |

#### Subscribe Example

```json
{
  "type": "subscribe",
  "data": {
    "events": ["session_start", "session_end"],
    "filters": {
      "zone_id": 2
    }
  }
}
```

## API Versioning

The API uses URI versioning (v1, v2, etc.). When a new version is released, the previous version will be supported for at least 12 months.

## Rate Limiting and Throttling

To ensure system stability, the API implements rate limiting:

- Standard clients: 100 requests per minute
- Node clients: 300 requests per minute
- Admin clients: 200 requests per minute

When a client exceeds their rate limit, a 429 Too Many Requests response is returned.

## Cross-Origin Resource Sharing (CORS)

The API supports CORS with the following configuration:

- Allowed Origins: Configurable for your deployment
- Allowed Methods: GET, POST, PUT, DELETE, OPTIONS
- Allowed Headers: Content-Type, Authorization
- Exposed Headers: X-RateLimit-Limit, X-RateLimit-Remaining, X-RateLimit-Reset
- Credentials: Supported

## Best Practices

1. **Minimize API Calls**
   - Use filtering parameters to reduce response size
   - Use pagination for large datasets
   - Batch operations when possible

2. **Error Handling**
   - Implement exponential backoff for retries
   - Handle 429 responses appropriately
   - Check error codes and respond accordingly

3. **Security**
   - Store tokens securely
   - Implement token refresh flows
   - Use HTTPS for all requests
   - Validate all server responses

4. **WebSockets**
   - Implement reconnection logic
   - Handle connection drops gracefully
   - Use event filtering to reduce message volume

## Support

For API support, please contact support@example.com or open an issue on our GitHub repository.