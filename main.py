from app import app
from routes import *
import os
import logging
from security import init_app as init_security
from error_handling import logger as error_logger, init_app as init_error_handling, load_queued_errors, save_queued_errors
import atexit

# Initialize and register the integration blueprints
from integration.api_routes import integration_bp
from integration.web_routes import web_routes_bp
from integration.user_api import user_api_bp
from integration import integration
from integration.device_discovery import device_manager

# Import our new area and zone blueprints
from routes.areas import areas_bp
from routes.zones import zones_bp

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)



# Register the integration blueprints
app.register_blueprint(integration_bp)
app.register_blueprint(web_routes_bp)
app.register_blueprint(user_api_bp)

# Register our area and zone blueprints
app.register_blueprint(areas_bp)
app.register_blueprint(zones_bp)

# Initialize the security system
init_security(app)
error_logger.info("Security system initialized")

# Initialize the enhanced error handling system
init_error_handling(app)
error_logger.info("Enhanced error handling system initialized")

# Load any previously queued errors from disk
load_queued_errors()

# Register function to save queued errors on shutdown
atexit.register(save_queued_errors)

# Initialize the integration
integration.init_app(app)

# Initialize device discovery for ESP32 nodes
device_manager.init_app(app)

# Create firmware directory if it doesn't exist
firmware_dir = os.path.join(os.path.dirname(__file__), "firmware")
if not os.path.exists(firmware_dir):
    os.makedirs(firmware_dir)
    logger.info(f"Created firmware directory at {firmware_dir}")

# Create logs directory if it doesn't exist
logs_dir = os.path.join(os.path.dirname(__file__), "logs")
if not os.path.exists(logs_dir):
    os.makedirs(logs_dir)
    logger.info(f"Created logs directory at {logs_dir}")

if __name__ == "__main__":
    # In production, set debug=False
    debug_mode = os.environ.get('DEBUG', 'True').lower() in ('true', '1', 't')
    
    # Set up graceful shutdown handling
    try:
        app.run(host="0.0.0.0", port=5000, debug=debug_mode)
    except KeyboardInterrupt:
        logger.info("Server shutting down...")
        save_queued_errors()  # Save any queued errors
    except Exception as e:
        logger.error(f"Server error: {str(e)}")
        save_queued_errors()  # Save any queued errors in case of crash
