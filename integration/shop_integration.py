"""
Shop Suite Integration Module
---------------------------
Handles synchronization between ShopMonitor and ShopTracker applications
"""

import datetime
import json
import logging
import requests
import time
from threading import Thread

from flask import current_app, g, request
from sqlalchemy import and_, or_

from app import db
from models import User, RFIDUser, MachineAuthorization, Machine
from shop_suite.models import SuiteUser, SyncEvent, UserSuiteMapping, RFIDSuiteMapping

# Configure module logger
logger = logging.getLogger(__name__)


class ShopTrackerSync:
    """
    Handles synchronization of data between ShopMonitor and ShopTracker
    """
    
    def __init__(self, app=None):
        """Initialize with Flask app"""
        self.app = app
        self._sync_thread = None
        self._stop_sync = False
        
        if app is not None:
            self.init_app(app)
            
    def init_app(self, app):
        """Initialize with Flask app"""
        self.app = app
        
        # Register event hooks
        app.after_request(self._capture_data_changes)
        
        # Start background sync thread if configured
        if app.config.get('SHOP_TRACKER_API_BASE_URL'):
            self.start_sync_thread()
            
    def start_sync_thread(self):
        """Start background synchronization thread"""
        if self._sync_thread is None or not self._sync_thread.is_alive():
            self._stop_sync = False
            self._sync_thread = Thread(target=self._sync_worker, daemon=True)
            self._sync_thread.start()
            logger.info("Started ShopTracker sync thread")
            
    def stop_sync_thread(self):
        """Stop background synchronization thread"""
        if self._sync_thread and self._sync_thread.is_alive():
            self._stop_sync = True
            self._sync_thread.join(timeout=5.0)
            logger.info("Stopped ShopTracker sync thread")
            
    def _sync_worker(self):
        """Background worker to process sync events"""
        with self.app.app_context():
            logger.info("Sync worker started")
            
            while not self._stop_sync:
                try:
                    # Process pending sync events
                    self._process_pending_events()
                    
                    # Sleep for a short interval before next check
                    sync_interval = current_app.config.get('SYNC_INTERVAL', 60)
                    for _ in range(min(sync_interval, 60)):
                        if self._stop_sync:
                            break
                        time.sleep(1)
                        
                except Exception as e:
                    logger.error(f"Error in sync worker: {str(e)}")
                    time.sleep(10)  # Sleep before retry on error
                    
    def _process_pending_events(self):
        """Process pending sync events"""
        # Find pending events targeted for ShopTracker
        events = SyncEvent.query.filter_by(
            target_app='shop_tracker',
            status='pending'
        ).order_by(SyncEvent.created_at).limit(10).all()
        
        if not events:
            return
            
        logger.info(f"Processing {len(events)} pending sync events")
        
        for event in events:
            try:
                self._process_event(event)
            except Exception as e:
                logger.error(f"Error processing event {event.id}: {str(e)}")
                event.mark_failed(str(e))
                
    def _process_event(self, event):
        """Process a single sync event"""
        logger.info(f"Processing sync event {event.id}: {event.event_type} for {event.resource_type}/{event.resource_id}")
        
        # Get API URL and key from config
        api_url = current_app.config.get('SHOP_TRACKER_API_BASE_URL')
        api_key = current_app.config.get('SHOP_TRACKER_API_KEY')
        
        if not api_url:
            event.mark_failed("ShopTracker API URL not configured")
            return
            
        # Handle different event types
        if event.event_type == 'user.created' or event.event_type == 'user.updated':
            self._sync_user(event, api_url, api_key)
            
        elif event.event_type == 'authorization.updated':
            self._sync_authorization(event, api_url, api_key)
            
        elif event.event_type == 'machine.created' or event.event_type == 'machine.updated':
            self._sync_machine(event, api_url, api_key)
            
        else:
            event.mark_failed(f"Unknown event type: {event.event_type}")
            
    def _sync_user(self, event, api_url, api_key):
        """Sync user data to ShopTracker"""
        payload = event.get_payload()
        user_id = event.resource_id
        
        # Get the RFID user
        rfid_user = RFIDUser.query.get(user_id)
        if not rfid_user:
            event.mark_failed(f"User {user_id} not found")
            return
            
        # Prepare data for API
        user_data = {
            'rfid_tag': rfid_user.rfid_tag,
            'name': rfid_user.name,
            'email': rfid_user.email,
            'active': rfid_user.active,
            'is_offline_access': rfid_user.is_offline_access,
            'is_admin_override': rfid_user.is_admin_override,
            'source_id': rfid_user.id
        }
        
        # Add associated suite user if exists
        if rfid_user.suite_user:
            user_data['suite_user_id'] = rfid_user.suite_user.id
            
        # Send to ShopTracker API
        try:
            response = requests.post(
                f"{api_url.rstrip('/')}/api/sync/users",
                json=user_data,
                headers={
                    'Authorization': f'Bearer {api_key}',
                    'Content-Type': 'application/json'
                },
                timeout=10
            )
            
            if response.status_code != 200:
                event.mark_failed(f"API error: {response.status_code} - {response.text}")
                return
                
            result = response.json()
            if result.get('success'):
                event.mark_processed()
                logger.info(f"Successfully synced user {user_id} to ShopTracker")
            else:
                event.mark_failed(f"API error: {result.get('message', 'Unknown error')}")
                
        except requests.RequestException as e:
            event.mark_failed(f"Request error: {str(e)}")
            
    def _sync_authorization(self, event, api_url, api_key):
        """Sync machine authorizations to ShopTracker"""
        payload = event.get_payload()
        user_id = event.resource_id
        
        # Get user's authorizations
        authorizations = MachineAuthorization.query.filter_by(user_id=user_id).all()
        authorized_machine_ids = [auth.machine_id for auth in authorizations]
        
        # Get corresponding machines
        machines = Machine.query.filter(Machine.id.in_(authorized_machine_ids)).all()
        machine_data = [{
            'machine_id': m.machine_id,
            'name': m.name,
            'id': m.id
        } for m in machines]
        
        # Prepare data for API
        auth_data = {
            'user_id': user_id,
            'machines': machine_data
        }
        
        # Send to ShopTracker API
        try:
            response = requests.post(
                f"{api_url.rstrip('/')}/api/sync/authorizations",
                json=auth_data,
                headers={
                    'Authorization': f'Bearer {api_key}',
                    'Content-Type': 'application/json'
                },
                timeout=10
            )
            
            if response.status_code != 200:
                event.mark_failed(f"API error: {response.status_code} - {response.text}")
                return
                
            result = response.json()
            if result.get('success'):
                event.mark_processed()
                logger.info(f"Successfully synced authorizations for user {user_id} to ShopTracker")
            else:
                event.mark_failed(f"API error: {result.get('message', 'Unknown error')}")
                
        except requests.RequestException as e:
            event.mark_failed(f"Request error: {str(e)}")
            
    def _sync_machine(self, event, api_url, api_key):
        """Sync machine data to ShopTracker"""
        payload = event.get_payload()
        machine_id = event.resource_id
        
        # Get the machine
        machine = Machine.query.get(machine_id)
        if not machine:
            event.mark_failed(f"Machine {machine_id} not found")
            return
            
        # Prepare data for API
        machine_data = {
            'id': machine.id,
            'machine_id': machine.machine_id,
            'name': machine.name,
            'description': machine.description,
            'zone_id': machine.zone_id,
            'zone_name': machine.zone.name if machine.zone else None,
            'status': machine.status,
            'source_id': machine.id
        }
        
        # Send to ShopTracker API
        try:
            response = requests.post(
                f"{api_url.rstrip('/')}/api/sync/machines",
                json=machine_data,
                headers={
                    'Authorization': f'Bearer {api_key}',
                    'Content-Type': 'application/json'
                },
                timeout=10
            )
            
            if response.status_code != 200:
                event.mark_failed(f"API error: {response.status_code} - {response.text}")
                return
                
            result = response.json()
            if result.get('success'):
                event.mark_processed()
                logger.info(f"Successfully synced machine {machine_id} to ShopTracker")
            else:
                event.mark_failed(f"API error: {result.get('message', 'Unknown error')}")
                
        except requests.RequestException as e:
            event.mark_failed(f"Request error: {str(e)}")
    
    def _capture_data_changes(self, response):
        """After request handler to capture data changes"""
        # Only run for successful write operations
        if request.method not in ('POST', 'PUT', 'DELETE'):
            return response
            
        # Check request path
        if 'users' in request.path and request.method in ('POST', 'PUT'):
            self._capture_user_change()
        elif 'authorizations' in request.path and request.method == 'POST':
            user_id = request.view_args.get('user_id')
            if user_id:
                self._create_sync_event('authorization.updated', 'user', user_id)
        elif 'machines' in request.path and 'add' in request.path and request.method == 'POST':
            self._capture_machine_change()
            
        return response
        
    def _capture_user_change(self):
        """Capture user data changes"""
        # This is a simplification - in a real app we'd need more sophisticated change tracking
        try:
            user_id = request.form.get('id')
            rfid_tag = request.form.get('rfid_tag')
            
            if not user_id and rfid_tag:
                # Find by RFID tag (for new users)
                user = RFIDUser.query.filter_by(rfid_tag=rfid_tag).first()
                if user:
                    user_id = user.id
                    
            if user_id:
                event_type = 'user.updated'
                self._create_sync_event(event_type, 'user', user_id)
        except Exception as e:
            logger.error(f"Error capturing user change: {str(e)}")
            
    def _capture_machine_change(self):
        """Capture machine data changes"""
        try:
            machine_id = request.form.get('machine_id')
            if machine_id:
                machine = Machine.query.filter_by(machine_id=machine_id).first()
                if machine:
                    event_type = 'machine.updated'
                    self._create_sync_event(event_type, 'machine', machine.id)
        except Exception as e:
            logger.error(f"Error capturing machine change: {str(e)}")
            
    def _create_sync_event(self, event_type, resource_type, resource_id, payload=None):
        """Create a sync event"""
        try:
            event = SyncEvent(
                event_type=event_type,
                resource_type=resource_type,
                resource_id=resource_id,
                source_app='shop_monitor',
                target_app='shop_tracker',
                status='pending'
            )
            
            if payload:
                event.set_payload(payload)
                
            db.session.add(event)
            db.session.commit()
            logger.info(f"Created sync event: {event_type} for {resource_type}/{resource_id}")
            
        except Exception as e:
            db.session.rollback()
            logger.error(f"Error creating sync event: {str(e)}")

# Helper functions for API use
def create_user_sync_event(user_id, event_type='user.updated'):
    """Create event to sync a user to ShopTracker"""
    sync_event = SyncEvent(
        event_type=event_type,
        resource_type='user',
        resource_id=user_id,
        source_app='shop_monitor',
        target_app='shop_tracker',
        status='pending'
    )
    db.session.add(sync_event)
    db.session.commit()
    
def push_user_changes(user):
    """Push RFID user changes to ShopTracker"""
    create_user_sync_event(user.id)
    
def push_authorization_changes(user_id):
    """Push authorization changes to ShopTracker"""
    sync_event = SyncEvent(
        event_type='authorization.updated',
        resource_type='user',
        resource_id=user_id,
        source_app='shop_monitor',
        target_app='shop_tracker',
        status='pending'
    )
    db.session.add(sync_event)
    db.session.commit()
    
def get_last_sync_time():
    """Get the last successful sync time with ShopTracker"""
    last_sync = SyncEvent.query.filter(
        SyncEvent.status == 'processed',
        SyncEvent.target_app == 'shop_tracker'
    ).order_by(SyncEvent.processed_at.desc()).first()
    
    if last_sync and last_sync.processed_at:
        return last_sync.processed_at.strftime('%Y-%m-%d %H:%M:%S')
    
    return "Never"