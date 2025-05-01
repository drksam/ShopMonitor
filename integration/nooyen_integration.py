"""
Integration module for NooyenUSATracker API
Handles data synchronization between the RFID Access Control system and the main tracker app
"""

import os
import json
import logging
import requests
from datetime import datetime, timedelta
import threading
import time
from flask import current_app
from models import RFIDUser, Zone, Machine, MachineAuthorization, db

# Configuration
NOOYEN_API_BASE_URL = os.environ.get('NOOYEN_API_BASE_URL')
NOOYEN_API_KEY = os.environ.get('NOOYEN_API_KEY')
SYNC_INTERVAL = int(os.environ.get('SYNC_INTERVAL', 3600))  # Default: 1 hour

# Logging setup
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# API endpoints
USERS_ENDPOINT = "/api/users"
LOCATIONS_ENDPOINT = "/api/locations"
MACHINES_ENDPOINT = "/api/machines"
USER_PERMISSIONS_ENDPOINT = "/api/user_permissions"
PUSH_ENDPOINT = "/api/push_changes"


class NooyenIntegration:
    """
    Integration class for NooyenUSATracker
    Handles data synchronization between the two systems
    """

    def __init__(self, app=None):
        self.app = app
        self.last_sync = None
        self.sync_thread = None
        self.stop_sync = False
        
        if app is not None:
            self.init_app(app)
    
    def init_app(self, app):
        """Initialize with Flask app context"""
        self.app = app
        
        # Add config values to app
        app.config.setdefault('NOOYEN_API_BASE_URL', NOOYEN_API_BASE_URL)
        app.config.setdefault('NOOYEN_API_KEY', NOOYEN_API_KEY)
        app.config.setdefault('SYNC_INTERVAL', SYNC_INTERVAL)
        
        # Start background sync if config is available
        if app.config['NOOYEN_API_BASE_URL'] and app.config['NOOYEN_API_KEY']:
            self.start_background_sync()
    
    def api_headers(self):
        """Get API headers with authentication"""
        return {
            'Authorization': f'Bearer {current_app.config["NOOYEN_API_KEY"]}',
            'Content-Type': 'application/json',
            'Accept': 'application/json'
        }
    
    def get_api_url(self, endpoint):
        """Construct full API URL"""
        base_url = current_app.config['NOOYEN_API_BASE_URL'].rstrip('/')
        return f"{base_url}{endpoint}"
    
    def fetch_data(self, endpoint):
        """Fetch data from NooyenUSATracker API"""
        try:
            url = self.get_api_url(endpoint)
            response = requests.get(url, headers=self.api_headers(), timeout=30)
            
            if response.status_code == 200:
                return response.json()
            else:
                logger.error(f"API request failed: {response.status_code} - {response.text}")
                return None
        except Exception as e:
            logger.exception(f"Error fetching data from {endpoint}: {str(e)}")
            return None
    
    def sync_users(self):
        """Synchronize users from NooyenUSATracker to RFID system"""
        with self.app.app_context():
            logger.info("Synchronizing users from NooyenUSATracker...")
            users_data = self.fetch_data(USERS_ENDPOINT)
            
            if not users_data:
                logger.warning("No user data received from NooyenUSATracker")
                return False
            
            # Track processed users for cleanup
            processed_users = []
            
            for user_data in users_data:
                # Extract user details
                external_id = user_data.get('id')
                rfid_tag = user_data.get('rfid_tag')
                name = user_data.get('name')
                email = user_data.get('email')
                is_active = user_data.get('is_active', True)
                
                # Skip users without RFID tags
                if not rfid_tag:
                    continue
                
                processed_users.append(rfid_tag)
                
                # Check if user exists
                user = RFIDUser.query.filter_by(rfid_tag=rfid_tag).first()
                
                if user:
                    # Update existing user
                    user.name = name
                    user.email = email
                    user.active = is_active
                    logger.info(f"Updated user: {name} ({rfid_tag})")
                else:
                    # Create new user
                    new_user = RFIDUser(
                        rfid_tag=rfid_tag,
                        name=name,
                        email=email,
                        active=is_active
                    )
                    db.session.add(new_user)
                    logger.info(f"Created new user: {name} ({rfid_tag})")
            
            # Commit changes
            db.session.commit()
            logger.info(f"User synchronization complete. Processed {len(processed_users)} users.")
            return True
    
    def sync_locations(self):
        """Synchronize locations (zones) from NooyenUSATracker"""
        with self.app.app_context():
            logger.info("Synchronizing locations from NooyenUSATracker...")
            locations_data = self.fetch_data(LOCATIONS_ENDPOINT)
            
            if not locations_data:
                logger.warning("No location data received from NooyenUSATracker")
                return False
            
            # Track processed locations for cleanup
            processed_locations = []
            
            for location_data in locations_data:
                # Extract location details
                external_id = location_data.get('id')
                name = location_data.get('name')
                description = location_data.get('description', '')
                
                processed_locations.append(external_id)
                
                # Check if location exists
                zone = Zone.query.filter_by(name=name).first()
                
                if zone:
                    # Update existing zone
                    zone.description = description
                    logger.info(f"Updated location: {name}")
                else:
                    # Create new zone
                    new_zone = Zone(
                        name=name,
                        description=description
                    )
                    db.session.add(new_zone)
                    logger.info(f"Created new location: {name}")
            
            # Commit changes
            db.session.commit()
            logger.info(f"Location synchronization complete. Processed {len(processed_locations)} locations.")
            return True
    
    def sync_machines(self):
        """Synchronize machines from NooyenUSATracker"""
        with self.app.app_context():
            logger.info("Synchronizing machines from NooyenUSATracker...")
            machines_data = self.fetch_data(MACHINES_ENDPOINT)
            
            if not machines_data:
                logger.warning("No machine data received from NooyenUSATracker")
                return False
            
            # Track processed machines for cleanup
            processed_machines = []
            
            for machine_data in machines_data:
                # Extract machine details
                external_id = machine_data.get('id')
                name = machine_data.get('name')
                machine_id = machine_data.get('machine_id')  # Two-digit ID for Arduino
                description = machine_data.get('description', '')
                location_name = machine_data.get('location')
                
                # Skip machines without proper IDs
                if not machine_id or len(machine_id) != 2:
                    logger.warning(f"Skipping machine {name} - invalid machine_id: {machine_id}")
                    continue
                
                processed_machines.append(machine_id)
                
                # Find zone by name
                zone = Zone.query.filter_by(name=location_name).first()
                
                if not zone:
                    logger.warning(f"Location '{location_name}' not found for machine {name}. Skipping.")
                    continue
                
                # Check if machine exists
                machine = Machine.query.filter_by(machine_id=machine_id).first()
                
                if machine:
                    # Update existing machine
                    machine.name = name
                    machine.description = description
                    machine.zone_id = zone.id
                    logger.info(f"Updated machine: {name} (ID: {machine_id})")
                else:
                    # Create new machine
                    new_machine = Machine(
                        machine_id=machine_id,
                        name=name,
                        description=description,
                        zone_id=zone.id
                    )
                    db.session.add(new_machine)
                    logger.info(f"Created new machine: {name} (ID: {machine_id})")
            
            # Commit changes
            db.session.commit()
            logger.info(f"Machine synchronization complete. Processed {len(processed_machines)} machines.")
            return True
    
    def sync_all(self):
        """Synchronize all data from NooyenUSATracker"""
        success = True
        
        # Order matters: zones first, then machines, then users
        if not self.sync_locations():
            success = False
        
        if not self.sync_machines():
            success = False
        
        if not self.sync_users():
            success = False
            
        # Sync user permissions as well
        if not self.sync_user_permissions():
            success = False
        
        self.last_sync = datetime.now()
        return success
    
    def background_sync_worker(self):
        """Background worker to periodically sync data"""
        logger.info("Starting background sync worker")
        
        while not self.stop_sync:
            try:
                with self.app.app_context():
                    self.sync_all()
            except Exception as e:
                logger.exception(f"Error in background sync: {str(e)}")
            
            # Sleep for the configured interval
            for _ in range(current_app.config['SYNC_INTERVAL']):
                if self.stop_sync:
                    break
                time.sleep(1)
    
    def start_background_sync(self):
        """Start background synchronization thread"""
        if not self.sync_thread or not self.sync_thread.is_alive():
            self.stop_sync = False
            self.sync_thread = threading.Thread(target=self.background_sync_worker)
            self.sync_thread.daemon = True
            self.sync_thread.start()
            logger.info("Background sync started")
    
    def stop_background_sync(self):
        """Stop background synchronization thread"""
        if self.sync_thread and self.sync_thread.is_alive():
            self.stop_sync = True
            self.sync_thread.join(timeout=10)
            logger.info("Background sync stopped")


    def sync_user_permissions(self):
        """Synchronize user machine permissions"""
        with self.app.app_context():
            logger.info("Synchronizing user permissions from NooyenUSATracker...")
            permissions_data = self.fetch_data(USER_PERMISSIONS_ENDPOINT)
            
            if not permissions_data:
                logger.warning("No user permissions data received from NooyenUSATracker")
                return False
                
            # MachineAuthorization is already imported at the top
            
            # Process permissions data
            for perm_data in permissions_data:
                rfid_tag = perm_data.get('rfid_tag')
                machine_id = perm_data.get('machine_id')
                
                # Skip if missing required data
                if not rfid_tag or not machine_id:
                    continue
                    
                # Find user and machine
                user = RFIDUser.query.filter_by(rfid_tag=rfid_tag).first()
                machine = Machine.query.filter_by(machine_id=machine_id).first()
                
                if not user or not machine:
                    continue
                    
                # Check if authorization already exists
                auth = MachineAuthorization.query.filter_by(
                    user_id=user.id, machine_id=machine.id).first()
                    
                if not auth:
                    # Create new authorization
                    new_auth = MachineAuthorization(
                        user_id=user.id,
                        machine_id=machine.id
                    )
                    db.session.add(new_auth)
                    logger.info(f"Created new permission: {user.name} -> {machine.name}")
            
            # Commit changes
            db.session.commit()
            logger.info("User permissions synchronization complete")
            return True
            
    def push_local_changes(self, changes):
        """Push local changes back to NooyenUSATracker"""
        try:
            url = self.get_api_url(PUSH_ENDPOINT)
            payload = json.dumps(changes)
            
            response = requests.post(
                url, 
                headers=self.api_headers(), 
                data=payload,
                timeout=30
            )
            
            if response.status_code == 200:
                logger.info("Successfully pushed local changes to NooyenUSATracker")
                return True
            else:
                logger.error(f"Failed to push changes: {response.status_code} - {response.text}")
                return False
        except Exception as e:
            logger.exception(f"Error pushing changes: {str(e)}")
            return False
    
    def push_user_changes(self, user):
        """Push RFID user changes to NooyenUSATracker"""
        if not user:
            return False
            
        changes = {
            'type': 'user_update',
            'data': {
                'rfid_tag': user.rfid_tag,
                'name': user.name,
                'email': user.email,
                'active': user.active
            }
        }
        
        return self.push_local_changes(changes)
        
    def push_authorization_changes(self, user_id):
        """Push user authorization changes to NooyenUSATracker"""
        from models import MachineAuthorization
        
        user = RFIDUser.query.get(user_id)
        if not user:
            return False
            
        # Get all machine authorizations for this user
        authorizations = MachineAuthorization.query.filter_by(user_id=user_id).all()
        machine_ids = []
        
        for auth in authorizations:
            machine = Machine.query.get(auth.machine_id)
            if machine:
                machine_ids.append(machine.machine_id)
                
        changes = {
            'type': 'permission_update',
            'data': {
                'rfid_tag': user.rfid_tag,
                'machine_ids': machine_ids
            }
        }
        
        return self.push_local_changes(changes)
            
    def fetch_available_users(self):
        """Fetch available users from NooyenUSATracker without creating them locally
        This allows displaying users that don't yet exist in our system
        """
        logger.info("Fetching available users from NooyenUSATracker")
        
        try:
            users_data = self.fetch_data(USERS_ENDPOINT)
            
            if not users_data:
                logger.warning("No user data received from NooyenUSATracker")
                return []
            
            # Track which users are already in our system
            existing_rfid_tags = {user.rfid_tag for user in RFIDUser.query.all()}
            
            # Format user data for display
            available_users = []
            
            for user_data in users_data:
                rfid_tag = user_data.get('rfid_tag')
                # Skip users without RFID tags
                if not rfid_tag:
                    continue
                    
                # Only include users that aren't already imported
                if rfid_tag not in existing_rfid_tags:
                    available_users.append({
                        'external_id': user_data.get('id'),
                        'rfid_tag': rfid_tag,
                        'name': user_data.get('name'),
                        'email': user_data.get('email', ''),
                        'is_active': user_data.get('is_active', True)
                    })
                
            return available_users
            
        except Exception as e:
            logger.error(f"Error fetching available users: {str(e)}")
            return []
            
    def fetch_user_by_id(self, external_id):
        """Fetch a specific user by external ID from NooyenUSATracker"""
        logger.info(f"Fetching user with ID {external_id} from NooyenUSATracker")
        
        try:
            # User detail endpoint
            user_endpoint = f"{USERS_ENDPOINT}/{external_id}"
            user_data = self.fetch_data(user_endpoint)
            
            if not user_data:
                logger.warning(f"User with ID {external_id} not found")
                return None
                
            # Also fetch user's permissions
            permissions_endpoint = f"{USER_PERMISSIONS_ENDPOINT}/{external_id}"
            permissions_data = self.fetch_data(permissions_endpoint)
            
            # Combine user data with permissions
            if permissions_data and isinstance(permissions_data, dict):
                user_data['machine_ids'] = permissions_data.get('machine_ids', [])
                
            return user_data
            
        except Exception as e:
            logger.error(f"Error fetching user by ID: {str(e)}")
            return None


# Initialize integration
integration = NooyenIntegration()

def manual_sync():
    """Manually trigger synchronization"""
    with current_app.app_context():
        return integration.sync_all()

def push_user_changes(user):
    """Push user changes back to NooyenUSATracker"""
    with current_app.app_context():
        return integration.push_user_changes(user)
        
def push_authorization_changes(user_id):
    """Push user's machine authorizations back to NooyenUSATracker"""
    with current_app.app_context():
        return integration.push_authorization_changes(user_id)

# Endpoint for manually triggering sync
def get_last_sync_time():
    """Get the timestamp of the last successful synchronization"""
    if integration.last_sync:
        return integration.last_sync.strftime("%Y-%m-%d %H:%M:%S")
    return "Never"

def fetch_available_users():
    """Fetch available users from NooyenUSATracker without saving them
    This is used to display potential users for import in the UI
    """
    with current_app.app_context():
        return integration.fetch_available_users()

def fetch_user_by_id(external_id):
    """Fetch a specific user by external ID from NooyenUSATracker
    This is used to import a single user to our system
    """
    with current_app.app_context():
        return integration.fetch_user_by_id(external_id)

def get_available_users():
    """Get users from NooyenUSATracker that aren't in our system yet
    This is a convenient wrapper that returns properly formatted user data
    """
    return fetch_available_users()

def get_user_by_id(external_id):
    """Get a specific user by external ID from NooyenUSATracker
    This is a convenient wrapper that returns properly formatted user data
    """
    return fetch_user_by_id(external_id)

def update_user_permissions(external_id, machine_ids):
    """Update a user's machine permissions in NooyenUSATracker
    
    Args:
        external_id: The ID of the user in NooyenUSATracker
        machine_ids: List of machine IDs (external IDs like "W1")
        
    Returns:
        bool: True if successful, False otherwise
    """
    # Create data payload
    payload = {
        'type': 'permission_update',
        'data': {
            'external_id': external_id,
            'machine_ids': machine_ids
        }
    }
    
    # Push changes
    with current_app.app_context():
        return integration.push_local_changes(payload)