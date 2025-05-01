"""
API routes for NooyenUSATracker integration
These routes provide endpoints for manual synchronization, status checking, and data exchange
"""

from flask import Blueprint, jsonify, current_app, request
from flask_login import login_required, current_user
from .nooyen_integration import integration, manual_sync, get_last_sync_time
from datetime import datetime
from models import db, Node, Machine, MachineLog, RFIDUser, Alert, User, MachineAuthorization

integration_bp = Blueprint('integration', __name__, url_prefix='/integration')

@integration_bp.route('/status', methods=['GET'])
@login_required
def integration_status():
    """Get integration status and configuration"""
    api_url = current_app.config.get('NOOYEN_API_BASE_URL')
    has_api_key = bool(current_app.config.get('NOOYEN_API_KEY'))
    sync_interval = current_app.config.get('SYNC_INTERVAL', 3600)
    last_sync = get_last_sync_time()
    
    return jsonify({
        'status': 'configured' if api_url and has_api_key else 'not_configured',
        'api_url': api_url,
        'has_api_key': has_api_key,
        'sync_interval': sync_interval,
        'last_sync': last_sync
    })

@integration_bp.route('/sync', methods=['POST'])
@login_required
def trigger_sync():
    """Manually trigger data synchronization"""
    if not current_app.config.get('NOOYEN_API_BASE_URL') or not current_app.config.get('NOOYEN_API_KEY'):
        return jsonify({
            'success': False,
            'message': 'Integration not properly configured. Please set NOOYEN_API_BASE_URL and NOOYEN_API_KEY.'
        }), 400
    
    success = manual_sync()
    
    if success:
        return jsonify({
            'success': True,
            'message': 'Data synchronization completed successfully.',
            'last_sync': get_last_sync_time()
        })
    else:
        return jsonify({
            'success': False,
            'message': 'Data synchronization failed. Check logs for details.'
        }), 500

@integration_bp.route('/api/auth', methods=['POST'])
def auth_endpoint():
    """Authenticate RFID card access for a specific machine
    
    This endpoint is used by NooyenUSATracker to verify if a user has access to a machine
    
    Required payload:
    {
      "card_id": "0123456789",
      "machine_id": "W1"
    }
    """
    # API Key verification
    api_key = request.headers.get('X-API-Key')
    if not api_key or api_key != current_app.config.get('NOOYEN_API_KEY'):
        return jsonify({'success': False, 'message': 'Invalid API key'}), 401
    
    data = request.get_json()
    if not data or 'card_id' not in data or 'machine_id' not in data:
        return jsonify({'success': False, 'message': 'Missing required parameters'}), 400
    
    card_id = data['card_id']
    machine_id = data['machine_id']
    
    # Find the user and machine
    user = RFIDUser.query.filter_by(rfid_tag=card_id).first()
    machine = Machine.query.filter_by(machine_id=machine_id).first()
    
    if not user:
        return jsonify({'success': False, 'message': 'User not found'}), 404
    
    if not machine:
        return jsonify({'success': False, 'message': 'Machine not found'}), 404
    
    # Check if user is active
    if not user.active:
        return jsonify({'success': False, 'message': 'User inactive'}), 403
    
    # Admin override check
    if user.is_admin_override:
        access_level = "admin"
        has_access = True
    else:
        # Check machine authorization
        from models import MachineAuthorization
        auth = MachineAuthorization.query.filter_by(user_id=user.id, machine_id=machine.id).first()
        has_access = bool(auth)
        access_level = "operator" if has_access else "none"
    
    response = {
        'success': has_access,
        'user': {
            'id': user.id,
            'username': user.rfid_tag,
            'fullName': user.name,
            'role': access_level
        },
        'access_level': access_level,
        'machine_id': machine.machine_id,
        'timestamp': datetime.utcnow().isoformat() + 'Z'
    }
    
    return jsonify(response)

@integration_bp.route('/api/node_status', methods=['GET'])
def node_status_endpoint():
    """Get status of all active nodes
    
    This endpoint provides comprehensive status information about all nodes
    and their connected machines. It includes activity counts and user information.
    """
    # API Key verification
    api_key = request.headers.get('X-API-Key')
    if not api_key or api_key != current_app.config.get('NOOYEN_API_KEY'):
        return jsonify({'success': False, 'message': 'Invalid API key'}), 401
    
    # Get all nodes
    nodes = Node.query.all()
    node_data = []
    
    today = datetime.utcnow().date()
    
    for node in nodes:
        # Get machines for this node
        machines = Machine.query.filter_by(node_id=node.id).all()
        machine_data = []
        
        for machine in machines:
            # Get today's access count
            today_count = MachineLog.query.filter(
                MachineLog.machine_id == machine.id,
                MachineLog.login_time >= today
            ).count()
            
            # Get current user details if machine is active
            user_data = None
            activity_count = 0
            
            if machine.current_user_id:
                user = RFIDUser.query.get(machine.current_user_id)
                if user:
                    user_data = {
                        'id': user.id,
                        'name': user.name,
                        'rfid_tag': user.rfid_tag
                    }
                
                # Get current session log for activity count
                current_log = MachineLog.query.filter_by(
                    machine_id=machine.id,
                    user_id=machine.current_user_id,
                    logout_time=None
                ).first()
                
                if current_log:
                    # Get count since login
                    activity_count = 0  # This would come from the count pin on the ESP32
            
            machine_data.append({
                'id': machine.id,
                'machine_id': machine.machine_id,
                'name': machine.name,
                'status': machine.status,
                'zone': machine.zone.name if machine.zone else "Unassigned",
                'current_user': user_data,
                'today_access_count': today_count,
                'activity_count': activity_count,
                'last_activity': machine.last_activity.isoformat() if machine.last_activity else None
            })
        
        # Compile node data
        node_data.append({
            'id': node.id,
            'node_id': node.node_id,
            'name': node.name,
            'ip_address': node.ip_address,
            'node_type': node.node_type,
            'status': node.status(),
            'last_seen': node.last_seen.isoformat() if node.last_seen else None,
            'machines': machine_data
        })
    
    return jsonify({
        'timestamp': datetime.utcnow().isoformat() + 'Z',
        'nodes': node_data
    })

@integration_bp.route('/api/alerts', methods=['POST'])
def alerts_endpoint():
    """Receive alerts from NooyenUSATracker
    
    This endpoint receives alerts from NooyenUSATracker and forwards them to the appropriate
    node or displays them on the dashboard.
    
    Required payload:
    {
      "id": 1,
      "machineId": "W1",
      "senderId": 1,
      "message": "Machine requires maintenance",
      "alertType": "warning",
      "status": "pending",
      "origin": "machine",
      "createdAt": "2025-04-18T12:30:45Z"
    }
    """
    # API Key verification
    api_key = request.headers.get('X-API-Key')
    if not api_key or api_key != current_app.config.get('NOOYEN_API_KEY'):
        return jsonify({'success': False, 'message': 'Invalid API key'}), 401
    
    data = request.get_json()
    if not data:
        return jsonify({'success': False, 'message': 'Missing alert data'}), 400
    
    # Find the associated machine
    machine_id = data.get('machineId')
    machine = Machine.query.filter_by(machine_id=machine_id).first()
    
    # Create a new alert in our database
    new_alert = Alert(
        external_id=data.get('id'),
        machine_id=machine.id if machine else None,
        message=data.get('message', 'No message provided'),
        alert_type=data.get('alertType', 'info'),
        status=data.get('status', 'pending'),
        origin='nooyen_tracker'  # Mark this alert as coming from NooyenUSATracker
    )
    
    db.session.add(new_alert)
    db.session.commit()
    
    response = {
        'success': True,
        'message': 'Alert received and stored',
        'local_alert_id': new_alert.id,
        'external_alert_id': data.get('id'),
        'timestamp': datetime.utcnow().isoformat() + 'Z'
    }
    
    if machine:
        response['machine_name'] = machine.name
    
    return jsonify(response)

@integration_bp.route('/api/alerts/<int:alert_id>/acknowledge', methods=['POST'])
def acknowledge_alert(alert_id):
    """Acknowledge an alert from NooyenUSATracker"""
    # API Key verification
    api_key = request.headers.get('X-API-Key')
    if not api_key or api_key != current_app.config.get('NOOYEN_API_KEY'):
        return jsonify({'success': False, 'message': 'Invalid API key'}), 401
    
    # Find the alert by external ID
    alert = Alert.query.filter_by(external_id=alert_id).first()
    
    if not alert:
        return jsonify({
            'success': False,
            'message': f'Alert with external ID {alert_id} not found'
        }), 404
    
    # Mark as acknowledged
    alert.status = 'acknowledged'
    alert.acknowledged_at = datetime.utcnow()
    
    # Use the first admin user if we're not in a login context
    if current_user.is_authenticated:
        alert.acknowledged_by = current_user.id
    else:
        admin = User.query.filter_by(role_id=1).first()  # assuming role_id 1 is admin
        if admin:
            alert.acknowledged_by = admin.id
    
    db.session.commit()
    
    return jsonify({
        'success': True,
        'message': f'Alert {alert_id} acknowledged',
        'alert': alert.to_dict(),
        'timestamp': datetime.utcnow().isoformat() + 'Z'
    })

@integration_bp.route('/api/alerts/<int:alert_id>/resolve', methods=['POST'])
def resolve_alert(alert_id):
    """Resolve an alert from NooyenUSATracker"""
    # API Key verification
    api_key = request.headers.get('X-API-Key')
    if not api_key or api_key != current_app.config.get('NOOYEN_API_KEY'):
        return jsonify({'success': False, 'message': 'Invalid API key'}), 401
    
    # Find the alert by external ID
    alert = Alert.query.filter_by(external_id=alert_id).first()
    
    if not alert:
        return jsonify({
            'success': False,
            'message': f'Alert with external ID {alert_id} not found'
        }), 404
    
    # Mark as resolved
    alert.status = 'resolved'
    alert.resolved_at = datetime.utcnow()
    
    # Use the first admin user if we're not in a login context
    if current_user.is_authenticated:
        alert.resolved_by = current_user.id
    else:
        admin = User.query.filter_by(role_id=1).first()  # assuming role_id 1 is admin
        if admin:
            alert.resolved_by = admin.id
    
    db.session.commit()
    
    return jsonify({
        'success': True,
        'message': f'Alert {alert_id} resolved',
        'alert': alert.to_dict(),
        'timestamp': datetime.utcnow().isoformat() + 'Z'
    })