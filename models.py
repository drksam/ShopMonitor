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
    
    # Area permissions - many-to-many relationship
    area_permissions = db.relationship('AreaPermission', backref='user', lazy='dynamic')
    
    def has_area_permission(self, area_id):
        """Check if user has access to a specific area
        
        Args:
            area_id: Integer ID of the area to check
            
        Returns:
            bool: True if user has access to this area
        """
        # Check if user has admin role
        if self.role and self.role.name == "admin":
            return True
            
        # Check for specific area permission
        permission = AreaPermission.query.filter_by(
            user_id=self.id, 
            area_id=area_id
        ).first()
        
        return permission is not None

# Area model for building-level organization (top level in hierarchy)
class Area(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(64), nullable=False)
    description = db.Column(db.String(255))
    code = db.Column(db.String(10), unique=True, nullable=False)  # Short code for area
    active = db.Column(db.Boolean, default=True)
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    
    # Relationships
    zones = db.relationship('Zone', backref='area', lazy='dynamic')
    user_permissions = db.relationship('AreaPermission', backref='area', lazy='dynamic')
    
    @property
    def machine_count(self):
        """Get total number of machines in this area"""
        count = 0
        for zone in self.zones:
            count += zone.machines.count()
        return count
    
    @property
    def active_zones(self):
        """Get count of active zones in this area"""
        return self.zones.filter_by(active=True).count()

# Area permissions for users
class AreaPermission(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('user.id'), nullable=False)
    area_id = db.Column(db.Integer, db.ForeignKey('area.id'), nullable=False)
    permission_level = db.Column(db.String(20), default="view")  # view, operate, manage, admin
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    
    # Unique constraint to prevent duplicate permissions
    __table_args__ = (db.UniqueConstraint('user_id', 'area_id'),)

# Location model (previously Zone) for organizing machines within an area
class Zone(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(64), nullable=False)
    description = db.Column(db.String(255))
    area_id = db.Column(db.Integer, db.ForeignKey('area.id'))  # Link to parent area
    code = db.Column(db.String(10))  # Short code for zone/location
    active = db.Column(db.Boolean, default=True)
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    machines = db.relationship('Machine', backref='zone', lazy='dynamic')
    
    # Rename the display for clarity
    @property
    def display_name(self):
        return f"{self.name} Location"
    
    @property
    def full_code(self):
        """Get hierarchical code including area code"""
        if self.area:
            return f"{self.area.code}-{self.code}"
        return self.code

# Node model for managing devices (ESP32/Arduino controllers)
class Node(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    node_id = db.Column(db.String(32), unique=True, nullable=False)  # Unique node identifier (mDNS name or assigned ID)
    name = db.Column(db.String(64), nullable=False)
    description = db.Column(db.String(255))
    ip_address = db.Column(db.String(15))  # IP address of the node
    node_type = db.Column(db.String(20), default="machine_monitor")  # machine_monitor, office_reader, accessory_io, location_display, machine_display, area_controller
    is_esp32 = db.Column(db.Boolean, default=False)  # ESP32 or Arduino
    has_display = db.Column(db.Boolean, default=False)  # Whether node has CYD display
    area_id = db.Column(db.Integer, db.ForeignKey('area.id'), nullable=True)  # Area this node belongs to
    zone_id = db.Column(db.Integer, db.ForeignKey('zone.id'), nullable=True)  # Zone this node belongs to
    last_seen = db.Column(db.DateTime)
    firmware_version = db.Column(db.String(20))
    config = db.Column(db.Text)  # JSON configuration
    
    # Get connected machines
    machines = db.relationship('Machine', backref='node', lazy='dynamic')
    
    # Add relationships for area and zone
    area = db.relationship('Area', backref=db.backref('nodes', lazy='dynamic'))
    zone_rel = db.relationship('Zone', backref=db.backref('nodes', lazy='dynamic'))
    
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
    
    @property
    def location_hierarchy(self):
        """Get formatted string of area and zone"""
        area_name = self.area.name if self.area else "Unassigned"
        zone_name = self.zone_rel.name if self.zone_rel else "Unassigned"
        return f"{area_name} / {zone_name}"

# Machine model for tracking equipment
class Machine(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    machine_id = db.Column(db.String(10), unique=True, nullable=False)  # Machine ID (expanded from 2 digits)
    name = db.Column(db.String(64), nullable=False)
    description = db.Column(db.String(255))
    zone_id = db.Column(db.Integer, db.ForeignKey('zone.id'))
    ip_address = db.Column(db.String(15))  # Legacy field for direct IP connection
    node_id = db.Column(db.Integer, db.ForeignKey('node.id'))  # Node that controls this machine
    node_port = db.Column(db.Integer, default=0)  # Port/Zone on the node (0-3)
    status = db.Column(db.String(20), default="idle")  # idle, active, warning, offline
    current_users = db.relationship('MachineSession', backref='machine', lazy='dynamic', 
                                   primaryjoin="and_(Machine.id==MachineSession.machine_id, MachineSession.logout_time==None)")
    lead_operator_id = db.Column(db.Integer, db.ForeignKey('rfid_user.id'), nullable=True)
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
    
    @property
    def area_name(self):
        """Get the area name safely"""
        if self.zone_id and self.zone and self.zone.area:
            return self.zone.area.name
        return "Unassigned"
    
    @property
    def has_lead_operator(self):
        """Check if machine has a lead operator assigned"""
        return self.lead_operator_id is not None
    
    @property
    def current_user_count(self):
        """Get the number of users currently logged in"""
        return self.current_users.count()
    
    @property
    def hierarchical_id(self):
        """Get hierarchical machine ID including area and zone codes"""
        if self.zone and self.zone.area:
            return f"{self.zone.area.code}-{self.zone.code}-{self.machine_id}"
        elif self.zone:
            return f"{self.zone.code}-{self.machine_id}"
        return self.machine_id

# Machine Session model for tracking active users on machines
class MachineSession(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    machine_id = db.Column(db.Integer, db.ForeignKey('machine.id'), nullable=False)
    user_id = db.Column(db.Integer, db.ForeignKey('rfid_user.id'), nullable=False)
    is_lead = db.Column(db.Boolean, default=False)  # Whether this user is the lead operator
    login_time = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    logout_time = db.Column(db.DateTime, nullable=True)
    rework_qty = db.Column(db.Integer, default=0)  # Quantity of rework reported
    scrap_qty = db.Column(db.Integer, default=0)   # Quantity of scrap reported
    
    # Relationships
    user = db.relationship('RFIDUser', backref=db.backref('sessions', lazy='dynamic'))

# RFID User model (different from admin users)
class RFIDUser(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    rfid_tag = db.Column(db.String(64), unique=True, nullable=False)
    name = db.Column(db.String(64), nullable=False)
    email = db.Column(db.String(120))
    active = db.Column(db.Boolean, default=True)
    is_offline_access = db.Column(db.Boolean, default=False)  # For emergency offline access
    is_admin_override = db.Column(db.Boolean, default=False)  # For admin override access
    can_be_lead = db.Column(db.Boolean, default=False)  # Whether user can be assigned as lead operator
    external_id = db.Column(db.Integer)  # ID in ShopTracker system, for synchronization
    last_synced = db.Column(db.DateTime)  # Last time this user was synced with ShopTracker
    authorized_machines = db.relationship('MachineAuthorization', backref='user', lazy='dynamic')
    logs = db.relationship('MachineLog', backref='user', lazy='dynamic')
    lead_machines = db.relationship('Machine', backref='lead_operator', 
                                   foreign_keys=[Machine.lead_operator_id])
    
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
    
    def can_be_lead_on_machine(self, machine_id):
        """Check if user can be lead operator on specific machine
        
        Args:
            machine_id: Integer ID of the machine to check
            
        Returns:
            bool: True if user can be a lead operator on this machine
        """
        # First check if user can be a lead operator at all
        if not self.can_be_lead:
            return False
            
        # Admin override users can be lead on any machine
        if self.is_admin_override:
            return True
            
        # Check specific machine authorization
        auth = MachineAuthorization.query.filter_by(
            user_id=self.id, 
            machine_id=machine_id,
            can_be_lead=True
        ).first()
        
        return auth is not None
    
    def can_login_with_others(self, machine_id, current_user_count):
        """Check if user can log in with other users already on machine
        
        Args:
            machine_id: Integer ID of the machine to check
            current_user_count: Number of users currently logged in
            
        Returns:
            bool: True if user can log in with current users
        """
        # Admin override can always log in
        if self.is_admin_override:
            return True
            
        # Get machine authorization
        auth = MachineAuthorization.query.filter_by(
            user_id=self.id, 
            machine_id=machine_id
        ).first()
        
        if not auth:
            return False
            
        return auth.can_login_with_current_users(current_user_count)
        
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
    
    def get_lead_eligible_machines(self):
        """Get list of machines where user can be lead operator
        
        Returns:
            list: List of machine objects
        """
        if not self.can_be_lead:
            return []
            
        if self.is_admin_override:
            return Machine.query.all()
            
        # Get machines where this user can be lead
        return Machine.query.join(MachineAuthorization).filter(
            MachineAuthorization.user_id == self.id,
            MachineAuthorization.can_be_lead == True
        ).all()
    
    def transfer_lead_status(self, from_machine_id, to_machine_id, reason="transfer"):
        """Transfer lead status from one machine to another
        
        Args:
            from_machine_id: Machine ID to transfer from
            to_machine_id: Machine ID to transfer to  
            reason: Reason for transfer (default "transfer")
            
        Returns:
            bool: True if transfer was successful
        """
        # Verify user is currently lead on from_machine
        from_machine = Machine.query.get(from_machine_id)
        if not from_machine or from_machine.lead_operator_id != self.id:
            return False
            
        # Verify user can be lead on to_machine
        if not self.can_be_lead_on_machine(to_machine_id):
            return False
            
        # Get to_machine
        to_machine = Machine.query.get(to_machine_id)
        if not to_machine:
            return False
            
        # Close current lead history
        current_history = LeadOperatorHistory.query.filter_by(
            machine_id=from_machine_id,
            user_id=self.id,
            removed_time=None
        ).order_by(LeadOperatorHistory.assigned_time.desc()).first()
        
        if current_history:
            current_history.removed_time = datetime.datetime.utcnow()
            current_history.removal_reason = reason
        
        # If there's an existing lead on the destination machine, remove them
        if to_machine.lead_operator_id:
            # Get their session and update it
            lead_session = MachineSession.query.filter_by(
                machine_id=to_machine_id,
                user_id=to_machine.lead_operator_id,
                logout_time=None,
                is_lead=True
            ).first()
            
            if lead_session:
                lead_session.is_lead = False
            
            # Close their lead history
            lead_history = LeadOperatorHistory.query.filter_by(
                machine_id=to_machine_id,
                user_id=to_machine.lead_operator_id,
                removed_time=None
            ).order_by(LeadOperatorHistory.assigned_time.desc()).first()
            
            if lead_history:
                lead_history.removed_time = datetime.datetime.utcnow()
                lead_history.removal_reason = "override"
        
        # Update the machine lead operator
        to_machine.lead_operator_id = self.id
        
        # Create new lead history record
        new_history = LeadOperatorHistory(
            machine_id=to_machine_id,
            user_id=self.id,
            assigned_time=datetime.datetime.utcnow(),
            assigned_by_id=self.id
        )
        
        # Add the new history and get the user's session on to_machine
        db.session.add(new_history)
        
        # Find or create session on the destination machine
        session = MachineSession.query.filter_by(
            machine_id=to_machine_id,
            user_id=self.id,
            logout_time=None
        ).first()
        
        if session:
            session.is_lead = True
        else:
            # Create new session
            session = MachineSession(
                machine_id=to_machine_id,
                user_id=self.id,
                is_lead=True,
                login_time=datetime.datetime.utcnow()
            )
            db.session.add(session)
        
        # Save all changes
        db.session.commit()
        return True
    
    @property
    def active_sessions(self):
        """Get list of active sessions for this user"""
        return MachineSession.query.filter_by(
            user_id=self.id,
            logout_time=None
        ).all()
    
    @property
    def active_session_count(self):
        """Get count of active sessions for this user"""
        return MachineSession.query.filter_by(
            user_id=self.id,
            logout_time=None
        ).count()
    
    @property
    def active_lead_sessions(self):
        """Get list of active sessions where user is lead operator"""
        return MachineSession.query.filter_by(
            user_id=self.id,
            logout_time=None,
            is_lead=True
        ).all()

# Machine authorization model
class MachineAuthorization(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('rfid_user.id'), nullable=False)
    machine_id = db.Column(db.Integer, db.ForeignKey('machine.id'), nullable=False)
    can_be_lead = db.Column(db.Boolean, default=False)  # Whether user can be lead operator on this machine
    multi_user_allowed = db.Column(db.Boolean, default=True)  # Whether this user can log in when others are already logged in
    max_concurrent_users = db.Column(db.Integer, default=3)  # Maximum number of users allowed simultaneously
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    
    # Add a unique constraint to prevent duplicate authorizations
    __table_args__ = (db.UniqueConstraint('user_id', 'machine_id'),)
    
    # Relationship to machine for easier access
    machine = db.relationship('Machine', backref=db.backref('authorizations', lazy='dynamic'))
    
    def can_login_with_current_users(self, current_user_count):
        """Check if user can log in with the current number of users
        
        Args:
            current_user_count: Number of users currently logged in
            
        Returns:
            bool: True if user can log in
        """
        # If multi-user is not allowed, only can log in if no one else is on the machine
        if not self.multi_user_allowed and current_user_count > 0:
            return False
            
        # Check against max concurrent users
        return current_user_count < self.max_concurrent_users

# Lead Operator History for tracking lead changes
class LeadOperatorHistory(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    machine_id = db.Column(db.Integer, db.ForeignKey('machine.id'), nullable=False)
    user_id = db.Column(db.Integer, db.ForeignKey('rfid_user.id'), nullable=False)
    assigned_time = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    removed_time = db.Column(db.DateTime, nullable=True)
    assigned_by_id = db.Column(db.Integer, db.ForeignKey('rfid_user.id'), nullable=True)
    removal_reason = db.Column(db.String(20), nullable=True)  # logout, transfer, override, system
    
    # Relationships
    machine = db.relationship('Machine', foreign_keys=[machine_id])
    user = db.relationship('RFIDUser', foreign_keys=[user_id])
    assigned_by = db.relationship('RFIDUser', foreign_keys=[assigned_by_id])

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
    was_lead = db.Column(db.Boolean, default=False)  # Whether user was lead operator
    rework_qty = db.Column(db.Integer, default=0)  # Quantity of rework reported
    scrap_qty = db.Column(db.Integer, default=0)   # Quantity of scrap reported

# E-STOP events for tracking emergency stops
class EStopEvent(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    area_id = db.Column(db.Integer, db.ForeignKey('area.id'), nullable=True)
    zone_id = db.Column(db.Integer, db.ForeignKey('zone.id'), nullable=True)
    node_id = db.Column(db.Integer, db.ForeignKey('node.id'), nullable=True)
    trigger_time = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    reset_time = db.Column(db.DateTime, nullable=True)
    triggered_by = db.Column(db.String(64))  # Node that triggered the E-STOP
    reset_by = db.Column(db.String(64), nullable=True)  # User or node that reset the E-STOP
    affected_machines = db.Column(db.Text)  # JSON list of affected machine IDs
    
    # Relationships
    area = db.relationship('Area', backref=db.backref('estop_events', lazy='dynamic'))
    zone = db.relationship('Zone', backref=db.backref('estop_events', lazy='dynamic'))
    node = db.relationship('Node', backref=db.backref('estop_events', lazy='dynamic'))
    
    @property
    def duration_seconds(self):
        """Get duration of E-STOP in seconds"""
        if not self.reset_time:
            return (datetime.datetime.utcnow() - self.trigger_time).total_seconds()
        return (self.reset_time - self.trigger_time).total_seconds()
    
    @property
    def is_active(self):
        """Check if E-STOP is still active"""
        return self.reset_time is None

# Alert system for bidirectional communication with ShopTracker
class Alert(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    external_id = db.Column(db.Integer, nullable=True)  # ID from ShopTracker
    area_id = db.Column(db.Integer, db.ForeignKey('area.id'), nullable=True)  # New field for area
    machine_id = db.Column(db.Integer, db.ForeignKey('machine.id'), nullable=True)
    node_id = db.Column(db.Integer, db.ForeignKey('node.id'), nullable=True)
    user_id = db.Column(db.Integer, db.ForeignKey('rfid_user.id'), nullable=True)
    message = db.Column(db.String(255), nullable=False)
    alert_type = db.Column(db.String(20), nullable=False)  # warning, error, info, maintenance, estop
    status = db.Column(db.String(20), default="pending")  # pending, acknowledged, resolved
    origin = db.Column(db.String(20), nullable=False)  # machine, user, system, Shop_tracker
    created_at = db.Column(db.DateTime, default=datetime.datetime.utcnow)
    acknowledged_at = db.Column(db.DateTime, nullable=True)
    resolved_at = db.Column(db.DateTime, nullable=True)
    acknowledged_by = db.Column(db.Integer, db.ForeignKey('user.id'), nullable=True)
    resolved_by = db.Column(db.Integer, db.ForeignKey('user.id'), nullable=True)
    
    # Relationships
    area = db.relationship('Area', backref=db.backref('alerts', lazy='dynamic'))
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
            'area_id': self.area_id,
            'area_name': self.area.name if self.area else None,
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

# Bluetooth Audio Device for CYD nodes
class BluetoothDevice(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    node_id = db.Column(db.Integer, db.ForeignKey('node.id'), nullable=False)
    device_name = db.Column(db.String(64), nullable=False)
    mac_address = db.Column(db.String(17), unique=True, nullable=False)
    last_connected = db.Column(db.DateTime, nullable=True)
    is_paired = db.Column(db.Boolean, default=False)
    status = db.Column(db.String(20), default="disconnected")  # connected, disconnected, pairing
    
    # Relationships
    node = db.relationship('Node', backref=db.backref('bluetooth_devices', lazy='dynamic'))
