"""
Error Handling and Logging Module for Shop System
Provides centralized error handling, logging, and retry mechanisms
"""
import functools
import logging
import time
import os
import json
import platform
import traceback
import queue
from datetime import datetime
from flask import jsonify, request, current_app

# Setup logging to file
log_dir = "logs"
if not os.path.exists(log_dir):
    os.makedirs(log_dir)

log_file = os.path.join(log_dir, f"Shop_{datetime.now().strftime('%Y-%m-%d')}.log")
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(log_file),
        logging.StreamHandler()
    ]
)

# Create a logger instance
logger = logging.getLogger('Shop')

# Queue for storing errors when offline
error_queue = queue.Queue()
MAX_QUEUED_ERRORS = 1000

# Define error codes
ERROR_CODES = {
    # Network and connectivity errors (1000-1999)
    1000: "Network connection error",
    1001: "Server unreachable",
    1002: "Timeout error",
    1003: "DNS resolution error",
    1004: "Disconnected from WiFi",
    1005: "Connection reset",
    1006: "Socket error",
    1007: "API rate limit exceeded",
    
    # Authentication and permission errors (2000-2999)
    2000: "Authentication required",
    2001: "Invalid API token",
    2002: "Token expired",
    2003: "Permission denied",
    2004: "Invalid credentials",
    2005: "Account locked",
    
    # Database errors (3000-3999)
    3000: "Database error",
    3001: "Record not found",
    3002: "Duplicate record",
    3003: "Transaction failed",
    3004: "Database connection lost",
    3005: "Database constraint violation",
    
    # Hardware errors (4000-4999)
    4000: "Hardware error",
    4001: "RFID reader error",
    4002: "Relay control error",
    4003: "Sensor error",
    4004: "Power interruption",
    4005: "Device overheating",
    4006: "I/O error",
    4007: "Memory error",
    4008: "EEPROM write error",
    4009: "EEPROM read error",
    
    # API and integration errors (5000-5999)
    5000: "API error",
    5001: "Invalid input",
    5002: "Resource already exists",
    5003: "Resource not found",
    5004: "Invalid request format",
    5005: "API version mismatch",
    5006: "Request too large",
}

class AppError(Exception):
    """Custom application error class for structured error handling"""
    def __init__(self, code=5000, message=None, details=None):
        self.code = code
        self.message = message or ERROR_CODES.get(code, "Unknown error")
        self.details = details
        super().__init__(self.message)
        
    def to_dict(self):
        """Convert error to dictionary for JSON response"""
        error_dict = {
            'success': False,
            'error_code': self.code,
            'error_message': self.message,
            'timestamp': datetime.now().isoformat()
        }
        if self.details:
            error_dict['error_details'] = self.details
        return error_dict

def handle_api_errors(f):
    """Decorator to handle API errors consistently"""
    @functools.wraps(f)
    def decorated_function(*args, **kwargs):
        try:
            return f(*args, **kwargs)
        except AppError as e:
            logger.error(f"AppError: {e.code} - {e.message} - {request.path}")
            return jsonify(e.to_dict()), get_status_code_for_error(e.code)
        except Exception as e:
            logger.error(f"Uncaught error in {f.__name__}: {str(e)}")
            # Log full stack trace for debugging
            logger.error(traceback.format_exc())
            app_error = AppError(code=5000, message=f"Internal server error: {str(e)}")
            return jsonify(app_error.to_dict()), 500
    return decorated_function

def exponential_backoff(max_retries=5, base_delay=0.5):
    """
    Decorator for implementing exponential backoff on failed operations
    
    Args:
        max_retries: Maximum number of retry attempts
        base_delay: Base delay time in seconds
    """
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            last_exception = None
            jitter_factor = 0.1  # Add some randomness to prevent thundering herd
            
            for attempt in range(max_retries):
                try:
                    return func(*args, **kwargs)
                except Exception as e:
                    last_exception = e
                    # Check if it's a network-related error
                    is_network_error = isinstance(e, (ConnectionError, TimeoutError)) or \
                                      "connection" in str(e).lower() or \
                                      "timeout" in str(e).lower() or \
                                      "network" in str(e).lower()
                    
                    # Calculate wait time with jitter
                    wait_time = base_delay * (2 ** attempt)
                    jitter = wait_time * jitter_factor * (2 * (0.5 - random.random()))
                    wait_time = max(0.1, wait_time + jitter)  # Ensure minimum wait time
                    
                    logger.warning(f"Retry {attempt+1}/{max_retries} for {func.__name__} in {wait_time:.2f}s: {str(e)}")
                    
                    # For network errors, log to persistent storage
                    if is_network_error:
                        log_network_error(func.__name__, str(e), attempt+1)
                    
                    time.sleep(wait_time)
            
            # If we get here, all retries failed
            logger.error(f"All {max_retries} retries failed for {func.__name__}: {str(last_exception)}")
            
            # Queue the error for later processing if it's likely a network error
            if isinstance(last_exception, (ConnectionError, TimeoutError)) or \
               "connection" in str(last_exception).lower() or \
               "timeout" in str(last_exception).lower():
                queue_failed_operation(func.__name__, args, kwargs, str(last_exception))
                
            raise last_exception
        return wrapper
    return decorator

def get_status_code_for_error(error_code):
    """
    Map internal error codes to HTTP status codes
    
    Args:
        error_code: Internal application error code
    
    Returns:
        HTTP status code
    """
    # Map error code ranges to HTTP status codes
    if 1000 <= error_code < 2000:
        return 502  # Bad Gateway for network errors
    elif 2000 <= error_code < 3000:
        return 403  # Forbidden for auth/permission errors
    elif 3000 <= error_code < 4000:
        if error_code == 3001:
            return 404  # Not Found
        return 500  # Internal Server Error for database errors
    elif 4000 <= error_code < 5000:
        return 503  # Service Unavailable for hardware errors
    elif 5000 <= error_code < 6000:
        if error_code in [5001, 5002]:
            return 400  # Bad Request for invalid inputs
        elif error_code == 5003:
            return 404  # Not Found
        return 400  # Default to Bad Request for API errors
    else:
        return 500  # Internal Server Error default

def sanitize_input(data):
    """
    Sanitize input data to prevent injection attacks
    
    Args:
        data: Input data which could be a string, dict, or list
    
    Returns:
        Sanitized data
    """
    if isinstance(data, str):
        # Basic sanitization for strings
        # Remove potentially dangerous characters or patterns
        return data.replace('<script>', '').replace('</script>', '')
    elif isinstance(data, dict):
        # Recursively sanitize each value in a dictionary
        return {k: sanitize_input(v) for k, v in data.items()}
    elif isinstance(data, list):
        # Recursively sanitize each item in a list
        return [sanitize_input(item) for item in data]
    else:
        # Leave other types unchanged
        return data

def log_network_error(func_name, error_msg, attempt):
    """
    Log network errors to persistent storage
    
    Args:
        func_name: The name of the function that encountered the error
        error_msg: The error message
        attempt: The retry attempt number
    """
    try:
        error_log_file = os.path.join(log_dir, "network_errors.log")
        timestamp = datetime.now().isoformat()
        with open(error_log_file, "a") as f:
            f.write(f"{timestamp} - {func_name} - Attempt {attempt} - {error_msg}\n")
    except Exception as e:
        logger.error(f"Failed to log network error to file: {str(e)}")

def queue_failed_operation(func_name, args, kwargs, error_msg):
    """
    Queue a failed operation for later retry
    
    Args:
        func_name: Name of the function
        args: Function arguments
        kwargs: Function keyword arguments
        error_msg: Error message from the exception
    """
    if error_queue.qsize() < MAX_QUEUED_ERRORS:
        error_queue.put({
            'timestamp': datetime.now().isoformat(),
            'function': func_name,
            'args': str(args),
            'kwargs': str(kwargs),
            'error': error_msg
        })
        logger.info(f"Queued operation {func_name} for later retry")
    else:
        logger.warning("Error queue full, discarding oldest error")
        # Remove oldest item and add new one
        try:
            error_queue.get_nowait()
            error_queue.put({
                'timestamp': datetime.now().isoformat(),
                'function': func_name,
                'args': str(args),
                'kwargs': str(kwargs),
                'error': error_msg
            })
        except Exception:
            pass

def log_system_info():
    """Log system information for diagnostic purposes"""
    try:
        system_info = {
            'os': platform.system(),
            'os_version': platform.version(),
            'architecture': platform.machine(),
            'python_version': platform.python_version(),
            'node': platform.node(),
            'timestamp': datetime.now().isoformat()
        }
        
        system_log_file = os.path.join(log_dir, "system_info.log")
        with open(system_log_file, "w") as f:
            json.dump(system_info, f, indent=2)
        
        logger.info("System information logged")
    except Exception as e:
        logger.error(f"Failed to log system information: {str(e)}")

def init_app(app):
    """
    Initialize error handling for the Flask app
    
    Args:
        app: Flask application instance
    """
    # Import needed for timeout handling
    import random
    
    # Log system information on startup
    log_system_info()
    
    @app.errorhandler(404)
    def not_found(error):
        logger.warning(f"404 error: {request.path}")
        return jsonify({
            'success': False,
            'error_code': 5003,
            'error_message': 'Resource not found',
            'path': request.path
        }), 404
    
    @app.errorhandler(500)
    def server_error(error):
        logger.error(f"500 error: {str(error)}")
        logger.error(traceback.format_exc())
        return jsonify({
            'success': False,
            'error_code': 5000,
            'error_message': 'Internal server error',
            'error_details': str(error) if app.debug else None
        }), 500
    
    @app.errorhandler(405)
    def method_not_allowed(error):
        logger.warning(f"405 error: {request.method} {request.path}")
        return jsonify({
            'success': False,
            'error_code': 5004,
            'error_message': 'Method not allowed',
            'method': request.method,
            'path': request.path
        }), 405
    
    @app.errorhandler(429)
    def too_many_requests(error):
        logger.warning(f"429 error: {request.path}")
        return jsonify({
            'success': False,
            'error_code': 1007,
            'error_message': 'Too many requests, please try again later',
        }), 429
        
    # Register a timeout handler for long-running requests
    @app.before_request
    def timeout_handler():
        request.timeout_start = time.time()
        
    @app.after_request
    def log_response_time(response):
        if hasattr(request, 'timeout_start'):
            duration = time.time() - request.timeout_start
            if duration > 1.0:  # Log slow requests (more than 1 second)
                logger.warning(f"Slow request: {request.method} {request.path} took {duration:.2f}s")
        return response
        
    # Return the app for chaining
    return app

def get_network_errors():
    """
    Get queued network errors
    
    Returns:
        List of queued errors
    """
    errors = []
    while not error_queue.empty():
        try:
            errors.append(error_queue.get_nowait())
        except queue.Empty:
            break
    return errors

def save_queued_errors():
    """Save queued errors to disk for persistence across restarts"""
    try:
        errors = get_network_errors()
        if errors:
            error_queue_file = os.path.join(log_dir, "error_queue.json")
            with open(error_queue_file, "w") as f:
                json.dump(errors, f)
            logger.info(f"Saved {len(errors)} queued errors to disk")
    except Exception as e:
        logger.error(f"Failed to save queued errors: {str(e)}")

def load_queued_errors():
    """Load queued errors from disk"""
    try:
        error_queue_file = os.path.join(log_dir, "error_queue.json")
        if os.path.exists(error_queue_file):
            with open(error_queue_file, "r") as f:
                errors = json.load(f)
            
            # Add errors back to the queue
            for error in errors:
                if error_queue.qsize() < MAX_QUEUED_ERRORS:
                    error_queue.put(error)
            
            logger.info(f"Loaded {len(errors)} queued errors from disk")
            
            # Remove the file after loading
            os.remove(error_queue_file)
    except Exception as e:
        logger.error(f"Failed to load queued errors: {str(e)}")

# Hardware-specific error handling for ESP32/Arduino
class HardwareError(AppError):
    """Custom error class for hardware-specific issues"""
    def __init__(self, code=4000, message=None, details=None, device_id=None):
        super().__init__(code, message, details)
        self.device_id = device_id
        
    def to_dict(self):
        error_dict = super().to_dict()
        if self.device_id:
            error_dict['device_id'] = self.device_id
        return error_dict

def log_hardware_error(device_id, error_type, error_msg):
    """
    Log hardware errors to persistent storage
    
    Args:
        device_id: The ID of the device
        error_type: Type of error (RFID, relay, sensor, etc.)
        error_msg: The error message
    """
    try:
        hardware_log_file = os.path.join(log_dir, "hardware_errors.log")
        timestamp = datetime.now().isoformat()
        with open(hardware_log_file, "a") as f:
            f.write(f"{timestamp} - Device: {device_id} - Type: {error_type} - {error_msg}\n")
            
        # Also log to the main logger
        logger.error(f"Hardware error: {device_id} - {error_type} - {error_msg}")
        
    except Exception as e:
        logger.error(f"Failed to log hardware error to file: {str(e)}")