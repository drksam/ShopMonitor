"""
Shop Suite Shared Models
-----------------------
Database models shared between ShopMonitor and ShopTracker applications
"""

import datetime
import json
from flask_login import UserMixin
from app import db


class SuiteUser(UserMixin, db.Model):
    """Unified user model for cross-app authentication"""
    __tablename__ = 'suite_users'
    
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(64), unique=True, nullable=False)
    email = db.Column(db.String(120), unique=True, nullable=False)
    password_hash = db.Column(db.String(256), nullable=False)
    display_name = db.Column(db.String(100))
    active = db.Column(db.Boolean, default=True)
    
    # Authentication details
    auth_provider = db.Column(db.String(50), default="local")  # local, azure, google, etc.
    external_id = db.Column(db.String(100))  # ID in external auth system
    is_admin = db.Column(db.Boolean, default=False)
    
    # Session tracking
    current_app = db.Column(db.String(50))  # Which app user is currently using
    last_login = db.Column(db.DateTime)
    
    # Cross-app management
    created_by_app = db.Column(db.String(50))  # "shop_monitor" or "shop_tracker"
    managed_by_app = db.Column(db.String(50))  # Which app manages this user
    
    # RFID integration
    rfid_tag = db.Column(db.String(64))  # Optional RFID association
    
    # Timestamps
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    updated_at = db.Column(db.DateTime, onupdate=datetime.datetime.utcnow)
    
    # Relationships
    permissions = db.relationship('SuitePermission', backref='user', lazy='dynamic')
    sessions = db.relationship('UserSession', backref='user', lazy='dynamic')
    
    # Legacy user mappings
    admin_user = db.relationship('User', secondary='user_suite_mapping', 
                             backref=db.backref('suite_user', uselist=False), 
                             uselist=False)
    rfid_user = db.relationship('RFIDUser', secondary='rfid_suite_mapping', 
                             backref=db.backref('suite_user', uselist=False), 
                             uselist=False)
    
    @property
    def is_active(self):
        """Required by Flask-Login"""
        return self.active
    
    def has_permission(self, resource_type, resource_id, app_name, required_level):
        """Check if user has a specific permission level for a resource
        
        Args:
            resource_type: Type of resource (area, machine, report, etc.)
            resource_id: ID of the resource
            app_name: Name of the application ("shop_monitor" or "shop_tracker")
            required_level: Required permission level (view, edit, admin, etc.)
            
        Returns:
            bool: Whether user has the required permission
        """
        # Admins have all permissions
        if self.is_admin:
            return True
            
        # Check for specific permission
        permission = SuitePermission.query.filter_by(
            user_id=self.id,
            resource_type=resource_type,
            resource_id=resource_id,
            app_context=app_name
        ).first()
        
        if not permission:
            return False
            
        # Check that permission is valid
        now = datetime.datetime.utcnow()
        if permission.valid_until and permission.valid_until < now:
            return False
            
        # Map permission levels to numeric values for comparison
        level_map = {
            'view': 10, 
            'use': 20, 
            'operate': 30, 
            'edit': 40, 
            'admin': 50,
            'owner': 60
        }
        
        required_value = level_map.get(required_level, 0)
        actual_value = level_map.get(permission.permission_level, 0)
        
        return actual_value >= required_value
    
    def get_cross_app_token(self):
        """Generate a secure token for cross-app authentication"""
        # This would be implemented with JWT or similar in production
        from secrets import token_urlsafe
        
        # Check for existing valid session
        existing_session = UserSession.query.filter_by(
            user_id=self.id,
            invalidated=False
        ).filter(
            UserSession.expires_at > datetime.datetime.utcnow()
        ).first()
        
        if existing_session:
            return existing_session.session_token
            
        # Create new session
        expiration = datetime.datetime.utcnow() + datetime.timedelta(days=1)
        session = UserSession(
            user_id=self.id,
            session_token=token_urlsafe(32),
            expires_at=expiration
        )
        db.session.add(session)
        db.session.commit()
        
        return session.session_token


class SuitePermission(db.Model):
    """Unified permission model across applications"""
    __tablename__ = 'suite_permissions'
    
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('suite_users.id'), nullable=False)
    
    # Target resource
    resource_type = db.Column(db.String(50), nullable=False)  # area, machine, report, etc.
    resource_id = db.Column(db.Integer, nullable=False)  # The ID of the resource
    
    # Permission details
    app_context = db.Column(db.String(50), nullable=False)  # Which app this permission applies to
    permission_level = db.Column(db.String(20), nullable=False)  # view, edit, admin, operate, etc.
    
    # Validity
    valid_from = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    valid_until = db.Column(db.DateTime, nullable=True)  # Optional expiration
    
    # Timestamps
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    updated_at = db.Column(db.DateTime, onupdate=datetime.datetime.utcnow)
    
    # Grant information 
    granted_by = db.Column(db.Integer, db.ForeignKey('suite_users.id'))
    grantor = db.relationship('SuiteUser', foreign_keys=[granted_by])
    
    @property
    def is_valid(self):
        """Check if permission is currently valid"""
        now = datetime.datetime.utcnow()
        return (not self.valid_until or self.valid_until > now) and self.valid_from <= now


class UserSession(db.Model):
    """Tracks user sessions across applications for SSO"""
    __tablename__ = 'user_sessions'
    
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('suite_users.id'), nullable=False)
    session_token = db.Column(db.String(128), unique=True, nullable=False, index=True)
    
    # Session metadata
    ip_address = db.Column(db.String(45))
    user_agent = db.Column(db.String(255))
    current_app = db.Column(db.String(50))  # Which app user is currently using
    
    # Validity
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow) 
    expires_at = db.Column(db.DateTime, nullable=False)
    last_active = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    invalidated = db.Column(db.Boolean, default=False)
    
    @property
    def is_active(self):
        """Check if session is still valid"""
        now = datetime.datetime.utcnow()
        return not self.invalidated and now < self.expires_at
    
    def update_activity(self):
        """Update the last activity timestamp"""
        self.last_active = datetime.datetime.utcnow()
        db.session.commit()
    
    def invalidate(self):
        """Invalidate this session"""
        self.invalidated = True
        db.session.commit()


class SyncEvent(db.Model):
    """Tracks synchronization events between applications"""
    __tablename__ = 'sync_events'
    
    id = db.Column(db.Integer, primary_key=True)
    event_type = db.Column(db.String(50), nullable=False)  # user.created, machine.updated, etc.
    resource_type = db.Column(db.String(50), nullable=False)  # user, machine, area, etc.
    resource_id = db.Column(db.Integer, nullable=False)
    source_app = db.Column(db.String(50), nullable=False)  # Which app generated this event
    target_app = db.Column(db.String(50), nullable=False)  # Which app should process this event
    status = db.Column(db.String(20), default="pending", index=True)  # pending, processed, failed
    payload = db.Column(db.Text)  # JSON data for the event
    
    # Processing metadata
    attempts = db.Column(db.Integer, default=0)
    last_attempt = db.Column(db.DateTime)
    processed_at = db.Column(db.DateTime)
    error_message = db.Column(db.Text)
    
    # Timestamps
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    updated_at = db.Column(db.DateTime, onupdate=datetime.datetime.utcnow)
    
    def get_payload(self):
        """Get event payload as a dictionary"""
        if not self.payload:
            return {}
        try:
            return json.loads(self.payload)
        except json.JSONDecodeError:
            return {}
    
    def set_payload(self, data):
        """Set the payload from a dictionary"""
        self.payload = json.dumps(data)
    
    def mark_processed(self):
        """Mark this event as successfully processed"""
        self.status = "processed"
        self.processed_at = datetime.datetime.utcnow()
        db.session.commit()
    
    def mark_failed(self, error_message):
        """Mark this event as failed with an error message"""
        self.status = "failed"
        self.attempts += 1
        self.last_attempt = datetime.datetime.utcnow()
        self.error_message = error_message
        db.session.commit()
    
    def retry(self):
        """Reset status to retry this event"""
        self.status = "pending"
        db.session.commit()


# Mapping tables for legacy integration
class UserSuiteMapping(db.Model):
    """Maps legacy admin users to suite users"""
    __tablename__ = 'user_suite_mapping'
    
    id = db.Column(db.Integer, primary_key=True)
    legacy_user_id = db.Column(db.Integer, db.ForeignKey('user.id'), unique=True)
    suite_user_id = db.Column(db.Integer, db.ForeignKey('suite_users.id'), unique=True)
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)


class RFIDSuiteMapping(db.Model):
    """Maps RFID users to suite users"""
    __tablename__ = 'rfid_suite_mapping'
    
    id = db.Column(db.Integer, primary_key=True)
    rfid_user_id = db.Column(db.Integer, db.ForeignKey('rfid_user.id'), unique=True)
    suite_user_id = db.Column(db.Integer, db.ForeignKey('suite_users.id'), unique=True)
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)