"""
User API module for NooyenUSATracker integration
This module provides API endpoints for user synchronization
"""
import json
from flask import Blueprint, jsonify, request, current_app
from flask_login import login_required, current_user
from app import db
from models import RFIDUser, Machine, MachineAuthorization, Zone
from .nooyen_integration import integration

# Create a blueprint for the API
user_api_bp = Blueprint('user_api', __name__, url_prefix='/integration/user_api')

@user_api_bp.route('/available', methods=['GET'])
@login_required
def get_available_users():
    """
    Get available users from NooyenUSATracker that can be imported
    """
    # Check if user is admin
    if not current_user.role or current_user.role.name.lower() != 'admin':
        return jsonify({
            'success': False,
            'error': 'Permission denied. Admin role required.'
        }), 403
    
    # Check if integration is configured
    api_url = current_app.config.get('NOOYEN_API_BASE_URL', '')
    api_key = current_app.config.get('NOOYEN_API_KEY', '')
    
    if not api_url or not api_key:
        return jsonify({
            'success': False,
            'error': 'NooyenUSATracker integration is not configured.'
        }), 400
    
    try:
        # Fetch users from NooyenUSATracker
        nooyen_users = integration.get_available_users()
        
        # Get existing RFID tags in our system
        existing_rfid_tags = set(u.rfid_tag for u in RFIDUser.query.all())
        
        # Filter out users that already exist in our system
        available_users = [
            user for user in nooyen_users 
            if user.get('rfid_tag') and user.get('rfid_tag') not in existing_rfid_tags
        ]
        
        return jsonify({
            'success': True,
            'users': available_users
        })
    
    except Exception as e:
        return jsonify({
            'success': False,
            'error': f'Error fetching users: {str(e)}'
        }), 500

@user_api_bp.route('/import', methods=['POST'])
@login_required
def import_user():
    """
    Import a user from NooyenUSATracker
    """
    # Check if user is admin
    if not current_user.role or current_user.role.name.lower() != 'admin':
        return jsonify({
            'success': False,
            'error': 'Permission denied. Admin role required.'
        }), 403
    
    # Check if integration is configured
    api_url = current_app.config.get('NOOYEN_API_BASE_URL', '')
    api_key = current_app.config.get('NOOYEN_API_KEY', '')
    
    if not api_url or not api_key:
        return jsonify({
            'success': False,
            'error': 'NooyenUSATracker integration is not configured.'
        }), 400
    
    # Get external user ID from request
    data = request.get_json()
    if not data or 'external_id' not in data:
        return jsonify({
            'success': False,
            'error': 'External user ID is required.'
        }), 400
    
    external_id = data['external_id']
    
    try:
        # Get user details from NooyenUSATracker
        user_data = integration.get_user_by_id(external_id)
        
        if not user_data:
            return jsonify({
                'success': False,
                'error': f'User with ID {external_id} not found in NooyenUSATracker.'
            }), 404
        
        # Check if user already exists
        existing_user = RFIDUser.query.filter_by(rfid_tag=user_data.get('rfid_tag')).first()
        if existing_user:
            return jsonify({
                'success': False,
                'error': f'A user with RFID tag {user_data.get("rfid_tag")} already exists.'
            }), 409
        
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
        
        return jsonify({
            'success': True,
            'message': f'User {user_data.get("name")} imported successfully',
            'user': {
                'id': new_user.id,
                'name': new_user.name,
                'rfid_tag': new_user.rfid_tag
            }
        })
    
    except Exception as e:
        db.session.rollback()
        return jsonify({
            'success': False,
            'error': f'Error importing user: {str(e)}'
        }), 500

@user_api_bp.route('/update_permissions/<int:user_id>', methods=['POST'])
@login_required
def update_permissions(user_id):
    """
    Update a user's machine permissions and sync with NooyenUSATracker
    """
    # Check if user is admin
    if not current_user.role or current_user.role.name.lower() != 'admin':
        return jsonify({
            'success': False,
            'error': 'Permission denied. Admin role required.'
        }), 403
    
    # Get the user
    user = RFIDUser.query.get_or_404(user_id)
    
    # Get data from request
    data = request.get_json()
    if not data or 'machine_ids' not in data:
        return jsonify({
            'success': False,
            'error': 'Machine IDs are required.'
        }), 400
    
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
        
        # Sync with NooyenUSATracker if integration is configured
        api_url = current_app.config.get('NOOYEN_API_BASE_URL', '')
        api_key = current_app.config.get('NOOYEN_API_KEY', '')
        
        if api_url and api_key and hasattr(user, 'external_id') and user.external_id:
            # Get machine IDs in the format expected by NooyenUSATracker
            machine_external_ids = []
            for machine_id in machine_ids:
                machine = Machine.query.get(machine_id)
                if machine and machine.machine_id:  # machine_id is the external ID/code like "W1"
                    machine_external_ids.append(machine.machine_id)
            
            # Send to NooyenUSATracker
            success = integration.update_user_permissions(
                user.external_id, 
                machine_external_ids
            )
            
            if not success:
                return jsonify({
                    'success': True,
                    'warning': 'Permissions updated locally but failed to sync with NooyenUSATracker.'
                })
        
        return jsonify({
            'success': True,
            'message': 'Permissions updated successfully'
        })
    
    except Exception as e:
        db.session.rollback()
        return jsonify({
            'success': False,
            'error': f'Error updating permissions: {str(e)}'
        }), 500

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