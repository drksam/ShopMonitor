# Shop Monitor (Shop) v1.0.1
## Part of ShopSuite v1.0.1

## Overview

The Shop Monitor is a comprehensive system for tracking and managing machine usage in industrial environments. It provides real-time monitoring of machines, tracks user access via RFID, and integrates with shop management systems.

![Shop Monitor Dashboard](static/img/logo/generated-icon.png)

## Features

### Core System

- **Machine Activity Tracking**: Monitors machine usage in real-time
- **RFID Access Control**: Controls machine access using RFID cards
- **Hierarchical Area Management**: Organizes machines by areas, locations, and zones
- **Multi-User Operation**: Supports multiple users on a single machine
- **Lead Operator System**: Designates lead operators for areas and machines
- **Emergency Stop System**: Integrated E-STOP management
- **Inspection System**: Quality control inspection tracking

### Security Features

- **Enhanced API Security**: Token-based authentication with scope control, encryption, and token persistence
- **Brute Force Protection**: Intelligent IP-based lockout with exponential backoff and progressive timing
- **Input Validation & Sanitization**: Comprehensive XSS protection with recursive sanitization for nested structures
- **CSRF Protection**: Token validation for state-changing operations with origin validation
- **Security Headers**: Comprehensive security header implementation including CSP, X-Content-Type-Options, X-Frame-Options
- **Audit Logging**: Detailed security event tracking with microsecond precision and forensic analysis capabilities
- **Database-level Protections**: Parameterized queries, column-level encryption, and row-level security

### Error Handling

- **Centralized Error Management**: Global error handlers with @handle_api_errors decorator for consistent responses
- **Error Code System**: Structured application-specific error codes (1xxx-5xxx) with categories
- **API Error Standardization**: Consistent JSON error responses with traceability and reference IDs
- **Resiliency Patterns**: Circuit breakers for external services, exponential backoff retries, and graceful degradation
- **Comprehensive Logging**: Non-volatile storage with daily rotation, microsecond precision, and severity classification
- **Structured Logging**: JSON format with rich contextual information and sanitized request data

### Integration

- **ShopTracker Integration**: Seamless data sharing with ShopTracker application
- **Cross-App Authentication**: Single sign-on across applications
- **External API Support**: Well-documented API for third-party integration
- **Webhook System**: Real-time event notifications

### Hardware Support

- **ESP32 Nodes**: WiFi-connected monitoring nodes
- **ESP32-CYD Support**: Touch-screen enabled location and machine nodes
- **Multiple RFID Readers**: Support for up to 4 readers per node
- **Accessory I/O**: Control relays and monitor inputs

## Architecture

The application follows a modern architecture:

- **Backend**: Flask (Python) with SQLAlchemy ORM
- **Frontend**: Bootstrap, Chart.js, Alpine.js
- **Database**: SQLite (development) or PostgreSQL (production)
- **Hardware**: ESP32 microcontrollers with RFID readers
- **API**: RESTful API with token authentication

## Documentation

- [API Documentation](API_DOCUMENTATION.md)
- [Database Schema](database.md)
- [Installation Guide](INSTALLATION.md)
- Arduino Sketches:
  - [Machine Monitor](arduino/machine_monitor)
  - [Office Reader](arduino/office_reader)
  - [ESP32 Monitor](arduino/esp32_machine_monitor)

## Security Practices

This application implements industry best practices for security:

1. **Authentication & Authorization**
   - Token-based authentication for API access
   - Role-based access control
   - Permission-based authorization
   - Session management with secure cookies
   - IP-based throttling and lockouts

2. **Data Protection**
   - Input validation and sanitization
   - Parameterized queries to prevent SQL injection
   - CSRF protection for all state-changing requests
   - XSS protection through output encoding
   - Content Security Policy implementation

3. **Infrastructure Security**
   - Security headers configuration
   - HTTPS enforcement
   - Rate limiting and request throttling
   - Database connection security
   - Secure error handling

4. **Audit & Compliance**
   - Comprehensive logging of security events
   - User action audit trails
   - Access attempt monitoring
   - Security health checks
   - API token lifecycle management

## Error Handling Philosophy

Our application uses a multi-layered approach to error management:

1. **User-Facing Errors**
   - Clear, actionable error messages
   - Appropriate HTTP status codes
   - Non-technical explanations
   - Support reference codes

2. **Developer Errors**
   - Detailed logging with context
   - Stack traces in development
   - Consistent error format
   - Centralized error handling

3. **System Resilience**
   - Graceful degradation
   - Circuit breaker patterns
   - Auto-recovery mechanisms
   - Fault isolation

4. **Monitoring & Alerting**
   - Error rate monitoring
   - Critical error alerting
   - Performance degradation detection
   - Security incident alerting

## Getting Started

See the [Installation Guide](INSTALLATION.md) for detailed setup instructions.

### Quick Start

1. Clone this repository
2. Create a virtual environment
3. Install dependencies: `pip install -r requirements.txt`
4. Initialize database: `flask db upgrade`
5. Run development server: `flask run`

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

