from app import app
from routes import *
import os
import logging

# Initialize and register the integration blueprints
from integration.api_routes import integration_bp
from integration.web_routes import web_routes_bp
from integration.user_api import user_api_bp
from integration import integration
from integration.device_discovery import device_manager

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Register the integration blueprints
app.register_blueprint(integration_bp)
app.register_blueprint(web_routes_bp)
app.register_blueprint(user_api_bp)

# Initialize the integration
integration.init_app(app)

# Initialize device discovery for ESP32 nodes
device_manager.init_app(app)

# Create firmware directory if it doesn't exist
firmware_dir = os.path.join(os.path.dirname(__file__), "firmware")
if not os.path.exists(firmware_dir):
    os.makedirs(firmware_dir)
    logger.info(f"Created firmware directory at {firmware_dir}")

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
