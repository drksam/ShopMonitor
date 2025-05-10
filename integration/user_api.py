"""
User API module for ShopMonitor integration
This module provides API endpoints for user synchronization
"""
import json
from flask import Blueprint, jsonify, request, current_app
from flask_login import login_required, current_user
from app import db
from models import RFIDUser, Machine, MachineAuthorization, Zone
from .shop_integration import integration
from error_handling import handle_api_errors, exponential_backoff, AppError, logger

# Create a blueprint for the API
user_api_bp = Blueprint('user_api', __name__, url_prefix='/integration/user_api')

@user_api_bp.route('/available', methods=['GET'])
@login_required
@handle_api_errors
def get_available_users():
    """
    Get available users from ShopMonitor that can be imported
    """
    # Check if user is admin
    if not current_user.role or current_user.role.name.lower() != 'admin':
        raise AppError(code=2003, message="Permission denied. Admin role required.")
    
    # Check if integration is configured
    api_url = current_app.config.get('SHOP_API_BASE_URL') or current_app.config.get('Shop_API_BASE_URL', '')
    api_key = current_app.config.get('SHOP_API_KEY') or current_app.config.get('Shop_API_KEY', '')
    
    if not api_url or not api_key:
        raise AppError(code=5001, message="ShopMonitor integration is not configured.")
    
    # Fetch users from ShopMonitor with exponential backoff
    shop_users = fetch_external_users()
    
    # Get existing RFID tags in our system
    existing_rfid_tags = set(u.rfid_tag for u in RFIDUser.query.all())
    
    # Filter out users that already exist in our system
    available_users = [
        user for user in shop_users 
        if user.get('rfid_tag') and user.get('rfid_tag') not in existing_rfid_tags
    ]
    
    return jsonify({
        'success': True,
        'users': available_users
    })

@user_api_bp.route('/import', methods=['POST'])
@login_required
@handle_api_errors
def import_user():
    """
    Import a user from ShopMonitor
    """
    # Check if user is admin
    if not current_user.role or current_user.role.name.lower() != 'admin':
        raise AppError(code=2003, message="Permission denied. Admin role required.")
    
    # Check if integration is configured
    api_url = current_app.config.get('SHOP_API_BASE_URL') or current_app.config.get('Shop_API_BASE_URL', '')
    api_key = current_app.config.get('SHOP_API_KEY') or current_app.config.get('Shop_API_KEY', '')
    
    if not api_url or not api_key:
        raise AppError(code=5001, message="ShopMonitor integration is not configured.")
    
    # Get external user ID from request
    data = request.get_json()
    if not data or 'external_id' not in data:
        raise AppError(code=5001, message="External user ID is required.")
    
    external_id = data['external_id']
    
    try:
        # Get user details from ShopMonitor with exponential backoff
        user_data = get_external_user_by_id(external_id)
        
        if not user_data:
            raise AppError(code=5001, message=f'User with ID {external_id} not found in ShopMonitor.')
        
        # Check if user already exists
        existing_user = RFIDUser.query.filter_by(rfid_tag=user_data.get('rfid_tag')).first()
        if existing_user:
            raise AppError(code=5002, message=f'A user with RFID tag {user_data.get("rfid_tag")} already exists.')
        
        # Create new user
        new_user = RFIDUser(
            rfid_tag=user_data.get('rfid_tag'),
            name=user_data.get('name'),
            email=user_data.get('email', ''),
            active=user_data.get('is_active', True),
            external_id=external_id  # Store the external ID for future reference
        )
        
        db.session.add(new_user)
        db.session.commit()
        
        # Import machine permissions if available
        if 'permissions' in user_data and user_data['permissions']:
            import_permissions(new_user, user_data['permissions'])
        
        logger.info(f"User {user_data.get('name')} (RFID: {user_data.get('rfid_tag')}) imported successfully")
        
        return jsonify({
            'success': True,
            'message': f'User {user_data.get("name")} imported successfully',
            'user': {
                'id': new_user.id,
                'name': new_user.name,
                'rfid_tag': new_user.rfid_tag
            }
        })
    
    except AppError as e:
        db.session.rollback()
        raise
    except Exception as e:
        db.session.rollback()
        logger.error(f"Error importing user: {str(e)}")
        raise AppError(code=5000, message=f"Error importing user: {str(e)}")

@user_api_bp.route('/update_permissions/<int:user_id>', methods=['POST'])
@login_required
@handle_api_errors
def update_permissions(user_id):
    """
    Update a user's machine permissions and sync with ShopMonitor
    """
    # Check if user is admin
    if not current_user.role or current_user.role.name.lower() != 'admin':
        raise AppError(code=2003, message="Permission denied. Admin role required.")
    
    # Get the user
    user = RFIDUser.query.get(user_id)
    if not user:
        raise AppError(code=5001, message=f"User with ID {user_id} not found.")
    
    # Get data from request
    data = request.get_json()
    if not data or 'machine_ids' not in data:
        raise AppError(code=5001, message="Machine IDs are required.")
    
    machine_ids = data['machine_ids']
    
    try:
        # Update local permissions
        # First, remove all existing authorizations
        MachineAuthorization.query.filter_by(user_id=user.id).delete()
        
        # Add new authorizations
        for machine_id in machine_ids:
            machine = Machine.query.get(machine_id)
            if machine:
                auth = MachineAuthorization(user_id=user.id, machine_id=machine.id)
                db.session.add(auth)
        
        db.session.commit()
        
        # Sync with ShopMonitor if integration is configured
        api_url = current_app.config.get('SHOP_API_BASE_URL') or current_app.config.get('Shop_API_BASE_URL', '')
        api_key = current_app.config.get('SHOP_API_KEY') or current_app.config.get('Shop_API_KEY', '')
        
        if api_url and api_key and hasattr(user, 'external_id') and user.external_id:
            # Get machine IDs in the format expected by ShopMonitor
            machine_external_ids = []
            for machine_id in machine_ids:
                machine = Machine.query.get(machine_id)
                if machine and machine.machine_id:  # machine_id is the external ID/code like "W1"
                    machine_external_ids.append(machine.machine_id)
            
            # Send to ShopMonitor with exponential backoff
            success = sync_external_permissions(user.external_id, machine_external_ids)
            
            if not success:
                logger.warning(f"Permissions updated locally but failed to sync with ShopMonitor for user {user.id}")
                return jsonify({
                    'success': True,
                    'warning': 'Permissions updated locally but failed to sync with ShopMonitor.'
                })
        
        logger.info(f"Permissions updated successfully for user {user.id}")
        return jsonify({
            'success': True,
            'message': 'Permissions updated successfully'
        })
    
    except AppError as e:
        db.session.rollback()
        raise
    except Exception as e:
        db.session.rollback()
        logger.error(f"Error updating permissions: {str(e)}")
        raise AppError(code=5000, message=f"Error updating permissions: {str(e)}")

def import_permissions(user, permissions):
    """
    Import machine permissions for a user
    
    Args:
        user: The RFIDUser object
        permissions: List of machine IDs (external IDs like "W1")
    """
    for machine_id in permissions:
        machine = Machine.query.filter_by(machine_id=machine_id).first()
        if machine:
            auth = MachineAuthorization(user_id=user.id, machine_id=machine.id)
            db.session.add(auth)
    
    db.session.commit()

# Functions with exponential backoff retry logic
@exponential_backoff(max_retries=5, base_delay=0.5)
def fetch_external_users():
    """Fetch users from external system with retry logic"""
    try:
        return integration.get_available_users()
    except Exception as e:
        logger.error(f"Error fetching external users: {str(e)}")
        raise AppError(code=1000, message=f"Network error: {str(e)}")

@exponential_backoff(max_retries=5, base_delay=0.5)
def get_external_user_by_id(external_id):
    """Get user details from external system with retry logic"""
    try:
        return integration.get_user_by_id(external_id)
    except Exception as e:
        logger.error(f"Error fetching external user {external_id}: {str(e)}")
        raise AppError(code=1000, message=f"Network error: {str(e)}")

@exponential_backoff(max_retries=5, base_delay=0.5)
def sync_external_permissions(user_external_id, machine_ids):
    """Sync permissions with external system with retry logic"""
    try:
        return integration.update_user_permissions(user_external_id, machine_ids)
    except Exception as e:
        logger.error(f"Error syncing permissions for user {user_external_id}: {str(e)}")
        raise AppError(code=1000, message=f"Network error: {str(e)}")