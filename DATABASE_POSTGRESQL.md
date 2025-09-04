

# ShopMonitor PostgreSQL Database Schema
## Version 1.0.1 - Complete Database Information

This document provides comprehensive PostgreSQL database schema, setup instructions, and maintenance information for the ShopMonitor application.

## Table of Contents
1. [Database Overview](#database-overview)
2. [PostgreSQL Setup](#postgresql-setup)
3. [Complete Schema DDL](#complete-schema-ddl)
4. [Data Types and Constraints](#data-types-and-constraints)
5. [Indexes and Performance](#indexes-and-performance)
6. [Security and Permissions](#security-and-permissions)
7. [Backup and Recovery](#backup-and-recovery)
8. [Maintenance Procedures](#maintenance-procedures)
9. [Migration Scripts](#migration-scripts)
10. [Performance Tuning](#performance-tuning)

## Database Overview

ShopMonitor uses PostgreSQL as its primary database system to store:
- User authentication and authorization data
- Manufacturing area and zone organization
- Machine definitions and configurations
- RFID access sessions and activity logs
- ESP32 node management and status
- Security events and audit trails
- Error logs and system diagnostics

### Key Features
- Multi-tenant architecture with area-based permissions
- Hierarchical organization (Areas > Zones > Machines)
- Real-time session tracking
- Comprehensive audit logging
- Integration with Shop Suite applications

## PostgreSQL Setup

### System Requirements
- PostgreSQL 12.0 or higher
- Minimum 4GB RAM for production
- SSD storage recommended for better I/O performance
- UTF-8 encoding support

### Installation and Configuration

#### 1. Database Creation
```sql
-- Create database
CREATE DATABASE shop_monitor 
    WITH ENCODING 'UTF8' 
    LC_COLLATE='en_US.UTF-8' 
    LC_CTYPE='en_US.UTF-8';

-- Create application user
CREATE USER shop_monitor_user WITH PASSWORD 'secure_password_here';

-- Grant privileges
GRANT ALL PRIVILEGES ON DATABASE shop_monitor TO shop_monitor_user;
```

#### 2. Extensions
```sql
-- Connect to shop_monitor database
\c shop_monitor

-- Enable required extensions
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";
CREATE EXTENSION IF NOT EXISTS "pg_stat_statements";
```

## Complete Schema DDL

### Core User Management

```sql
-- Roles table for user permission levels
CREATE TABLE roles (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) UNIQUE NOT NULL,
    description VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Users table for system authentication
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(64) UNIQUE NOT NULL,
    email VARCHAR(120) UNIQUE NOT NULL,
    password_hash VARCHAR(256) NOT NULL,
    role_id INTEGER REFERENCES roles(id),
    rfid_tag VARCHAR(50) UNIQUE,
    active BOOLEAN DEFAULT TRUE,
    last_login TIMESTAMP,
    failed_login_attempts INTEGER DEFAULT 0,
    locked_until TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Trigger to update updated_at timestamp
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ language 'plpgsql';

CREATE TRIGGER update_users_updated_at 
    BEFORE UPDATE ON users 
    FOR EACH ROW 
    EXECUTE FUNCTION update_updated_at_column();
```

### Organizational Structure

```sql
-- Areas table (top-level organization)
CREATE TABLE areas (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    description TEXT,
    code VARCHAR(10) UNIQUE NOT NULL,
    active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TRIGGER update_areas_updated_at 
    BEFORE UPDATE ON areas 
    FOR EACH ROW 
    EXECUTE FUNCTION update_updated_at_column();

-- Zones table (sub-areas within areas)
CREATE TABLE zones (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    description TEXT,
    area_id INTEGER NOT NULL REFERENCES areas(id) ON DELETE CASCADE,
    code VARCHAR(10),
    active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(area_id, code)
);

CREATE TRIGGER update_zones_updated_at 
    BEFORE UPDATE ON zones 
    FOR EACH ROW 
    EXECUTE FUNCTION update_updated_at_column();

-- Area permissions for users
CREATE TABLE area_permissions (
    id SERIAL PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    area_id INTEGER NOT NULL REFERENCES areas(id) ON DELETE CASCADE,
    permission_level VARCHAR(20) DEFAULT 'view' CHECK (permission_level IN ('view', 'operate', 'manage', 'admin')),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_id, area_id)
);
```

### Machine Management

```sql
-- Machines table
CREATE TABLE machines (
    id SERIAL PRIMARY KEY,
    machine_id VARCHAR(50) UNIQUE NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    zone_id INTEGER REFERENCES zones(id) ON DELETE SET NULL,
    node_id INTEGER,  -- Will reference nodes table
    node_port INTEGER DEFAULT 0,
    active BOOLEAN DEFAULT TRUE,
    requires_rfid BOOLEAN DEFAULT TRUE,
    timeout_minutes INTEGER DEFAULT 60,
    warning_minutes INTEGER DEFAULT 5,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TRIGGER update_machines_updated_at 
    BEFORE UPDATE ON machines 
    FOR EACH ROW 
    EXECUTE FUNCTION update_updated_at_column();

-- Machine sessions for tracking usage
CREATE TABLE machine_sessions (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    machine_id INTEGER NOT NULL REFERENCES machines(id) ON DELETE CASCADE,
    rfid_tag VARCHAR(50),
    start_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    end_time TIMESTAMP,
    active BOOLEAN DEFAULT TRUE,
    activity_count INTEGER DEFAULT 0,
    last_activity TIMESTAMP,
    timeout_warning_sent BOOLEAN DEFAULT FALSE,
    ended_by VARCHAR(20) DEFAULT 'user' CHECK (ended_by IN ('user', 'timeout', 'admin', 'estop')),
    notes TEXT
);

-- Machine activity logs
CREATE TABLE machine_logs (
    id SERIAL PRIMARY KEY,
    machine_id INTEGER NOT NULL REFERENCES machines(id) ON DELETE CASCADE,
    user_id INTEGER REFERENCES users(id),
    session_id INTEGER REFERENCES machine_sessions(id),
    event_type VARCHAR(50) NOT NULL,
    event_data JSONB,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    ip_address INET
);
```

### ESP32 Node Management

```sql
-- ESP32 nodes table
CREATE TABLE nodes (
    id SERIAL PRIMARY KEY,
    node_id VARCHAR(32) UNIQUE NOT NULL,
    name VARCHAR(64) NOT NULL,
    description TEXT,
    ip_address INET,
    node_type VARCHAR(20) DEFAULT 'machine_monitor' CHECK (node_type IN ('machine_monitor', 'office_reader', 'accessory_io')),
    zone_id INTEGER REFERENCES zones(id),
    is_esp32 BOOLEAN DEFAULT FALSE,
    last_seen TIMESTAMP,
    firmware_version VARCHAR(20),
    config JSONB,
    status VARCHAR(20) DEFAULT 'offline' CHECK (status IN ('online', 'offline', 'error', 'updating')),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TRIGGER update_nodes_updated_at 
    BEFORE UPDATE ON nodes 
    FOR EACH ROW 
    EXECUTE FUNCTION update_updated_at_column();

-- Add foreign key to machines table for node reference
ALTER TABLE machines 
ADD CONSTRAINT fk_machine_node 
FOREIGN KEY (node_id) REFERENCES nodes(id);

-- Accessory I/O management
CREATE TABLE accessory_io (
    id SERIAL PRIMARY KEY,
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    name VARCHAR(64) NOT NULL,
    description TEXT,
    io_type VARCHAR(10) NOT NULL CHECK (io_type IN ('input', 'output')),
    io_number INTEGER NOT NULL,
    linked_machine_id INTEGER REFERENCES machines(id),
    activation_delay INTEGER DEFAULT 0,
    invert_logic BOOLEAN DEFAULT FALSE,
    current_state BOOLEAN DEFAULT FALSE,
    last_changed TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(node_id, io_number, io_type)
);

-- Bluetooth devices for ESP32-CYD nodes
CREATE TABLE bluetooth_devices (
    id SERIAL PRIMARY KEY,
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    device_name VARCHAR(100),
    device_address VARCHAR(17) UNIQUE, -- MAC address format
    device_type VARCHAR(50),
    is_connected BOOLEAN DEFAULT FALSE,
    last_seen TIMESTAMP,
    signal_strength INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Security and Audit

```sql
-- API tokens for machine authentication
CREATE TABLE api_tokens (
    id SERIAL PRIMARY KEY,
    token_hash VARCHAR(128) UNIQUE NOT NULL,
    name VARCHAR(100),
    machine_id INTEGER REFERENCES machines(id),
    node_id INTEGER REFERENCES nodes(id),
    created_by INTEGER REFERENCES users(id),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL,
    last_used TIMESTAMP,
    is_revoked BOOLEAN DEFAULT FALSE,
    scopes VARCHAR(255) -- Comma-separated list
);

-- Security events for audit trail
CREATE TABLE security_events (
    id SERIAL PRIMARY KEY,
    event_type VARCHAR(50) NOT NULL,
    user_id INTEGER REFERENCES users(id),
    ip_address INET,
    resource_type VARCHAR(50),
    resource_id VARCHAR(50),
    app_context VARCHAR(50),
    success BOOLEAN,
    details JSONB,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Failed login attempts tracking
CREATE TABLE failed_logins (
    id SERIAL PRIMARY KEY,
    username VARCHAR(64),
    ip_address INET NOT NULL,
    user_agent TEXT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX(ip_address, timestamp),
    INDEX(username, timestamp)
);

-- Emergency stop events
CREATE TABLE estop_events (
    id SERIAL PRIMARY KEY,
    triggered_by INTEGER REFERENCES users(id),
    node_id INTEGER REFERENCES nodes(id),
    zone_id INTEGER REFERENCES zones(id),
    event_type VARCHAR(20) DEFAULT 'emergency_stop' CHECK (event_type IN ('emergency_stop', 'reset', 'test')),
    description TEXT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    acknowledged_by INTEGER REFERENCES users(id),
    acknowledged_at TIMESTAMP
);
```

### Error and Event Logging

```sql
-- Error logs for application diagnostics
CREATE TABLE error_logs (
    id SERIAL PRIMARY KEY,
    error_code INTEGER,
    error_type VARCHAR(100),
    message TEXT NOT NULL,
    stack_trace TEXT,
    user_id INTEGER REFERENCES users(id),
    ip_address INET,
    request_path VARCHAR(255),
    request_method VARCHAR(10),
    request_data JSONB,
    app_context VARCHAR(50),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_resolved BOOLEAN DEFAULT FALSE,
    resolved_by INTEGER REFERENCES users(id),
    resolved_at TIMESTAMP
);

-- System alerts
CREATE TABLE alerts (
    id SERIAL PRIMARY KEY,
    alert_type VARCHAR(50) NOT NULL,
    severity VARCHAR(20) DEFAULT 'info' CHECK (severity IN ('info', 'warning', 'error', 'critical')),
    title VARCHAR(200) NOT NULL,
    message TEXT,
    source_type VARCHAR(50), -- 'machine', 'node', 'system', etc.
    source_id INTEGER,
    user_id INTEGER REFERENCES users(id),
    machine_id INTEGER REFERENCES machines(id),
    node_id INTEGER REFERENCES nodes(id),
    acknowledged BOOLEAN DEFAULT FALSE,
    acknowledged_by INTEGER REFERENCES users(id),
    acknowledged_at TIMESTAMP,
    expires_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Shop Suite Integration

```sql
-- Sync events for cross-application data sharing
CREATE TABLE sync_events (
    id SERIAL PRIMARY KEY,
    event_type VARCHAR(50) NOT NULL,
    resource_type VARCHAR(50) NOT NULL,
    resource_id INTEGER,
    source_app VARCHAR(50) NOT NULL,
    target_app VARCHAR(50),
    status VARCHAR(20) DEFAULT 'pending' CHECK (status IN ('pending', 'processed', 'failed')),
    payload JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    processed_at TIMESTAMP,
    error_message TEXT,
    retry_count INTEGER DEFAULT 0
);
```

## Data Types and Constraints

### Key Data Type Decisions

1. **SERIAL vs UUID**: Using SERIAL for better performance and simplicity
2. **INET vs VARCHAR**: Using INET for IP addresses for better validation and indexing
3. **JSONB vs JSON**: Using JSONB for better performance and indexing capabilities
4. **TIMESTAMP vs TIMESTAMPTZ**: Using TIMESTAMP with application-level timezone handling

### Important Constraints

```sql
-- Add additional check constraints
ALTER TABLE users ADD CONSTRAINT check_email_format 
    CHECK (email ~* '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$');

ALTER TABLE machines ADD CONSTRAINT check_timeout_minutes 
    CHECK (timeout_minutes > 0 AND timeout_minutes <= 1440);

ALTER TABLE machine_sessions ADD CONSTRAINT check_session_times 
    CHECK (end_time IS NULL OR end_time >= start_time);

ALTER TABLE api_tokens ADD CONSTRAINT check_expires_future 
    CHECK (expires_at > created_at);
```

## Indexes and Performance

### Primary Indexes

```sql
-- User-related indexes
CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_users_rfid_tag ON users(rfid_tag) WHERE rfid_tag IS NOT NULL;
CREATE INDEX idx_users_role_id ON users(role_id);
CREATE INDEX idx_users_active ON users(active) WHERE active = TRUE;

-- Machine and session indexes
CREATE INDEX idx_machines_machine_id ON machines(machine_id);
CREATE INDEX idx_machines_zone_id ON machines(zone_id);
CREATE INDEX idx_machines_node_id ON machines(node_id);
CREATE INDEX idx_machines_active ON machines(active) WHERE active = TRUE;

CREATE INDEX idx_machine_sessions_user_id ON machine_sessions(user_id);
CREATE INDEX idx_machine_sessions_machine_id ON machine_sessions(machine_id);
CREATE INDEX idx_machine_sessions_active ON machine_sessions(active) WHERE active = TRUE;
CREATE INDEX idx_machine_sessions_start_time ON machine_sessions(start_time);
CREATE INDEX idx_machine_sessions_rfid_tag ON machine_sessions(rfid_tag);

-- Node and area indexes
CREATE INDEX idx_nodes_node_id ON nodes(node_id);
CREATE INDEX idx_nodes_ip_address ON nodes(ip_address);
CREATE INDEX idx_nodes_zone_id ON nodes(zone_id);
CREATE INDEX idx_nodes_status ON nodes(status);
CREATE INDEX idx_nodes_last_seen ON nodes(last_seen);

CREATE INDEX idx_areas_code ON areas(code);
CREATE INDEX idx_areas_active ON areas(active) WHERE active = TRUE;
CREATE INDEX idx_zones_area_id ON zones(area_id);
CREATE INDEX idx_zones_code ON zones(code);
CREATE INDEX idx_zones_active ON zones(active) WHERE active = TRUE;

-- Security and audit indexes
CREATE INDEX idx_security_events_timestamp ON security_events(timestamp);
CREATE INDEX idx_security_events_user_id ON security_events(user_id);
CREATE INDEX idx_security_events_ip_address ON security_events(ip_address);
CREATE INDEX idx_security_events_event_type ON security_events(event_type);

CREATE INDEX idx_api_tokens_token_hash ON api_tokens(token_hash);
CREATE INDEX idx_api_tokens_expires_at ON api_tokens(expires_at);
CREATE INDEX idx_api_tokens_node_id ON api_tokens(node_id);

-- Error and log indexes
CREATE INDEX idx_error_logs_timestamp ON error_logs(timestamp);
CREATE INDEX idx_error_logs_error_type ON error_logs(error_type);
CREATE INDEX idx_error_logs_is_resolved ON error_logs(is_resolved) WHERE is_resolved = FALSE;

CREATE INDEX idx_machine_logs_timestamp ON machine_logs(timestamp);
CREATE INDEX idx_machine_logs_machine_id ON machine_logs(machine_id);
CREATE INDEX idx_machine_logs_event_type ON machine_logs(event_type);
```

### Composite Indexes

```sql
-- Multi-column indexes for common queries
CREATE INDEX idx_area_permissions_user_area ON area_permissions(user_id, area_id);
CREATE INDEX idx_accessory_io_node_type ON accessory_io(node_id, io_type);
CREATE INDEX idx_failed_logins_ip_time ON failed_logins(ip_address, timestamp);
CREATE INDEX idx_alerts_source ON alerts(source_type, source_id) WHERE acknowledged = FALSE;
CREATE INDEX idx_sync_events_status_app ON sync_events(status, target_app);
```

### Partial Indexes for Performance

```sql
-- Indexes for active records only
CREATE INDEX idx_active_sessions ON machine_sessions(machine_id, start_time) 
    WHERE active = TRUE;

CREATE INDEX idx_unacknowledged_alerts ON alerts(created_at, severity) 
    WHERE acknowledged = FALSE;

CREATE INDEX idx_valid_tokens ON api_tokens(node_id, last_used) 
    WHERE is_revoked = FALSE AND expires_at > CURRENT_TIMESTAMP;
```

## Security and Permissions

### Database User Setup

```sql
-- Create application user with limited privileges
CREATE USER shop_monitor_app WITH PASSWORD 'app_password_here';

-- Grant table-level permissions
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO shop_monitor_app;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO shop_monitor_app;

-- Create read-only user for reporting
CREATE USER shop_monitor_readonly WITH PASSWORD 'readonly_password_here';
GRANT SELECT ON ALL TABLES IN SCHEMA public TO shop_monitor_readonly;

-- Create backup user
CREATE USER shop_monitor_backup WITH PASSWORD 'backup_password_here';
GRANT SELECT ON ALL TABLES IN SCHEMA public TO shop_monitor_backup;
```

### Row Level Security (RLS)

```sql
-- Enable RLS on sensitive tables
ALTER TABLE users ENABLE ROW LEVEL SECURITY;
ALTER TABLE area_permissions ENABLE ROW LEVEL SECURITY;
ALTER TABLE machine_sessions ENABLE ROW LEVEL SECURITY;

-- Example RLS policy for area-based access
CREATE POLICY user_area_access ON machine_sessions
    FOR ALL TO shop_monitor_app
    USING (
        machine_id IN (
            SELECT m.id FROM machines m
            JOIN zones z ON m.zone_id = z.id
            JOIN area_permissions ap ON z.area_id = ap.area_id
            WHERE ap.user_id = current_setting('app.current_user_id')::integer
        )
    );
```

## Backup and Recovery

### Automated Backup Script

```bash
#!/bin/bash
# backup_shop_monitor.sh

DB_NAME="shop_monitor"
DB_USER="shop_monitor_backup"
BACKUP_DIR="/var/backups/shop_monitor"
DATE=$(date +%Y%m%d_%H%M%S)

# Create backup directory if it doesn't exist
mkdir -p $BACKUP_DIR

# Full database backup
pg_dump -U $DB_USER -h localhost -F c -b -v -f $BACKUP_DIR/shop_monitor_full_$DATE.backup $DB_NAME

# Schema-only backup
pg_dump -U $DB_USER -h localhost -s -f $BACKUP_DIR/shop_monitor_schema_$DATE.sql $DB_NAME

# Compress old backups (older than 7 days)
find $BACKUP_DIR -name "*.backup" -type f -mtime +7 -exec gzip {} \;

# Remove very old backups (older than 30 days)
find $BACKUP_DIR -name "*.backup.gz" -type f -mtime +30 -delete
find $BACKUP_DIR -name "*.sql" -type f -mtime +30 -delete

echo "Backup completed: shop_monitor_full_$DATE.backup"
```

### Point-in-Time Recovery Setup

```sql
-- Enable archiving in postgresql.conf
-- archive_mode = on
-- archive_command = 'cp %p /var/lib/postgresql/archive/%f'
-- wal_level = replica

-- Create base backup
-- pg_basebackup -U shop_monitor_backup -h localhost -D /var/backups/shop_monitor/base -Ft -z -P
```

### Recovery Procedures

```bash
# Restore from custom format backup
pg_restore -U shop_monitor_user -h localhost -d shop_monitor_new -v shop_monitor_full_20250817.backup

# Point-in-time recovery
# 1. Stop PostgreSQL
# 2. Replace data directory with base backup
# 3. Create recovery.conf with target time
# 4. Start PostgreSQL
```

## Maintenance Procedures

### Regular Maintenance Tasks

```sql
-- Vacuum and analyze (run weekly)
VACUUM ANALYZE;

-- Reindex tables (run monthly)
REINDEX DATABASE shop_monitor;

-- Update table statistics
ANALYZE;

-- Check for bloat
SELECT schemaname, tablename, 
       pg_size_pretty(pg_total_relation_size(schemaname||'.'||tablename)) as size,
       pg_size_pretty(pg_relation_size(schemaname||'.'||tablename)) as table_size
FROM pg_tables 
WHERE schemaname = 'public' 
ORDER BY pg_total_relation_size(schemaname||'.'||tablename) DESC;
```

### Cleanup Procedures

```sql
-- Clean old machine logs (older than 1 year)
DELETE FROM machine_logs 
WHERE timestamp < CURRENT_TIMESTAMP - INTERVAL '1 year';

-- Clean old security events (older than 2 years)
DELETE FROM security_events 
WHERE timestamp < CURRENT_TIMESTAMP - INTERVAL '2 years';

-- Clean old error logs that are resolved (older than 6 months)
DELETE FROM error_logs 
WHERE is_resolved = TRUE 
AND resolved_at < CURRENT_TIMESTAMP - INTERVAL '6 months';

-- Clean expired API tokens
DELETE FROM api_tokens 
WHERE expires_at < CURRENT_TIMESTAMP;

-- Clean old failed login attempts (older than 30 days)
DELETE FROM failed_logins 
WHERE timestamp < CURRENT_TIMESTAMP - INTERVAL '30 days';
```

## Migration Scripts

### From SQLite to PostgreSQL

```python
# migration_sqlite_to_postgresql.py
import sqlite3
import psycopg2
import json

def migrate_data():
    # Connect to SQLite
    sqlite_conn = sqlite3.connect('instance/shop_monitor.db')
    sqlite_conn.row_factory = sqlite3.Row
    
    # Connect to PostgreSQL
    pg_conn = psycopg2.connect(
        host="localhost",
        database="shop_monitor",
        user="shop_monitor_user",
        password="your_password"
    )
    
    tables_to_migrate = [
        'roles', 'users', 'areas', 'zones', 'machines', 
        'machine_sessions', 'nodes', 'accessory_io'
    ]
    
    for table in tables_to_migrate:
        print(f"Migrating {table}...")
        
        # Read from SQLite
        cursor = sqlite_conn.execute(f"SELECT * FROM {table}")
        rows = cursor.fetchall()
        
        if rows:
            # Insert into PostgreSQL
            columns = list(rows[0].keys())
            placeholders = ','.join(['%s'] * len(columns))
            
            pg_cursor = pg_conn.cursor()
            insert_sql = f"INSERT INTO {table} ({','.join(columns)}) VALUES ({placeholders})"
            
            for row in rows:
                pg_cursor.execute(insert_sql, tuple(row))
            
            pg_conn.commit()
            print(f"Migrated {len(rows)} rows from {table}")
    
    sqlite_conn.close()
    pg_conn.close()

if __name__ == "__main__":
    migrate_data()
```

### Schema Version Management

```sql
-- Create schema version table
CREATE TABLE schema_versions (
    id SERIAL PRIMARY KEY,
    version VARCHAR(20) NOT NULL,
    description TEXT,
    applied_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    applied_by VARCHAR(64) DEFAULT CURRENT_USER
);

-- Insert current version
INSERT INTO schema_versions (version, description) 
VALUES ('1.0.1', 'Initial PostgreSQL schema for ShopMonitor');
```

## Performance Tuning

### PostgreSQL Configuration

```ini
# postgresql.conf settings for ShopMonitor

# Memory settings
shared_buffers = 256MB                  # 25% of total RAM
effective_cache_size = 1GB              # 75% of total RAM
work_mem = 4MB                          # For sorting operations
maintenance_work_mem = 64MB             # For maintenance operations

# Checkpoint settings
checkpoint_completion_target = 0.9
wal_buffers = 16MB
checkpoint_timeout = 10min

# Connection settings
max_connections = 200
shared_preload_libraries = 'pg_stat_statements'

# Query optimization
default_statistics_target = 100
random_page_cost = 1.1                  # For SSD storage
effective_io_concurrency = 200          # For SSD storage

# Logging
log_min_duration_statement = 1000       # Log queries > 1 second
log_checkpoints = on
log_connections = on
log_disconnections = on
log_lock_waits = on
```

### Query Optimization

```sql
-- Create function-based index for case-insensitive searches
CREATE INDEX idx_users_username_lower ON users(lower(username));

-- Create expression index for active machine sessions
CREATE INDEX idx_active_machine_sessions ON machine_sessions(machine_id) 
WHERE active = TRUE AND end_time IS NULL;

-- Create index for time-based queries
CREATE INDEX idx_machine_logs_recent ON machine_logs(timestamp DESC, machine_id);

-- Materialized view for dashboard statistics
CREATE MATERIALIZED VIEW dashboard_stats AS
SELECT 
    COUNT(*) FILTER (WHERE active = TRUE) as active_machines,
    COUNT(DISTINCT zone_id) as zones_with_machines,
    COUNT(*) FILTER (WHERE last_seen > CURRENT_TIMESTAMP - INTERVAL '5 minutes') as online_nodes
FROM machines m
LEFT JOIN nodes n ON m.node_id = n.id;

CREATE INDEX idx_dashboard_stats_refresh ON dashboard_stats(active_machines);

-- Refresh materialized view (run every 5 minutes)
-- REFRESH MATERIALIZED VIEW dashboard_stats;
```

### Connection Pooling Configuration

```python
# PostgreSQL connection pooling with SQLAlchemy
SQLALCHEMY_ENGINE_OPTIONS = {
    'pool_size': 20,
    'max_overflow': 30,
    'pool_timeout': 30,
    'pool_recycle': 3600,
    'pool_pre_ping': True,
    'pool_reset_on_return': 'commit',
    'echo': False,  # Set to True for debugging
    'echo_pool': False,
    'connect_args': {
        'connect_timeout': 10,
        'options': '-c timezone=UTC'
    }
}
```

## Monitoring and Alerting

### Database Health Queries

```sql
-- Check database size
SELECT pg_size_pretty(pg_database_size('shop_monitor')) as database_size;

-- Check table sizes
SELECT 
    schemaname,
    tablename,
    pg_size_pretty(pg_total_relation_size(schemaname||'.'||tablename)) as total_size,
    pg_size_pretty(pg_relation_size(schemaname||'.'||tablename)) as table_size,
    pg_size_pretty(pg_indexes_size(schemaname||'.'||tablename)) as indexes_size
FROM pg_tables 
WHERE schemaname = 'public'
ORDER BY pg_total_relation_size(schemaname||'.'||tablename) DESC;

-- Check active connections
SELECT 
    datname,
    usename,
    application_name,
    client_addr,
    state,
    query_start,
    state_change
FROM pg_stat_activity 
WHERE datname = 'shop_monitor';

-- Check slow queries
SELECT 
    query,
    calls,
    total_time,
    mean_time,
    rows
FROM pg_stat_statements 
WHERE query LIKE '%shop_monitor%'
ORDER BY total_time DESC 
LIMIT 10;
```

### Automated Monitoring Script

```bash
#!/bin/bash
# monitor_shop_monitor_db.sh

DB_NAME="shop_monitor"
DB_USER="shop_monitor_readonly"
LOG_FILE="/var/log/shop_monitor_db_monitor.log"

# Function to log with timestamp
log_with_timestamp() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" >> $LOG_FILE
}

# Check database connectivity
if ! psql -U $DB_USER -d $DB_NAME -c "SELECT 1;" >/dev/null 2>&1; then
    log_with_timestamp "ERROR: Cannot connect to database"
    exit 1
fi

# Check active sessions
ACTIVE_SESSIONS=$(psql -U $DB_USER -d $DB_NAME -t -c "SELECT COUNT(*) FROM machine_sessions WHERE active = TRUE;")
log_with_timestamp "Active sessions: $ACTIVE_SESSIONS"

# Check recent errors
RECENT_ERRORS=$(psql -U $DB_USER -d $DB_NAME -t -c "SELECT COUNT(*) FROM error_logs WHERE timestamp > CURRENT_TIMESTAMP - INTERVAL '1 hour' AND is_resolved = FALSE;")
if [ $RECENT_ERRORS -gt 10 ]; then
    log_with_timestamp "WARNING: High number of unresolved errors in last hour: $RECENT_ERRORS"
fi

# Check node connectivity
OFFLINE_NODES=$(psql -U $DB_USER -d $DB_NAME -t -c "SELECT COUNT(*) FROM nodes WHERE last_seen < CURRENT_TIMESTAMP - INTERVAL '10 minutes';")
if [ $OFFLINE_NODES -gt 0 ]; then
    log_with_timestamp "WARNING: $OFFLINE_NODES nodes appear to be offline"
fi

log_with_timestamp "Database monitoring completed successfully"
```

This comprehensive PostgreSQL database information file provides everything needed to set up, maintain, and optimize the ShopMonitor database system. It includes complete DDL scripts, performance tuning guidelines, backup procedures, and monitoring tools specifically designed for PostgreSQL deployment.
