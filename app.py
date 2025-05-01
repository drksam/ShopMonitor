import os
import logging

from flask import Flask
from flask_sqlalchemy import SQLAlchemy
from sqlalchemy.orm import DeclarativeBase
from flask_login import LoginManager
from werkzeug.middleware.proxy_fix import ProxyFix

# Configure logging
logging.basicConfig(level=logging.DEBUG)

class Base(DeclarativeBase):
    pass

# Initialize SQLAlchemy
db = SQLAlchemy(model_class=Base)

# Create the Flask app
app = Flask(__name__)
app.secret_key = os.environ.get("SESSION_SECRET", "nooyen_rfid_tracker_secret")
app.wsgi_app = ProxyFix(app.wsgi_app, x_proto=1, x_host=1)

# Configure SQLite database
app.config["SQLALCHEMY_DATABASE_URI"] = os.environ.get("DATABASE_URL", "sqlite:///nooyen_tracker.db")
app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False
app.config["SQLALCHEMY_ENGINE_OPTIONS"] = {
    "pool_recycle": 300,
    "pool_pre_ping": True,
}

# Initialize the database
db.init_app(app)

# Load NooyenUSATracker integration configuration from environment
app.config["NOOYEN_API_BASE_URL"] = os.environ.get("NOOYEN_API_BASE_URL", "")
app.config["NOOYEN_API_KEY"] = os.environ.get("NOOYEN_API_KEY", "")
app.config["SYNC_INTERVAL"] = int(os.environ.get("SYNC_INTERVAL", "3600"))

# Configure Flask-Login
login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view = 'login'

# Create database tables
with app.app_context():
    # Import models here to avoid circular imports
    import models
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
            email="admin@nooyen.com",
            password_hash=generate_password_hash("Pigfloors"),
            role_id=admin_role.id
        )
        db.session.add(admin_user)
        db.session.commit()
        logging.info("Created default admin user")

@login_manager.user_loader
def load_user(user_id):
    from models import User
    return User.query.get(int(user_id))

# Context processor to add datetime to all templates
@app.context_processor
def inject_now():
    from datetime import datetime
    return {'now': datetime.now()}

# Note: Integration blueprints are registered in main.py
