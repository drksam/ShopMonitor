import os
import logging

from flask import Flask
from flask_sqlalchemy import SQLAlchemy
from sqlalchemy.orm import DeclarativeBase
from flask_login import LoginManager
from werkzeug.middleware.proxy_fix import ProxyFix

# Application version information
APP_VERSION = "1.0.1"
SUITE_VERSION = "1.0.1"

# Configure logging
logging.basicConfig(level=logging.DEBUG)

class Base(DeclarativeBase):
    pass

# Initialize SQLAlchemy
db = SQLAlchemy(model_class=Base)

# Create the Flask app
app = Flask(__name__)
app.secret_key = os.environ.get("SESSION_SECRET", "shop_suite_shared_secret")
app.wsgi_app = ProxyFix(app.wsgi_app, x_proto=1, x_host=1)

# Configure database
app.config["SQLALCHEMY_DATABASE_URI"] = os.environ.get("DATABASE_URL", "sqlite:///instance/shop_monitor.db")
app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False
app.config["SQLALCHEMY_ENGINE_OPTIONS"] = {
    "pool_size": 10,
    "max_overflow": 20,
    "pool_recycle": 300,
    "pool_pre_ping": True,
}

# Enable SQLAlchemy query tracking for development
app.config["SQLALCHEMY_RECORD_QUERIES"] = app.debug
app.config["SQLALCHEMY_QUERY_THRESHOLD"] = 0.5  # Log slow queries (>0.5s)

# Initialize the database
db.init_app(app)

# Shop Suite Cross-App Configuration
app.config["SHOP_SUITE_APP_NAME"] = "shop_monitor"
app.config["SHOP_SUITE_SECRET_KEY"] = os.environ.get("SHOP_SUITE_SECRET_KEY", app.secret_key)
app.config["SHOP_MONITOR_URL"] = os.environ.get("SHOP_MONITOR_URL", "http://localhost:5000")
app.config["SHOP_TRACKER_URL"] = os.environ.get("SHOP_TRACKER_URL", "http://localhost:5001")
app.config["SHOP_SUITE_COOKIE_DOMAIN"] = os.environ.get("SHOP_SUITE_COOKIE_DOMAIN", None)

# Load ShopTracker integration configuration from environment
app.config["SHOP_TRACKER_API_BASE_URL"] = os.environ.get("SHOP_TRACKER_API_BASE_URL", "")
app.config["SHOP_TRACKER_API_KEY"] = os.environ.get("SHOP_TRACKER_API_KEY", "")
app.config["SYNC_INTERVAL"] = int(os.environ.get("SYNC_INTERVAL", "3600"))

# Configure Flask-Login
login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view = 'login'

# Initialize Shop Suite Auth
from shop_suite.auth import ShopSuiteAuth
suite_auth = ShopSuiteAuth(app)

# Initialize our error handling system
import error_handling
error_handling.init_app(app)

# Create database tables
with app.app_context():
    # Import models here to avoid circular imports
    import models
    from shop_suite.models import SuiteUser, SuitePermission, UserSession
    
    db.create_all()

    # Create default admin user if no users exist
    from models import User, Role
    from werkzeug.security import generate_password_hash
    
    if not User.query.first():
        admin_role = Role(name="admin", description="Administrator role")
        db.session.add(admin_role)
        db.session.commit()
        
        admin_user = User(
            username="admin",
            email="admin@shop-monitor.com",
            password_hash=generate_password_hash("Pigfloors"),
            role_id=admin_role.id
        )
        db.session.add(admin_user)
        db.session.commit()
        logging.info("Created default admin user")
        
        # Create corresponding suite user if needed
        if not SuiteUser.query.filter_by(username="admin").first():
            suite_user = SuiteUser(
                username="admin",
                email="admin@shop-monitor.com",
                password_hash=admin_user.password_hash,
                display_name="Administrator",
                is_admin=True,
                created_by_app="shop_monitor",
                managed_by_app="shop_monitor"
            )
            db.session.add(suite_user)
            db.session.commit()
            
            # Create mapping between legacy user and suite user
            from shop_suite.models import UserSuiteMapping
            mapping = UserSuiteMapping(
                legacy_user_id=admin_user.id,
                suite_user_id=suite_user.id
            )
            db.session.add(mapping)
            db.session.commit()
            
            logging.info("Created default admin suite user")

@login_manager.user_loader
def load_user(user_id):
    # Try to load from SuiteUser first if ID format indicates it
    if user_id and user_id.startswith('suite_'):
        from shop_suite.models import SuiteUser
        return SuiteUser.query.get(int(user_id[6:]))
    
    # Fall back to legacy user model
    from models import User
    return User.query.get(int(user_id))

# Context processor to add datetime to all templates
@app.context_processor
def inject_now():
    from datetime import datetime
    return {'now': datetime.now()}

# Context processor to add Shop Suite navigation helpers
@app.context_processor
def inject_shop_suite():
    def get_shop_tracker_url(path=None):
        """Generate URL to ShopTracker app with SSO"""
        from flask_login import current_user
        if hasattr(current_user, 'is_authenticated') and current_user.is_authenticated:
            if hasattr(current_user, 'suite_user') and current_user.suite_user:
                return suite_auth.generate_sso_url(
                    current_user.suite_user, 
                    "shop_tracker",
                    path
                )
        return app.config["SHOP_TRACKER_URL"] + (path or "")
    
    return {
        'get_shop_tracker_url': get_shop_tracker_url,
        'shop_suite_app_name': app.config["SHOP_SUITE_APP_NAME"]
    }

# Note: Integration blueprints are registered in main.py
