# Cross-App Database Architecture for Shop Suite Applications
## Shop Monitor v1.0.1 (Part of ShopSuite v1.0.1)

## Overview

This document outlines the database architecture for the Shop Suite applications:
- **ShopMonitor** - The real-world interface that tracks machines, areas, and shop floor activities
- **ShopTracker** - The office-side application for management, planning, and reporting
- **Shared Infrastructure** - The common database and services that connect these applications

## Database Architecture

The Shop Suite uses a modular database architecture designed for shared data access across multiple applications while maintaining separation of app-specific functionality:

```
shop_suite_db
├── Core (Shared)
│   ├── Users & Authentication
│   ├── Areas & Zones
│   ├── Machines & Equipment
│   ├── Permissions & Access Control
│   ├── Integration & Sync
│   └── Security & Audit
├── ShopMonitor (App-Specific)
│   ├── Node Management
│   ├── Machine Sessions
│   ├── RFID Activities
│   └── Shop Floor Events
└── ShopTracker (App-Specific)
    ├── Orders & Jobs
    ├── Planning & Scheduling
    ├── Analytics
    └── Resource Management
```

## Deployment Model

The database can be deployed in several configurations:

1. **Single Database Server** - All schemas on one server (development/small deployments)
2. **Distributed Database** - Core on central server with app-specific DBs on separate servers
3. **Cloud-Based** - Using managed database services with appropriate connection pooling

## Core Shared Database

### 1. User Management and Single Sign-On (SSO)

```python
class SuiteUser(db.Model):
    __tablename__ = 'suite_users'
    
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(64), unique=True, nullable=False)
    email = db.Column(db.String(120), unique=True, nullable=False)
    password_hash = db.Column(db.String(256), nullable=False)
    display_name = db.Column(db.String(100))
    active = db.Column(db.Boolean, default=True)
    auth_provider = db.Column(db.String(50), default="local")  # local, azure, google, etc.
    external_id = db.Column(db.String(100))  # ID in external auth system
    is_admin = db.Column(db.Boolean, default=False)
    rfid_tag = db.Column(db.String(64))  # Optional RFID association
    
    # Cross-app tracking
    created_by_app = db.Column(db.String(50))  # "shop_monitor" or "shop_tracker"
    managed_by_app = db.Column(db.String(50))  # Which app manages this user
    current_app = db.Column(db.String(50))  # Currently active app
    last_login = db.Column(db.DateTime)     # Last login timestamp
```

### 2. Permissions System

```python
class SuitePermission(db.Model):
    __tablename__ = 'suite_permissions'
    
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('suite_users.id'), nullable=False)
    resource_type = db.Column(db.String(50))  # area, machine, report, etc.
    resource_id = db.Column(db.Integer)  # The ID of the resource
    app_context = db.Column(db.String(50))  # Which app this permission applies to
    permission_level = db.Column(db.String(20))  # view, edit, admin, operate, etc.
```

### 3. Session Management

```python
class UserSession(db.Model):
    __tablename__ = 'user_sessions'
    
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('suite_users.id'), nullable=False)
    session_token = db.Column(db.String(128), unique=True, nullable=False)
    current_app = db.Column(db.String(50))  # Which app user is currently using
    expires_at = db.Column(db.DateTime, nullable=False)
    created_at = db.Column(db.DateTime, default=db.func.now())
    last_activity = db.Column(db.DateTime)
    ip_address = db.Column(db.String(45))  # Store client IP for audit
    user_agent = db.Column(db.String(255))  # Store browser/client info
    invalidated = db.Column(db.Boolean, default=False)  # Flag for revoked tokens
```

### 4. Synchronization and Events

```python
class SyncEvent(db.Model):
    __tablename__ = 'sync_events'
    
    id = db.Column(db.Integer, primary_key=True)
    event_type = db.Column(db.String(50))  # user.created, machine.updated, etc.
    resource_type = db.Column(db.String(50))  # user, machine, area, etc.
    resource_id = db.Column(db.Integer)
    source_app = db.Column(db.String(50))  # Which app generated this event
    target_app = db.Column(db.String(50))  # Which app should process this event
    status = db.Column(db.String(20), default="pending")  # pending, processed, failed
    payload = db.Column(db.Text)  # JSON data for the event
    created_at = db.Column(db.DateTime, default=db.func.now())
    processed_at = db.Column(db.DateTime)
    error_message = db.Column(db.Text)  # Store error details if processing failed
    retry_count = db.Column(db.Integer, default=0)  # Track retry attempts
```

### 5. Security and Audit System

```python
class SecurityEvent(db.Model):
    """Track security-related events for audit purposes"""
    __tablename__ = 'security_events'
    
    id = db.Column(db.Integer, primary_key=True)
    event_type = db.Column(db.String(50))  # login, logout, access_denied, etc.
    user_id = db.Column(db.Integer, db.ForeignKey('suite_users.id'), nullable=True)
    ip_address = db.Column(db.String(45))
    resource_type = db.Column(db.String(50))  # What resource was accessed
    resource_id = db.Column(db.String(50))  # ID of the accessed resource
    app_context = db.Column(db.String(50))  # Which app recorded this event
    success = db.Column(db.Boolean)  # Whether access was granted
    details = db.Column(db.Text)  # Additional security event details
    timestamp = db.Column(db.DateTime, default=db.func.now())

class APIToken(db.Model):
    """Store API tokens for machine-to-server communication"""
    __tablename__ = 'api_tokens'
    
    id = db.Column(db.Integer, primary_key=True)
    token_hash = db.Column(db.String(128), unique=True, nullable=False)
    name = db.Column(db.String(100))  # Descriptive name
    machine_id = db.Column(db.Integer, db.ForeignKey('machines.id'), nullable=True)
    node_id = db.Column(db.Integer, db.ForeignKey('nodes.id'), nullable=True)
    created_by = db.Column(db.Integer, db.ForeignKey('suite_users.id'))
    created_at = db.Column(db.DateTime, default=db.func.now())
    expires_at = db.Column(db.DateTime, nullable=False)
    last_used = db.Column(db.DateTime)
    is_revoked = db.Column(db.Boolean, default=False)
    scopes = db.Column(db.String(255))  # API access scopes, comma separated
```

## App-Specific Data Models

### ShopMonitor Models

The existing models are maintained with references to the new shared models:

1. **Node & Hardware Management**
   - Node model with reference to shared Areas/Zones
   - AccessoryIO model for hardware I/O management
   - BluetoothDevice model for audio devices

2. **Activity Tracking**
   - MachineSession model with reference to shared User model
   - MachineLog model for historical data

3. **Alert & Safety Systems**
   - EStopEvent model
   - Alert model updated to use shared user references

4. **Error Tracking System**
```python
class ErrorLog(db.Model):
    """Track application errors for diagnostics and reliability analysis"""
    __tablename__ = 'error_logs'
    
    id = db.Column(db.Integer, primary_key=True)
    error_code = db.Column(db.Integer)  # Application-specific error code
    error_type = db.Column(db.String(100))  # Exception type or error category
    message = db.Column(db.Text)  # Error message
    stack_trace = db.Column(db.Text, nullable=True)  # Stack trace if available
    user_id = db.Column(db.Integer, db.ForeignKey('suite_users.id'), nullable=True)
    ip_address = db.Column(db.String(45), nullable=True)
    request_path = db.Column(db.String(255), nullable=True)
    request_method = db.Column(db.String(10), nullable=True)
    request_data = db.Column(db.Text, nullable=True)  # Sanitized request data
    app_context = db.Column(db.String(50))  # Which app recorded this error
    timestamp = db.Column(db.DateTime, default=db.func.now())
    is_resolved = db.Column(db.Boolean, default=False)
```

### ShopTracker Models

New models for office-side functionality:

1. **Production Planning**
   - Orders
   - Jobs
   - Work Plans

2. **Resource Management**
   - Materials
   - Inventory
   - Equipment Maintenance

3. **Business Intelligence**
   - Reports
   - Analytics
   - Dashboards

## Integration Patterns

The database architecture supports these integration patterns:

1. **Shared Tables** - Core data accessed directly by both systems
2. **Event Propagation** - Changes in one app trigger events for others
3. **API Services** - Specialized access via REST APIs
4. **Message Queue** - Asynchronous processing using queues

## Security Features

### 1. Authentication & Authorization

The security system includes multiple protections:

1. **Token-based Authentication**
   - API tokens with expiration for machine-to-server communication
   - Token persistence across server restarts
   - Scoped tokens for limited access

2. **Brute Force Protection**
   - IP-based lockout system after multiple failed attempts
   - Exponential backoff for retry attempts
   - Security event logging for forensic analysis

3. **Cross-Site Request Forgery (CSRF) Protection**
   - Token validation for all state-changing operations
   - Origin validation for cross-origin requests

4. **Database-level Protections**
   - Parameterized queries for all database operations
   - Column level encryption for sensitive data
   - Row-level security policies for multi-tenant data

### 2. Input Validation & Sanitization

Multi-layered defense against injection attacks:

1. **Form Validation**
   - Client-side validation for immediate feedback
   - Server-side validation for security enforcement
   - Type checking and constraint validation

2. **Input Sanitization**
   - HTML sanitization to prevent XSS attacks
   - Recursive sanitization for nested data structures
   - Pattern matching against known attack signatures

3. **Output Encoding**
   - Context-aware output encoding
   - HTML entity encoding for dynamic content
   - Content Security Policy headers

## Error Handling Architecture

The application implements a comprehensive error handling system:

1. **Centralized Error Management**
   - Global error handlers for consistent responses
   - Error code system for clear categorization
   - Detailed logging with appropriate redaction of sensitive information

2. **API Error Handling**
   - Consistent JSON error response format
   - Appropriate HTTP status codes
   - Error references for support assistance

3. **Resiliency Patterns**
   - Circuit breakers for external dependencies
   - Retry mechanisms with exponential backoff
   - Graceful degradation of non-critical features

## Scalability Features

### 1. Horizontal Scaling

The database architecture supports dividing workloads across multiple servers:
   - Read replicas for heavy query workloads
   - Sharding by region for international deployments
   - Database connection pooling for efficient resource use

### 2. Vertical Scaling

Increasing resources for the database server:
   - Memory optimization for frequently accessed data
   - Index optimization based on query patterns
   - Query cache configurations

### 3. Caching Strategy

Multi-level caching to reduce database load:
   - Redis/Memcached for frequent queries
   - Application-level caching
   - Database query result caching

## Migration Strategy

Converting from the current database to this cross-app architecture:

1. **Create Shared Schema**:
   - Set up the new shared database tables
   - Deploy the API service layer

2. **Data Migration**:
   - Migrate core data to the shared database
   - Connect existing models to shared models

3. **Application Updates**:
   - Update each app to use the shared database
   - Implement SSO system

4. **Parallel Operation**:
   - Feature flags to control functionality
   - Validate each module before proceeding

## Database Configuration

The database configuration accommodates multiple applications:

```python
# ShopMonitor Config
app.config["SQLALCHEMY_DATABASE_URI"] = os.environ.get(
    "DATABASE_URL", 
    "postgresql://username:password@database.server:5432/shop_suite_db"
)

# Connection pooling for multi-app access
app.config["SQLALCHEMY_ENGINE_OPTIONS"] = {
    "pool_size": 10,
    "max_overflow": 20,
    "pool_recycle": 300,
    "pool_pre_ping": True,
}

# Specify schema (if using PostgreSQL schema separation)
app.config["SQLALCHEMY_BINDS"] = {
    'core': os.environ.get("CORE_DATABASE_URL", app.config["SQLALCHEMY_DATABASE_URI"]),
    'shop_monitor': os.environ.get("MONITOR_DATABASE_URL", app.config["SQLALCHEMY_DATABASE_URI"] + "?schema=shop_monitor"),
}
```

## Security Considerations

The shared database implements several security measures:

1. **Row-Level Security** - Database-enforced access controls
2. **Schema Separation** - Logical isolation of app-specific data
3. **Audit Trails** - Tracking all changes across applications
4. **Encrypted Credentials** - Sensitive data encryption
5. **Connection Firewalls** - Network-level isolation and protection
6. **API Token Lifecycle Management** - Full lifecycle management for API tokens
7. **Security Headers** - Implementation of security headers for all responses

## Backup and Recovery

Comprehensive backup strategy for the shared database:

1. **Continuous Archiving** - For point-in-time recovery
2. **Daily Snapshots** - For quick restores
3. **Cross-Region Replication** - For disaster recovery
4. **Automated Testing** - Regularly validated restore process

## Performance Monitoring

The database includes instrumentation for performance monitoring:

1. **Query Performance** - Tracking slow queries across apps
2. **Connection Usage** - Monitoring connection pools
3. **Growth Metrics** - Tracking table sizes and growth rates
4. **Cross-App Impact** - Identifying how one app affects another's performance
5. **Error Analysis** - Monitoring error rates and patterns for early detection

# Database Schema Documentation

## Overview

This document describes the database schema for the shop Monitor application. The application uses SQLAlchemy as an ORM with migrations managed by Alembic.

## Entity Relationship Diagram

```
+-------------+     +-------------+     +-------------+
|    User     |     |    Area     |     |    Zone     |
+-------------+     +-------------+     +-------------+
| id          |     | id          |     | id          |
| username    |     | name        |     | name        |
| password    |     | description |     | description |
| name        |     | active      |     | area_id     |
| role        |     | created_at  |     | created_at  |
| active      |     | updated_at  |     | updated_at  |
| rfid_tag    |<--->| parent_id   |<--->| parent_id   |
| created_at  |     +-------------+     +-------------+
| updated_at  |           |                    |
+-------------+           |                    |
      |                   |                    |
      v                   v                    v
+-------------+     +-------------+     +-------------+
|   Session   |     |  Machine    |     |    Node     |
+-------------+     +-------------+     +-------------+
| id          |     | id          |     | id          |
| user_id     |     | name        |     | node_id     |
| machine_id  |<--->| machine_id  |<--->| name        |
| start_time  |     | description |     | node_type   |
| end_time    |     | zone_id     |     | ip_address  |
| active      |     | active      |     | zone_id     |
| created_at  |     | created_at  |     | active      |
| updated_at  |     | updated_at  |     | created_at  |
+-------------+     +-------------+     +-------------+
                          |                   |
                          v                   v
                    +-------------+     +-------------+
                    |  Accessory  |     | NodeReading |
                    +-------------+     +-------------+
                    | id          |     | id          |
                    | name        |     | node_id     |
                    | description |     | reading_type|
                    | accessory_id|     | value       |
                    | machine_id  |     | timestamp   |
                    | created_at  |     | created_at  |
                    | updated_at  |     | updated_at  |
                    +-------------+     +-------------+
```

## Security Models

In addition to the core application models, the following security-related models are implemented:

```
+-------------+     +-------------+     +-------------+
| ApiToken    |     | AccessLog   |     | ApiScope    |
+-------------+     +-------------+     +-------------+
| id          |     | id          |     | id          |
| token       |     | user_id     |     | name        |
| name        |     | ip_address  |     | description |
| user_id     |<--->| timestamp   |<--->| code        |
| expires_at  |     | method      |     | created_at  |
| created_at  |     | path        |     | updated_at  |
| updated_at  |     | status_code |     +-------------+
| last_used_at|     | user_agent  |          |
| is_revoked  |     | created_at  |          |
| scopes      |     +-------------+          |
+-------------+                              |
      |                                      |
      v                                      v
+-------------+     +-------------+     +-------------+
| FailedLogin |     | SecuritySetting |  | ErrorLog   |
+-------------+     +-------------+     +-------------+
| id          |     | id          |     | id          |
| user_id     |     | key         |     | code        |
| ip_address  |     | value       |     | message     |
| timestamp   |     | description |     | details     |
| attempt_count|    | created_at  |     | stack_trace |
| locked_until|     | updated_at  |     | user_id     |
| created_at  |     | is_override |     | ip_address  |
| updated_at  |     +-------------+     | timestamp   |
+-------------+                          | severity    |
                                        | reference_id|
                                        | created_at  |
                                        +-------------+
```

## Table Descriptions

### Core Application Tables

#### Users

Stores user information including authentication details.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| username    | String(80)   | Unique username                       |
| password    | String(255)  | Hashed password                       |
| name        | String(120)  | Full name                             |
| role        | String(20)   | User role (admin, manager, user, etc.)|
| active      | Boolean      | Whether user account is active        |
| rfid_tag    | String(20)   | RFID card number                      |
| created_at  | DateTime     | Time when user was created            |
| updated_at  | DateTime     | Time when user was last updated       |
| last_login  | DateTime     | Last login timestamp                  |
| is_admin_override | Boolean| Whether user has admin override       |

#### Areas

Represents physical areas in the shop.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| name        | String(80)   | Area name                             |
| description | Text         | Area description                      |
| active      | Boolean      | Whether area is active                |
| created_at  | DateTime     | Time when area was created            |
| updated_at  | DateTime     | Time when area was last updated       |
| parent_id   | Integer      | Parent area ID (self-reference)       |

#### Zones

Represents zones within areas.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| name        | String(80)   | Zone name                             |
| description | Text         | Zone description                      |
| area_id     | Integer      | Foreign key to areas                  |
| created_at  | DateTime     | Time when zone was created            |
| updated_at  | DateTime     | Time when zone was last updated       |
| parent_id   | Integer      | Parent zone ID (self-reference)       |

#### Machines

Represents machines that can be monitored.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| name        | String(80)   | Machine name                          |
| machine_id  | String(20)   | Unique external machine identifier    |
| description | Text         | Machine description                   |
| zone_id     | Integer      | Foreign key to zones                  |
| active      | Boolean      | Whether machine is active             |
| created_at  | DateTime     | Time when machine was created         |
| updated_at  | DateTime     | Time when machine was last updated    |
| requires_training | Boolean| Training required for access          |
| access_level| String(20)   | Required access level                 |
| max_users   | Integer      | Maximum number of concurrent users    |

#### Sessions

Records machine usage sessions.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| user_id     | Integer      | Foreign key to users                  |
| machine_id  | Integer      | Foreign key to machines               |
| start_time  | DateTime     | Session start time                    |
| end_time    | DateTime     | Session end time (null if active)     |
| active      | Boolean      | Whether session is active             |
| created_at  | DateTime     | Time when session was created         |
| updated_at  | DateTime     | Time when session was last updated    |
| is_lead     | Boolean      | Whether user is lead operator         |
| notes       | Text         | Session notes                         |

#### Nodes

Represents physical ESP32 monitoring nodes.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| node_id     | String(20)   | Unique node identifier                |
| name        | String(80)   | Node name                             |
| node_type   | String(20)   | Type of node                          |
| ip_address  | String(15)   | Node IP address                       |
| zone_id     | Integer      | Foreign key to zones                  |
| active      | Boolean      | Whether node is active                |
| created_at  | DateTime     | Time when node was created            |
| updated_at  | DateTime     | Time when node was last updated       |
| last_seen   | DateTime     | Last contact time                     |
| firmware_version | String(15) | Current firmware version           |
| status      | String(20)   | Node status                           |

#### NodeReadings

Stores readings from nodes.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| node_id     | Integer      | Foreign key to nodes                  |
| reading_type| String(20)   | Type of reading                       |
| value       | Float        | Reading value                         |
| timestamp   | DateTime     | Time of reading                       |
| created_at  | DateTime     | Time when reading was recorded        |
| updated_at  | DateTime     | Time when reading was last updated    |

#### Accessories

Represents accessories attached to machines.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| name        | String(80)   | Accessory name                        |
| description | Text         | Accessory description                 |
| accessory_id| String(20)   | Unique accessory identifier           |
| machine_id  | Integer      | Foreign key to machines               |
| created_at  | DateTime     | Time when accessory was created       |
| updated_at  | DateTime     | Time when accessory was last updated  |
| accessory_type | String(20)| Type of accessory                     |
| status      | String(20)   | Current status                        |

### Security and Audit Tables

#### ApiTokens

Stores API authentication tokens with enhanced security features.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| token       | String(255)  | Hashed token value                    |
| encrypted_token | String(512) | Encrypted token for persistence    |
| name        | String(80)   | Token name/purpose                    |
| user_id     | Integer      | Foreign key to users                  |
| expires_at  | DateTime     | Expiration timestamp                  |
| created_at  | DateTime     | Time when token was created           |
| updated_at  | DateTime     | Time when token was last updated      |
| last_used_at| DateTime     | Last time token was used              |
| is_revoked  | Boolean      | Whether token has been revoked        |
| device_id   | String(80)   | Associated device identifier          |
| token_type  | String(20)   | Type of token (bearer, refresh, etc.) |
| scopes      | String(255)  | Comma-separated list of scopes        |

#### ApiScopes

Defines API permission scopes for granular access control.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| name        | String(80)   | Scope name                            |
| description | Text         | Scope description                     |
| code        | String(20)   | Scope code                            |
| created_at  | DateTime     | Time when scope was created           |
| updated_at  | DateTime     | Time when scope was last updated      |
| parent_id   | Integer      | Parent scope ID for hierarchical permissions |
| is_default  | Boolean      | Whether this scope is assigned by default |

#### AccessLogs

Enhanced audit logging for API and web access.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| user_id     | Integer      | Foreign key to users (nullable)       |
| ip_address  | String(45)   | Client IP address                     |
| timestamp   | DateTime     | Time of access with microsecond precision |
| method      | String(10)   | HTTP method                           |
| path        | String(255)  | Request path                          |
| status_code | Integer      | Response HTTP status code             |
| user_agent  | String(255)  | Client user agent string              |
| created_at  | DateTime     | Time when log was created             |
| request_id  | String(36)   | Unique request identifier             |
| duration_ms | Integer      | Request duration in milliseconds      |
| content_length | Integer   | Response size in bytes                |
| token_id    | Integer      | Associated API token ID if applicable |
| request_headers | Text     | Sanitized request headers             |
| response_headers | Text    | Response headers                      |
| is_suspicious | Boolean    | Flag for suspicious activity          |

#### SecurityEvents

Specialized table for security-related events.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| event_type  | String(50)   | Type of security event                |
| severity    | String(20)   | Event severity (info, warning, critical) |
| user_id     | Integer      | Associated user ID (nullable)         |
| ip_address  | String(45)   | Client IP address                     |
| timestamp   | DateTime     | Time of event with microsecond precision |
| details     | Text         | JSON-structured event details         |
| related_resource | String(50) | Related resource type              |
| resource_id | Integer      | ID of the related resource            |
| created_at  | DateTime     | Time when event was logged            |
| request_id  | String(36)   | Unique request identifier             |
| session_id  | String(36)   | Associated session ID                 |
| geo_location | String(100) | Geographic location if available      |

#### FailedLogins

Enhanced tracking of failed login attempts with intelligent lockout.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| user_id     | Integer      | Foreign key to users (nullable)       |
| ip_address  | String(45)   | Client IP address                     |
| timestamp   | DateTime     | Time of failed attempt                |
| attempt_count| Integer     | Consecutive failed attempts           |
| locked_until| DateTime     | Account lockout end time              |
| created_at  | DateTime     | Time when record was created          |
| updated_at  | DateTime     | Time when record was last updated     |
| username    | String(80)   | Attempted username                    |
| backoff_multiplier | Integer | Current exponential backoff multiplier |
| lockout_severity | String(20) | Lockout severity level (low/medium/high) |
| suspicious_activity | Boolean | Flag for potentially malicious activity |

#### ErrorLogs

Comprehensive error tracking system.

| Column      | Type         | Description                           |
|-------------|--------------|---------------------------------------|
| id          | Integer      | Primary key                           |
| error_code  | Integer      | Application-specific error code       |
| category    | String(50)   | Error category (auth, validation, system) |
| message     | String(255)  | Error message                         |
| details     | Text         | JSON-structured error details         |
| stack_trace | Text         | Stack trace (in development)          |
| user_id     | Integer      | Associated user ID (nullable)         |
| ip_address  | String(45)   | Client IP address                     |
| timestamp   | DateTime     | Time of error with microsecond precision |
| severity    | String(20)   | Error severity                        |
| reference_id| String(20)   | User-facing reference code            |
| created_at  | DateTime     | Time when error was logged            |
| request_id  | String(36)   | Unique request identifier             |
| endpoint    | String(255)  | Endpoint where error occurred         |
| resolved    | Boolean      | Whether this error has been resolved  |
| resolution_notes | Text    | Notes on how error was resolved       |
| is_recurring | Boolean     | Flag for recurring error patterns     |

## Error Handling Model

The application implements a structured error handling system with the following categories:

### Application Error Codes

| Code Range | Category      | Description                           |
|------------|--------------|---------------------------------------|
| 1000-1999  | Authentication | Authentication-related errors        |
| 2000-2999  | Authorization | Permission and access control errors  |
| 3000-3999  | Validation   | Input validation errors               |
| 4000-4999  | Resource     | Resource-related errors (not found, conflict) |
| 5000-5999  | System       | System and service errors             |

### Error Response Format

All API errors return a consistent JSON format:

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

### Resilience Patterns

The application implements several resilience patterns:

1. **Circuit Breaker**
   - Tracks failures to external services
   - Opens circuit after threshold is reached
   - Prevents cascading failures
   - Automatic recovery with half-open state

2. **Exponential Backoff**
   - Progressive retry delays
   - Configurable initial delay and maximum
   - Jitter for distributed systems
   - Header-based client guidance

3. **Graceful Degradation**
   - Core functionality remains available during partial outages
   - Feature flags for non-critical components
   - Fallback mechanisms for dependent services
   - Progressive enhancement strategy

## Relationships

- **User** 1:N **Session** - One user can have many sessions
- **Machine** 1:N **Session** - One machine can have many sessions
- **Zone** 1:N **Machine** - One zone can have many machines
- **Area** 1:N **Zone** - One area can have many zones
- **Zone** 1:N **Node** - One zone can have many nodes
- **Machine** 1:N **Accessory** - One machine can have many accessories
- **User** 1:N **ApiToken** - One user can have many API tokens
- **ApiToken** N:N **ApiScope** - Many tokens can have many scopes

## Indexes

The following indexes are defined to optimize query performance:

- **users**: username, rfid_tag
- **areas**: name, parent_id
- **zones**: name, area_id, parent_id
- **machines**: machine_id, zone_id
- **sessions**: user_id, machine_id, start_time, active
- **nodes**: node_id, zone_id, ip_address
- **api_tokens**: token, user_id, expires_at
- **access_logs**: user_id, ip_address, timestamp
- **failed_logins**: ip_address, user_id, locked_until

## Migrations

Database migrations are managed using Alembic. The migration history includes:

- **001_add_node_and_accessory_models.py**: Added Node and Accessory models
- **002_add_area_and_hierarchical_structure.py**: Added Area model and hierarchical structure
- **003_add_shop_suite_shared_tables.py**: Added integration with Shop Suite
- **004_add_security_and_audit_tables.py**: Added security and audit logging tables
- **005_add_error_handling_tables.py**: Added error logging tables

To run migrations:

```bash
flask db upgrade
```

To create a new migration:

```bash
flask db migrate -m "Description of changes"
```

## Backup and Recovery

The database should be backed up regularly. For production databases, use:

```bash
pg_dump -U username shop_monitor > /path/to/backups/shop_monitor_$(date +%Y%m%d).sql
```

For SQLite databases:

```bash
cp instance/shop_tracker.db /path/to/backups/shop_tracker_$(date +%Y%m%d).db
```

## Security Considerations

- All passwords are stored using bcrypt hashing
- API tokens are hashed in the database
- Session cookies use secure and httpOnly flags
- Database connections use TLS encryption in production
- User input is sanitized before storage
- Prepared statements are used to prevent SQL injection

## Performance Tuning

For large installations, consider the following optimizations:

- Index frequently queried columns
- Partition large tables (sessions, node_readings)
- Set up read replicas for reporting queries
- Implement query caching for frequent operations
- Consider time-series databases for reading data