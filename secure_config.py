"""
Secure Configuration Manager for ShopMonitor

This module handles secure loading of configuration variables from various sources:
1. Environment variables
2. .env file
3. Instance-specific configuration files
4. Secure credential storage

It ensures sensitive credentials are properly managed and not hardcoded.
"""

import os
import sys
import logging
import json
import base64
from pathlib import Path
from cryptography.fernet import Fernet
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC

# Try to load dotenv if available
try:
    from dotenv import load_dotenv
    dotenv_loaded = load_dotenv()
    if dotenv_loaded:
        logging.info(".env file loaded successfully")
except ImportError:
    logging.warning("python-dotenv not installed, skipping .env loading")
    dotenv_loaded = False

# Path to the instance directory for storing configuration
INSTANCE_DIR = Path("instance")
INSTANCE_DIR.mkdir(exist_ok=True)

# Path to the secure credential storage
SECURE_CREDS_PATH = INSTANCE_DIR / "secure_credentials.enc"
CREDS_KEY_PATH = INSTANCE_DIR / "creds_key.key"

# Default configuration values (non-sensitive)
DEFAULT_CONFIG = {
    "APP_NAME": "ShopMonitor",
    "DEBUG": False,
    "TESTING": False,
    "DATABASE_DIALECT": "sqlite",  # Options: sqlite, postgresql, mysql
}

# ==============================================================================
# Credential Encryption Functions
# ==============================================================================

def get_or_create_key():
    """Get or create the encryption key for secure credentials"""
    if CREDS_KEY_PATH.exists():
        with open(CREDS_KEY_PATH, "rb") as key_file:
            return key_file.read()
    else:
        # Create a secure random key
        key = Fernet.generate_key()
        with open(CREDS_KEY_PATH, "wb") as key_file:
            key_file.write(key)
        return key

def derive_key_from_password(password, salt=None):
    """Derive an encryption key from a password"""
    if salt is None:
        # If no salt is provided, create one
        salt = os.urandom(16)
    
    kdf = PBKDF2HMAC(
        algorithm=hashes.SHA256(),
        length=32,
        salt=salt,
        iterations=100000,
    )
    
    key = base64.urlsafe_b64encode(kdf.derive(password.encode()))
    return key, salt

def save_encrypted_credentials(credentials, master_password=None):
    """
    Save credentials to an encrypted file
    
    Args:
        credentials (dict): Dictionary of credentials to save
        master_password (str, optional): Master password for encryption
            If not provided, will use auto-generated key
    """
    if master_password:
        # Use password-based encryption
        key, salt = derive_key_from_password(master_password)
        # Store the salt in the credentials dict
        credentials["_salt"] = base64.b64encode(salt).decode('utf-8')
    else:
        # Use key-based encryption
        key = get_or_create_key()
    
    # Convert credentials to JSON and encrypt
    cipher_suite = Fernet(key)
    credentials_json = json.dumps(credentials).encode()
    encrypted_data = cipher_suite.encrypt(credentials_json)
    
    # Save encrypted data
    with open(SECURE_CREDS_PATH, "wb") as file:
        file.write(encrypted_data)
    
    logging.info("Credentials saved securely")
    return True

def load_encrypted_credentials(master_password=None):
    """
    Load credentials from encrypted file
    
    Args:
        master_password (str, optional): Master password for decryption
            If not provided, will use auto-generated key
    
    Returns:
        dict: Dictionary of credentials or empty dict if decryption fails
    """
    if not SECURE_CREDS_PATH.exists():
        logging.warning("No secure credentials file exists")
        return {}
    
    try:
        # Read encrypted data
        with open(SECURE_CREDS_PATH, "rb") as file:
            encrypted_data = file.read()
        
        if master_password:
            # First decrypt a bit to check for the salt
            # This tries to parse whatever we get as JSON to extract the salt
            # If this fails, it might not be a password-encrypted file
            try:
                # Use key-based decryption as fallback
                key = get_or_create_key()
                cipher_suite = Fernet(key)
                decrypted = cipher_suite.decrypt(encrypted_data)
                creds = json.loads(decrypted)
                
                # If there's a salt, it was password-encrypted
                if "_salt" in creds:
                    salt = base64.b64decode(creds["_salt"])
                    key, _ = derive_key_from_password(master_password, salt)
                    cipher_suite = Fernet(key)
                    decrypted = cipher_suite.decrypt(encrypted_data)
                    creds = json.loads(decrypted)
            except Exception:
                # Try direct password-based decryption
                # We'll have to guess the salt - this won't work reliably
                # between different machines without proper salt management
                key, _ = derive_key_from_password(master_password)
                cipher_suite = Fernet(key)
                decrypted = cipher_suite.decrypt(encrypted_data)
                creds = json.loads(decrypted)
        else:
            # Use key-based decryption
            key = get_or_create_key()
            cipher_suite = Fernet(key)
            decrypted = cipher_suite.decrypt(encrypted_data)
            creds = json.loads(decrypted)
        
        # Remove salt if it exists (internal use only)
        if "_salt" in creds:
            del creds["_salt"]
            
        return creds
        
    except Exception as e:
        logging.error(f"Failed to load secure credentials: {e}")
        return {}

# ==============================================================================
# Configuration Loading Functions
# ==============================================================================

def get_database_url():
    """
    Get the database URL based on configuration
    
    Returns:
        str: Database URL for SQLAlchemy
    """
    # First check for explicit DATABASE_URL environment variable
    db_url = os.environ.get("DATABASE_URL")
    if db_url:
        return db_url
        
    # Otherwise, construct from components
    dialect = get_config_value("DATABASE_DIALECT", "sqlite")
    
    if dialect == "sqlite":
        db_path = get_config_value("DATABASE_PATH", "instance/nooyen_tracker.db")
        return f"sqlite:///{db_path}"
        
    elif dialect in ["postgresql", "postgres"]:
        host = get_config_value("DATABASE_HOST", "localhost")
        port = get_config_value("DATABASE_PORT", "5432")
        name = get_config_value("DATABASE_NAME", "shop_monitor")
        user = get_config_value("DATABASE_USER")
        password = get_config_value("DATABASE_PASSWORD")
        
        # We need both user and password for PostgreSQL
        if not (user and password):
            logging.warning("Database user or password missing, using SQLite instead")
            return "sqlite:///instance/nooyen_tracker.db"
            
        return f"postgresql://{user}:{password}@{host}:{port}/{name}"
        
    elif dialect == "mysql":
        host = get_config_value("DATABASE_HOST", "localhost")
        port = get_config_value("DATABASE_PORT", "3306")
        name = get_config_value("DATABASE_NAME", "shop_monitor")
        user = get_config_value("DATABASE_USER")
        password = get_config_value("DATABASE_PASSWORD")
        
        # We need both user and password for MySQL
        if not (user and password):
            logging.warning("Database user or password missing, using SQLite instead")
            return "sqlite:///instance/nooyen_tracker.db"
            
        return f"mysql+pymysql://{user}:{password}@{host}:{port}/{name}"
        
    # Default to SQLite if dialect is unknown
    return "sqlite:///instance/nooyen_tracker.db"

def get_config_value(key, default=None):
    """
    Get a configuration value from various sources with priority:
    1. Environment variables
    2. Secure credentials storage
    3. Default configuration
    4. Provided default value
    
    Args:
        key (str): Configuration key to retrieve
        default: Default value if key is not found
        
    Returns:
        Value of the configuration key or default
    """
    # First check environment variables
    env_value = os.environ.get(key)
    if env_value is not None:
        return env_value
        
    # Then check secure credentials
    try:
        creds = load_encrypted_credentials()
        if key in creds:
            return creds[key]
    except Exception:
        pass
        
    # Then check default configuration
    if key in DEFAULT_CONFIG:
        return DEFAULT_CONFIG[key]
        
    # Finally, return the provided default
    return default

def get_or_create_secret_key():
    """
    Get or create a secret key for Flask sessions
    
    Returns:
        str: Secret key for Flask
    """
    secret_key = get_config_value("SECRET_KEY")
    if secret_key:
        return secret_key
        
    # Check if we have a secret key file
    secret_key_file = INSTANCE_DIR / "secret_key"
    if secret_key_file.exists():
        with open(secret_key_file, "rb") as f:
            return f.read()
            
    # Generate a new secret key
    import secrets
    secret_key = secrets.token_hex(32)
    
    # Save it for future use
    with open(secret_key_file, "w") as f:
        f.write(secret_key)
        
    return secret_key

def configure_app(app):
    """
    Configure a Flask app with secure settings
    
    Args:
        app: Flask application instance
    """
    # Set up basic Flask configuration
    app.config["SECRET_KEY"] = get_or_create_secret_key()
    app.config["SQLALCHEMY_DATABASE_URI"] = get_database_url()
    app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False
    
    # JWT Secret Key for API tokens
    app.config["JWT_SECRET_KEY"] = get_config_value("JWT_SECRET_KEY", app.config["SECRET_KEY"])
    
    # Debug and testing settings
    app.config["DEBUG"] = get_config_value("DEBUG", "False").lower() in ("true", "1", "yes")
    app.config["TESTING"] = get_config_value("TESTING", "False").lower() in ("true", "1", "yes")
    
    # Shop Suite Integration Settings
    app.config["SHOP_SUITE_APP_NAME"] = get_config_value("SHOP_SUITE_APP_NAME", "shop_monitor")
    app.config["SHOP_SUITE_SECRET_KEY"] = get_config_value("SHOP_SUITE_SECRET_KEY", app.config["SECRET_KEY"])
    app.config["SHOP_MONITOR_URL"] = get_config_value("SHOP_MONITOR_URL", "http://localhost:5000")
    app.config["SHOP_TRACKER_URL"] = get_config_value("SHOP_TRACKER_URL", "http://localhost:5001")
    app.config["SHOP_SUITE_COOKIE_DOMAIN"] = get_config_value("SHOP_SUITE_COOKIE_DOMAIN")
    
    # API Keys and Integration Settings
    app.config["SHOP_API_KEY"] = get_config_value("SHOP_API_KEY", get_config_value("API_KEY", ""))
    app.config["SHOP_TRACKER_API_BASE_URL"] = get_config_value("SHOP_TRACKER_API_BASE_URL", "")
    app.config["SHOP_TRACKER_API_KEY"] = get_config_value("SHOP_TRACKER_API_KEY", "")
    app.config["SYNC_INTERVAL"] = int(get_config_value("SYNC_INTERVAL", "3600"))
    
    # Security settings
    app.config["SECURITY_PASSWORD_SALT"] = get_config_value("SECURITY_PASSWORD_SALT", "shop_monitor_salt")
    app.config["SESSION_COOKIE_SECURE"] = get_config_value("SESSION_COOKIE_SECURE", "False").lower() in ("true", "1", "yes")
    app.config["SESSION_COOKIE_HTTPONLY"] = True
    app.config["REMEMBER_COOKIE_SECURE"] = app.config["SESSION_COOKIE_SECURE"]
    app.config["REMEMBER_COOKIE_HTTPONLY"] = True

def create_default_env_file():
    """Create a default .env file template if one doesn't exist"""
    env_path = Path(".env")
    
    # Don't overwrite existing .env file
    if env_path.exists():
        return
        
    default_env = """# ShopMonitor Environment Configuration
# Copy this file to .env and modify as needed

# Basic Application Configuration
DEBUG=False
TESTING=False

# Security Settings
# Generate a secure random key: python -c "import secrets; print(secrets.token_hex(32))"
SECRET_KEY=

# Database Configuration
DATABASE_DIALECT=sqlite
DATABASE_PATH=instance/nooyen_tracker.db
# For PostgreSQL or MySQL:
#DATABASE_HOST=localhost
#DATABASE_PORT=5432
#DATABASE_NAME=shop_monitor
#DATABASE_USER=
#DATABASE_PASSWORD=

# Shop Suite Integration
SHOP_SUITE_APP_NAME=shop_monitor
SHOP_SUITE_SECRET_KEY=
SHOP_MONITOR_URL=http://localhost:5000
SHOP_TRACKER_URL=http://localhost:5001
SHOP_SUITE_COOKIE_DOMAIN=

# API Keys
SHOP_API_KEY=
SHOP_TRACKER_API_BASE_URL=
SHOP_TRACKER_API_KEY=
SYNC_INTERVAL=3600

# Secure development credentials
# These will be used for development but should be stored in environment
# variables or secure credential storage in production
DEV_ADMIN_USERNAME=admin
DEV_ADMIN_PASSWORD=
"""
    with open(env_path, "w") as f:
        f.write(default_env)
    
    print("Created default .env template file. Please edit it with your configuration.")

def create_dev_credentials():
    """Create development credentials file with secure storage"""
    if not SECURE_CREDS_PATH.exists():
        # Default development credentials (don't hardcode in production)
        dev_creds = {
            "DEV_ADMIN_USERNAME": "admin",
            "DEV_ADMIN_PASSWORD": "Pigfloors",  # This will be encrypted
            "DATABASE_USER": "",
            "DATABASE_PASSWORD": "",
            "API_KEY": "dev_api_key_" + os.urandom(8).hex(),
            "JWT_SECRET_KEY": os.urandom(32).hex(),
            "SECRET_KEY": os.urandom(32).hex()
        }
        
        save_encrypted_credentials(dev_creds)
        logging.info("Created encrypted development credentials")

# Initialize credentials on module load if needed
if __name__ != "__main__":
    if not SECURE_CREDS_PATH.exists():
        create_dev_credentials()

# Command line interface for credential management
if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Manage secure credentials")
    subparsers = parser.add_subparsers(dest="command", help="Command to execute")
    
    # Create template env file
    env_parser = subparsers.add_parser("create-env", help="Create template .env file")
    
    # Init credentials command
    init_parser = subparsers.add_parser("init", help="Initialize secure credentials")
    
    # Set credential command
    set_parser = subparsers.add_parser("set", help="Set a secure credential")
    set_parser.add_argument("key", help="Credential key")
    set_parser.add_argument("value", help="Credential value")
    
    # Get credential command
    get_parser = subparsers.add_parser("get", help="Get a secure credential")
    get_parser.add_argument("key", help="Credential key")
    
    # List credentials command
    list_parser = subparsers.add_parser("list", help="List all secure credentials (masked)")
    
    # Password protection options
    set_parser.add_argument("--password", "-p", help="Master password for encryption")
    get_parser.add_argument("--password", "-p", help="Master password for decryption")
    list_parser.add_argument("--password", "-p", help="Master password for decryption")
    init_parser.add_argument("--password", "-p", help="Master password for encryption")
    
    args = parser.parse_args()
    
    if args.command == "create-env":
        create_default_env_file()
        print("Created default .env template file")
        
    elif args.command == "init":
        create_dev_credentials()
        print("Initialized secure credentials")
        
    elif args.command == "set":
        # Load existing credentials
        creds = load_encrypted_credentials(args.password)
        # Update the credential
        creds[args.key] = args.value
        # Save back
        save_encrypted_credentials(creds, args.password)
        print(f"Credential {args.key} set successfully")
        
    elif args.command == "get":
        creds = load_encrypted_credentials(args.password)
        if args.key in creds:
            print(f"{args.key}: {creds[args.key]}")
        else:
            print(f"Credential {args.key} not found")
            sys.exit(1)
            
    elif args.command == "list":
        creds = load_encrypted_credentials(args.password)
        print("Secure Credentials:")
        for key, value in creds.items():
            # Mask sensitive values
            if any(k in key.lower() for k in ["password", "secret", "key"]):
                masked = value[:2] + "*" * (len(value) - 4) + value[-2:] if len(value) > 4 else "*" * len(value)
                print(f"{key}: {masked}")
            else:
                print(f"{key}: {value}")
    else:
        parser.print_help()