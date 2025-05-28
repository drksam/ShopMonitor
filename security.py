import os
import secrets
import functools
import logging
from datetime import datetime, timedelta
import json
import re
import jwt
import hashlib
import base64
from cryptography.fernet import Fernet
from flask import request, jsonify, current_app
from werkzeug.security import check_password_hash, generate_password_hash
from error_handling import logger

# Configure logging
logger = logging.getLogger(__name__)

# API Token storage
api_tokens = {}

# Failed authentication tracking for brute force protection
failed_attempts = {}
MAX_FAILED_ATTEMPTS = 5  # Max number of failed attempts before lockout
LOCKOUT_DURATION = 300   # Lockout duration in seconds (5 minutes)

# Encryption key for token storage
# In production, this should be stored securely (env variable or secret manager)
def get_encryption_key():
    """Get or generate an encryption key for token storage"""
    key_path = os.path.join('instance', 'token_key.key')
    if os.path.exists(key_path):
        with open(key_path, 'rb') as f:
            return f.read()
    else:
        # Generate a new key
        key = Fernet.generate_key()
        os.makedirs('instance', exist_ok=True)
        with open(key_path, 'wb') as f:
            f.write(key)
        return key

# Initialize encryption
ENCRYPTION_KEY = get_encryption_key()
cipher_suite = Fernet(ENCRYPTION_KEY)

def generate_api_token(machine_id=None, expires_in_days=30, scopes=None):
    """
    Generate a secure API token for machine-to-server communication
    
    Args:
        machine_id (str): Optional machine identifier to associate with token
        expires_in_days (int): Number of days until token expires
        scopes (list): List of permission scopes for this token
    
    Returns:
        str: The generated API token
    """
    token_id = secrets.token_urlsafe(32)
    expires = datetime.utcnow() + timedelta(days=expires_in_days)
    
    # Default scopes if none provided
    if not scopes:
        scopes = ["read:basic"]
    
    # Generate JWT token with additional security features
    token_payload = {
        'sub': machine_id or 'api-client',
        'jti': token_id,
        'iat': datetime.utcnow(),
        'exp': expires,
        'scopes': scopes
    }
    
    # Get the secret key from config or use a secure default
    secret_key = current_app.config.get('JWT_SECRET_KEY') if 'current_app' in globals() and current_app else \
                 os.environ.get('JWT_SECRET_KEY', secrets.token_hex(32))
    
    token = jwt.encode(token_payload, secret_key, algorithm='HS256')
    
    # Store token metadata (but not the token itself) for validation
    api_tokens[token_id] = {
        'machine_id': machine_id,
        'expires': expires,
        'created': datetime.utcnow(),
        'last_used': None,
        'scopes': scopes,
        'revoked': False
    }
    
    # Persist tokens to file for restarts
    try:
        _persist_tokens()
    except Exception as e:
        logger.error(f"Failed to persist API tokens: {e}")
    
    return token

def validate_api_token(token):
    """
    Validate an API token
    
    Args:
        token (str): The token to validate
    
    Returns:
        dict: Token data if valid, None otherwise
    """
    try:
        # Get the secret key from config or use a secure default
        secret_key = current_app.config.get('JWT_SECRET_KEY') if 'current_app' in globals() and current_app else \
                     os.environ.get('JWT_SECRET_KEY', secrets.token_hex(32))
        
        # Decode and validate the JWT
        payload = jwt.decode(token, secret_key, algorithms=['HS256'])
        
        # Extract token ID
        token_id = payload.get('jti')
        
        if not token_id or token_id not in api_tokens:
            return None
        
        token_data = api_tokens[token_id]
        
        # Check if token has been revoked
        if token_data.get('revoked', False):
            return None
        
        # Check if token has expired
        if datetime.utcnow() > token_data['expires']:
            # Clean up expired token
            del api_tokens[token_id]
            _persist_tokens()
            return None
        
        # Update last used timestamp
        token_data['last_used'] = datetime.utcnow()
        _persist_tokens()
        
        return token_data
    
    except jwt.ExpiredSignatureError:
        logger.warning("Expired token attempted to be used")
        return None
    except jwt.InvalidTokenError as e:
        logger.warning(f"Invalid token: {e}")
        return None
    except Exception as e:
        logger.error(f"Token validation error: {e}")
        return None

def revoke_api_token(token):
    """
    Revoke an API token
    
    Args:
        token (str): The token to revoke
    
    Returns:
        bool: True if token was revoked, False otherwise
    """
    try:
        # Get the secret key from config or use a secure default
        secret_key = current_app.config.get('JWT_SECRET_KEY') if 'current_app' in globals() and current_app else \
                     os.environ.get('JWT_SECRET_KEY', secrets.token_hex(32))
        
        payload = jwt.decode(token, secret_key, algorithms=['HS256'])
        token_id = payload.get('jti')
        
        if token_id in api_tokens:
            api_tokens[token_id]['revoked'] = True
            _persist_tokens()
            return True
        
        return False
    except:
        return False

def has_scope(token_data, required_scope):
    """
    Check if token has a required scope
    
    Args:
        token_data (dict): The token data
        required_scope (str): The required scope
    
    Returns:
        bool: True if token has the required scope, False otherwise
    """
    if not token_data or 'scopes' not in token_data:
        return False
        
    return required_scope in token_data['scopes']

def require_api_token(f=None, required_scopes=None):
    """
    Decorator to require a valid API token for access to a route
    
    Args:
        f (function, optional): The function to decorate
        required_scopes (list, optional): List of required scopes
    """
    def decorator(f):
        @functools.wraps(f)
        def decorated_function(*args, **kwargs):
            # Get client IP for brute force protection
            client_ip = request.remote_addr
            
            # Check if IP is locked out
            if _is_ip_locked_out(client_ip):
                logger.warning(f"Locked out IP attempt: {client_ip}")
                return jsonify({'error': 'Too many failed attempts. Try again later.'}), 429
            
            # Legacy system support - first check for API key in header
            api_key = request.headers.get('X-API-Key')
            shop_api_key = current_app.config.get('SHOP_API_KEY') or \
                          current_app.config.get('SHOP_SUITE_API_KEY') or \
                          os.environ.get('API_KEY')
                          
            if api_key and shop_api_key and api_key == shop_api_key:
                # Legacy API key authentication successful
                return f(*args, **kwargs)
            
            # Modern token-based auth
            # Check for token in Authorization header or as a query parameter
            auth_header = request.headers.get('Authorization', '')
            
            if auth_header.startswith('Bearer '):
                token = auth_header.replace('Bearer ', '')
            else:
                token = request.args.get('token')
                
            token_data = validate_api_token(token)
            if not token_data:
                # Record failed attempt
                _record_failed_attempt(client_ip)
                logger.warning(f"Invalid API token attempt from {client_ip}")
                return jsonify({'error': 'Invalid or expired API token'}), 401
            
            # Check scopes if required
            if required_scopes:
                # Check if token has all required scopes
                if not all(has_scope(token_data, scope) for scope in required_scopes):
                    logger.warning(f"Insufficient scope for token from {client_ip}")
                    return jsonify({'error': 'Insufficient permissions'}), 403
                
            # Authentication successful, reset failed attempts
            _reset_failed_attempts(client_ip)
            
            # Store token data in request context for use in the endpoint
            request.token_data = token_data
            
            return f(*args, **kwargs)
        return decorated_function
    
    # Handle both @require_api_token and @require_api_token(scopes=['read:stuff'])
    if f:
        return decorator(f)
    return decorator

def sanitize_input(data, allowed_chars=None):
    """
    Sanitize input to prevent injection attacks
    
    Args:
        data (str): The input data to sanitize
        allowed_chars (str): Optional string of allowed characters
        
    Returns:
        str: Sanitized input
    """
    if not data:
        return data

    if isinstance(data, dict):
        return {k: sanitize_input(v, allowed_chars) for k, v in data.items()}
    elif isinstance(data, list):
        return [sanitize_input(i, allowed_chars) for i in data]
    elif isinstance(data, str):
        # Remove potentially harmful patterns
        # Remove script tags and other dangerous patterns
        data = re.sub(r'<script.*?>.*?</script>', '', data, flags=re.IGNORECASE | re.DOTALL)
        data = re.sub(r'javascript:', '', data, flags=re.IGNORECASE)
        data = re.sub(r'on\w+\s*=', '', data, flags=re.IGNORECASE)
        
        # If specific allowed characters are provided, filter to only those
        if allowed_chars is not None:
            data = ''.join(c for c in data if c in allowed_chars)
        
        return data
    
    # Return non-string data unchanged
    return data

def init_app(app):
    """
    Initialize security features for the Flask app
    
    Args:
        app: Flask application instance
    """
    # Load tokens from persistent storage
    _load_tokens()
    
    # Cleanup expired tokens periodically
    @app.before_request
    def cleanup_tokens():
        # Only run occasionally to avoid performance impact
        if secrets.randbelow(100) < 5:  # 5% chance on each request
            now = datetime.utcnow()
            expired_tokens = [token for token, data in api_tokens.items() 
                              if now > data['expires']]
            
            for token in expired_tokens:
                del api_tokens[token]
                
            if expired_tokens:
                _persist_tokens()
                logger.info(f"Cleaned up {len(expired_tokens)} expired tokens")

    # Add security headers to all responses
    @app.after_request
    def add_security_headers(response):
        response.headers['Content-Security-Policy'] = "default-src 'self'; script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net"
        response.headers['X-Content-Type-Options'] = 'nosniff'
        response.headers['X-Frame-Options'] = 'SAMEORIGIN'
        response.headers['X-XSS-Protection'] = '1; mode=block'
        return response

def _persist_tokens():
    """Persist tokens to file for restarts"""
    token_store = {}
    for token, data in api_tokens.items():
        # Convert datetime objects to ISO format strings
        token_store[token] = {
            'machine_id': data['machine_id'],
            'expires': data['expires'].isoformat(),
            'created': data['created'].isoformat(),
            'last_used': data['last_used'].isoformat() if data['last_used'] else None,
            'scopes': data['scopes'],
            'revoked': data['revoked']
        }
    
    # Save to a file in the instance directory
    os.makedirs('instance', exist_ok=True)
    with open('instance/api_tokens.json', 'w') as f:
        json.dump(token_store, f)

def _load_tokens():
    """Load tokens from persistent storage"""
    try:
        if os.path.exists('instance/api_tokens.json'):
            with open('instance/api_tokens.json', 'r') as f:
                token_store = json.load(f)
            
            # Convert ISO format strings back to datetime objects
            for token, data in token_store.items():
                api_tokens[token] = {
                    'machine_id': data['machine_id'],
                    'expires': datetime.fromisoformat(data['expires']),
                    'created': datetime.fromisoformat(data['created']),
                    'last_used': datetime.fromisoformat(data['last_used']) if data['last_used'] else None,
                    'scopes': data['scopes'],
                    'revoked': data['revoked']
                }
            logger.info(f"Loaded {len(api_tokens)} API tokens from storage")
    except Exception as e:
        logger.error(f"Failed to load API tokens: {e}")

def _record_failed_attempt(ip):
    """Record a failed authentication attempt"""
    now = datetime.utcnow()
    
    if ip not in failed_attempts:
        failed_attempts[ip] = {
            'count': 1,
            'first_attempt': now,
            'last_attempt': now,
            'locked_until': None
        }
    else:
        data = failed_attempts[ip]
        data['count'] += 1
        data['last_attempt'] = now
        
        # Check if we need to lock out this IP
        if data['count'] >= MAX_FAILED_ATTEMPTS:
            data['locked_until'] = now + timedelta(seconds=LOCKOUT_DURATION)
            logger.warning(f"IP {ip} locked out due to {data['count']} failed attempts")

def _reset_failed_attempts(ip):
    """Reset failed attempts after successful authentication"""
    if ip in failed_attempts:
        del failed_attempts[ip]

def _is_ip_locked_out(ip):
    """Check if an IP is currently locked out"""
    if ip not in failed_attempts:
        return False
    
    data = failed_attempts[ip]
    if data['locked_until'] is None:
        return False
    
    # Check if lockout has expired
    if datetime.utcnow() > data['locked_until']:
        del failed_attempts[ip]  # Reset after lockout expires
        return False
        
    return True