import datetime
import json
from app import db
from flask_login import UserMixin

# Role model for user permissions
class Role(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(64), unique=True, nullable=False)
    description = db.Column(db.String(255))
    users = db.relationship('User', backref='role', lazy='dynamic')

# User model for admin access to the system
class User(UserMixin, db.Model):
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(64), unique=True, nullable=False)
    email = db.Column(db.String(120), unique=True, nullable=False)
    password_hash = db.Column(db.String(256), nullable=False)
    role_id = db.Column(db.Integer, db.ForeignKey('role.id'))
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)

# Location model for organizing machines
class Zone(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(64), nullable=False)
    description = db.Column(db.String(255))
    machines = db.relationship('Machine', backref='zone', lazy='dynamic')
    
    # Rename the display for clarity
    @property
    def display_name(self):
        return f"{self.name} Location"

# Node model for managing devices (ESP32/Arduino controllers)
class Node(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    node_id = db.Column(db.String(32), unique=True, nullable=False)  # Unique node identifier (mDNS name or assigned ID)
    name = db.Column(db.String(64), nullable=False)
    description = db.Column(db.String(255))
    ip_address = db.Column(db.String(15))  # IP address of the node
    node_type = db.Column(db.String(20), default="machine_monitor")  # machine_monitor, accessory_io
    is_esp32 = db.Column(db.Boolean, default=False)  # ESP32 or Arduino
    last_seen = db.Column(db.DateTime)
    firmware_version = db.Column(db.String(20))
    config = db.Column(db.Text)  # JSON configuration
    
    # Get connected machines
    machines = db.relationship('Machine', backref='node', lazy='dynamic')
    
    # Helper methods
    def get_config_dict(self):
        """Get the configuration as a dictionary"""
        if not self.config:
            return {}
        try:
            return json.loads(self.config)
        except:
            return {}
    
    def set_config_dict(self, config_dict):
        """Set the configuration from a dictionary"""
        self.config = json.dumps(config_dict)
    
    @property
    def machine_count(self):
        """Get the number of machines connected to this node"""
        return self.machines.count()
    
    @property
    def status(self):
        """Get the node status based on last_seen"""
        if not self.last_seen:
            return "unknown"
        
        # If we've seen the node in the last 5 minutes, it's online
        if (datetime.datetime.utcnow() - self.last_seen).total_seconds() < 300:
            return "online"
        
        return "offline"


# Machine model for tracking equipment
class Machine(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    machine_id = db.Column(db.String(2), unique=True, nullable=False)  # 2-digit machine ID
    name = db.Column(db.String(64), nullable=False)
    description = db.Column(db.String(255))
    zone_id = db.Column(db.Integer, db.ForeignKey('zone.id'))
    ip_address = db.Column(db.String(15))  # Legacy field for direct IP connection
    node_id = db.Column(db.Integer, db.ForeignKey('node.id'))  # Node that controls this machine
    node_port = db.Column(db.Integer, default=0)  # Port/Zone on the node (0-3)
    status = db.Column(db.String(20), default="idle")  # idle, active, warning, offline
    current_user_id = db.Column(db.Integer, db.ForeignKey('rfid_user.id'), nullable=True)
    last_activity = db.Column(db.DateTime)
    logs = db.relationship('MachineLog', backref='machine', lazy='dynamic')
    
    # Helper methods
    @property
    def connection_method(self):
        """Return whether machine uses direct connection or node-based"""
        if self.node_id:
            return "node"
        elif self.ip_address:
            return "direct"
        else:
            return "unknown"
    
    @property
    def controller_name(self):
        """Get the controller name (IP or node name)"""
        if self.node_id and self.node:
            return self.node.name
        return self.ip_address or "Not connected"
    
    @property
    def zone_name(self):
        """Get the zone/location name safely"""
        if self.zone_id and self.zone:
            return self.zone.name
        return "Unassigned"


# RFID User model (different from admin users)
class RFIDUser(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    rfid_tag = db.Column(db.String(64), unique=True, nullable=False)
    name = db.Column(db.String(64), nullable=False)
    email = db.Column(db.String(120))
    active = db.Column(db.Boolean, default=True)
    is_offline_access = db.Column(db.Boolean, default=False)  # For emergency offline access
    is_admin_override = db.Column(db.Boolean, default=False)  # For admin override access
    external_id = db.Column(db.Integer)  # ID in NooyenUSATracker system, for synchronization
    last_synced = db.Column(db.DateTime)  # Last time this user was synced with NooyenUSATracker
    authorized_machines = db.relationship('MachineAuthorization', backref='user', lazy='dynamic')
    logs = db.relationship('MachineLog', backref='user', lazy='dynamic')
    current_machine = db.relationship('Machine', backref='current_user', uselist=False, 
                                     foreign_keys=[Machine.current_user_id])
    
    def has_machine_access(self, machine_id):
        """Check if user has access to a specific machine
        
        Args:
            machine_id: Integer ID of the machine to check
            
        Returns:
            bool: True if user has access or admin override
        """
        # Admin override users can access any machine
        if self.is_admin_override:
            return True
            
        # Check specific authorization
        auth = MachineAuthorization.query.filter_by(
            user_id=self.id, 
            machine_id=machine_id
        ).first()
        
        return auth is not None
        
    def get_authorized_machine_ids(self):
        """Get list of machine IDs this user is authorized to use
        
        Returns:
            list: List of machine ID integers
        """
        # If admin override, return all machine IDs
        if self.is_admin_override:
            return [m.id for m in Machine.query.all()]
            
        # Otherwise return specifically authorized machines
        return [auth.machine_id for auth in self.authorized_machines]


# Machine authorization model
class MachineAuthorization(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('rfid_user.id'), nullable=False)
    machine_id = db.Column(db.Integer, db.ForeignKey('machine.id'), nullable=False)
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    
    # Add a unique constraint to prevent duplicate authorizations
    __table_args__ = (db.UniqueConstraint('user_id', 'machine_id'),)


# Accessory I/O model for external devices
class AccessoryIO(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    node_id = db.Column(db.Integer, db.ForeignKey('node.id'), nullable=False)
    name = db.Column(db.String(64), nullable=False)
    description = db.Column(db.String(255))
    io_type = db.Column(db.String(10), nullable=False)  # input, output
    io_number = db.Column(db.Integer, nullable=False)  # 0-3 for inputs, 0-3 for outputs
    linked_machine_id = db.Column(db.Integer, db.ForeignKey('machine.id'), nullable=True)
    activation_delay = db.Column(db.Integer, default=0)  # seconds to wait before activation
    deactivation_delay = db.Column(db.Integer, default=0)  # seconds to wait before deactivation
    active = db.Column(db.Boolean, default=True)
    
    # Relationships
    node = db.relationship('Node', backref=db.backref('accessories', lazy='dynamic'))
    linked_machine = db.relationship('Machine', backref=db.backref('accessories', lazy='dynamic'))
    
    # Use a unique constraint to ensure only one device per node/type/number
    __table_args__ = (db.UniqueConstraint('node_id', 'io_type', 'io_number'),)
    
    # Helper properties
    @property
    def io_name(self):
        """Get formatted IO name with type and number"""
        return f"{self.io_type.title()} {self.io_number+1}"
    
    @property
    def machine_name(self):
        """Get linked machine name safely"""
        if self.linked_machine_id and self.linked_machine:
            return self.linked_machine.name
        return "None"


# Machine usage log
class MachineLog(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    machine_id = db.Column(db.Integer, db.ForeignKey('machine.id'), nullable=False)
    user_id = db.Column(db.Integer, db.ForeignKey('rfid_user.id'), nullable=False)
    login_time = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    logout_time = db.Column(db.DateTime, nullable=True)
    total_time = db.Column(db.Integer, nullable=True)  # Total time in seconds
    status = db.Column(db.String(20), default="active")  # active, completed, timeout, error


# Alert system for bidirectional communication with NooyenUSATracker
class Alert(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    external_id = db.Column(db.Integer, nullable=True)  # ID from NooyenUSATracker
    machine_id = db.Column(db.Integer, db.ForeignKey('machine.id'), nullable=True)
    node_id = db.Column(db.Integer, db.ForeignKey('node.id'), nullable=True)
    user_id = db.Column(db.Integer, db.ForeignKey('rfid_user.id'), nullable=True)
    message = db.Column(db.String(255), nullable=False)
    alert_type = db.Column(db.String(20), nullable=False)  # warning, error, info, maintenance
    status = db.Column(db.String(20), default="pending")  # pending, acknowledged, resolved
    origin = db.Column(db.String(20), nullable=False)  # machine, user, system, nooyen_tracker
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    acknowledged_at = db.Column(db.DateTime, nullable=True)
    resolved_at = db.Column(db.DateTime, nullable=True)
    acknowledged_by = db.Column(db.Integer, db.ForeignKey('user.id'), nullable=True)
    resolved_by = db.Column(db.Integer, db.ForeignKey('user.id'), nullable=True)
    
    # Relationships
    machine = db.relationship('Machine', backref=db.backref('alerts', lazy='dynamic'))
    node = db.relationship('Node', backref=db.backref('alerts', lazy='dynamic'))
    user = db.relationship('RFIDUser', backref=db.backref('alerts', lazy='dynamic'))
    acknowledger = db.relationship('User', foreign_keys=[acknowledged_by], backref=db.backref('acknowledged_alerts', lazy='dynamic'))
    resolver = db.relationship('User', foreign_keys=[resolved_by], backref=db.backref('resolved_alerts', lazy='dynamic'))
    
    # Return alert JSON for API consumption
    def to_dict(self):
        return {
            'id': self.id,
            'external_id': self.external_id,
            'machine_id': self.machine.machine_id if self.machine else None,
            'machine_name': self.machine.name if self.machine else None,
            'node_id': self.node.node_id if self.node else None,
            'node_name': self.node.name if self.node else None,
            'user_id': self.user_id,
            'user_name': self.user.name if self.user else None,
            'message': self.message,
            'alert_type': self.alert_type,
            'status': self.status,
            'origin': self.origin,
            'created_at': self.created_at.isoformat() + 'Z',
            'acknowledged_at': self.acknowledged_at.isoformat() + 'Z' if self.acknowledged_at else None,
            'resolved_at': self.resolved_at.isoformat() + 'Z' if self.resolved_at else None
        }
