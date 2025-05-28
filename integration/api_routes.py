"""
API routes for ShopMonitor integration
These routes provide endpoints for manual synchronization, status checking, and data exchange
"""

from flask import Blueprint, jsonify, current_app, request
from flask_login import login_required, current_user
from .shop_integration import integration, manual_sync, get_last_sync_time
from datetime import datetime, timedelta
from models import db, Node, Machine, MachineLog, RFIDUser, Alert, User, MachineAuthorization
from security import require_api_token, sanitize_input
from error_handling import handle_api_errors, AppError, logger, exponential_backoff
import hmac

integration_bp = Blueprint('integration', __name__, url_prefix='/integration')

@integration_bp.route('/status', methods=['GET'])
@login_required
@handle_api_errors
def integration_status():
    """Get integration status and configuration"""
    api_url = current_app.config.get('SHOP_API_BASE_URL') or current_app.config.get('Shop_API_BASE_URL')
    has_api_key = bool(current_app.config.get('SHOP_API_KEY') or current_app.config.get('Shop_API_KEY'))
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
@handle_api_errors
def trigger_sync():
    """Manually trigger data synchronization"""
    api_url = current_app.config.get('SHOP_API_BASE_URL') or current_app.config.get('Shop_API_BASE_URL')
    api_key = current_app.config.get('SHOP_API_KEY') or current_app.config.get('Shop_API_KEY')
    
    if not api_url or not api_key:
        raise AppError(code=5001, message="Integration not properly configured. Please set SHOP_API_BASE_URL and SHOP_API_KEY.")
    
    try:
        success = manual_sync()
        
        if not success:
            raise AppError(code=5000, message="Data synchronization failed. Check logs for details.")
        
        return jsonify({
            'success': True,
            'message': 'Data synchronization completed successfully.',
            'last_sync': get_last_sync_time()
        })
    except Exception as e:
        logger.error(f"Sync error: {str(e)}")
        raise AppError(code=5000, message=f"Synchronization error: {str(e)}")

@integration_bp.route('/api/auth', methods=['POST'])
@require_api_token
@handle_api_errors
def auth_endpoint():
    """Authenticate RFID card access for a specific machine
    
    This endpoint is used by ShopMonitor to verify if a user has access to a machine
    
    Required payload:
    {
      "card_id": "0123456789",
      "machine_id": "W1"
    }
    """
    data = request.get_json()
    if not data or 'card_id' not in data or 'machine_id' not in data:
        raise AppError(code=5001, message="Missing required parameters")
    
    card_id = sanitize_input(data['card_id'])
    machine_id = sanitize_input(data['machine_id'])
    
    # Find the user and machine
    user = RFIDUser.query.filter_by(rfid_tag=card_id).first()
    machine = Machine.query.filter_by(machine_id=machine_id).first()
    
    if not user:
        raise AppError(code=5003, message="User not found")
    
    if not machine:
        raise AppError(code=5003, message="Machine not found")
    
    # Check if user is active
    if not user.active:
        raise AppError(code=2003, message="User inactive")
    
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
@require_api_token
@handle_api_errors
def node_status_endpoint():
    """Get status of all active nodes
    
    This endpoint provides comprehensive status information about all nodes
    and their connected machines. It includes activity counts and user information.
    """
    try:
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
    except Exception as e:
        logger.error(f"Error in node status endpoint: {str(e)}")
        raise AppError(code=5000, message=f"Error retrieving node status: {str(e)}")

@integration_bp.route('/api/alerts', methods=['POST'])
@require_api_token
@handle_api_errors
def alerts_endpoint():
    """Receive alerts from ShopMonitor
    
    This endpoint receives alerts from ShopMonitor and forwards them to the appropriate
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
    data = request.get_json()
    if not data:
        raise AppError(code=5001, message="Missing alert data")
    
    try:
        # Find the associated machine - sanitize input
        machine_id = sanitize_input(data.get('machineId'))
        message = sanitize_input(data.get('message'))
        
        machine = Machine.query.filter_by(machine_id=machine_id).first()
        
        # Create a new alert in our database
        new_alert = Alert(
            external_id=data.get('id'),
            machine_id=machine.id if machine else None,
            message=message or 'No message provided',
            alert_type=sanitize_input(data.get('alertType', 'info')),
            status=sanitize_input(data.get('status', 'pending')),
            origin='shop_monitor'  # Mark this alert as coming from ShopMonitor
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
    except Exception as e:
        db.session.rollback()
        logger.error(f"Error processing alert: {str(e)}")
        raise AppError(code=5000, message=f"Error processing alert: {str(e)}")

@integration_bp.route('/api/alerts/<int:alert_id>/acknowledge', methods=['POST'])
@require_api_token
@handle_api_errors
def acknowledge_alert(alert_id):
    """Acknowledge an alert from ShopMonitor"""
    # Find the alert by external ID
    alert = Alert.query.filter_by(external_id=alert_id).first()
    
    if not alert:
        raise AppError(code=5003, message=f"Alert with external ID {alert_id} not found")
    
    try:
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
    except Exception as e:
        db.session.rollback()
        logger.error(f"Error acknowledging alert: {str(e)}")
        raise AppError(code=5000, message=f"Error acknowledging alert: {str(e)}")

@integration_bp.route('/api/alerts/<int:alert_id>/resolve', methods=['POST'])
@require_api_token
@handle_api_errors
def resolve_alert(alert_id):
    """Resolve an alert from ShopMonitor"""
    # Find the alert by external ID
    alert = Alert.query.filter_by(external_id=alert_id).first()
    
    if not alert:
        raise AppError(code=5003, message=f"Alert with external ID {alert_id} not found")
    
    try:
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
    except Exception as e:
        db.session.rollback()
        logger.error(f"Error resolving alert: {str(e)}")
        raise AppError(code=5000, message=f"Error resolving alert: {str(e)}")

@integration_bp.route('/api/device_status', methods=['POST'])
@require_api_token
@handle_api_errors
def device_status_update():
    """Receive status updates from ESP32 devices
    
    This endpoint allows ESP32 devices to report their status and sensor readings.
    
    Required payload:
    {
      "node_id": "esp32-001",
      "ip_address": "192.168.1.100",
      "version": "1.0.0",
      "uptime": 3600,
      "machine_states": [
        {
          "port": 0,
          "active": true,
          "current_rfid": "0123456789",
          "activity_count": 42,
          "voltage": 3.3
        }
      ]
    }
    """
    data = request.get_json()
    if not data or 'node_id' not in data:
        raise AppError(code=5001, message="Missing required node_id parameter")
    
    try:
        node_id = sanitize_input(data['node_id'])
        
        # Find or create the node
        node = Node.query.filter_by(node_id=node_id).first()
        if not node:
            # Auto-register new node
            node = Node(
                node_id=node_id,
                name=f"Node {node_id}",
                description=f"Auto-registered node {node_id}",
                ip_address=sanitize_input(data.get('ip_address', '')),
                node_type=sanitize_input(data.get('type', 'machine_monitor')),
                is_esp32=True
            )
            db.session.add(node)
            logger.info(f"Auto-registered new node: {node_id}")
        
        # Update node details
        node.ip_address = sanitize_input(data.get('ip_address', node.ip_address))
        node.firmware_version = sanitize_input(data.get('version', node.firmware_version))
        node.last_seen = datetime.utcnow()
        
        # Process machine states if provided
        if 'machine_states' in data and isinstance(data['machine_states'], list):
            for state in data['machine_states']:
                port = state.get('port')
                if port is not None:
                    # Find machine on this port
                    machine = Machine.query.filter_by(node_id=node.id, node_port=port).first()
                    if machine:
                        # Update machine status
                        machine.status = 'active' if state.get('active') else 'idle'
                        machine.last_activity = datetime.utcnow()
                        
                        # Handle RFID login/logout if provided
                        current_rfid = sanitize_input(state.get('current_rfid'))
                        if current_rfid:
                            # If the machine doesn't have this user logged in, log them in
                            user = RFIDUser.query.filter_by(rfid_tag=current_rfid).first()
                            if user and machine.current_user_id != user.id:
                                # Log out previous user if any
                                if machine.current_user_id:
                                    _logout_previous_user(machine)
                                
                                # Create new session
                                machine.current_user_id = user.id
                                log_entry = MachineLog(
                                    machine_id=machine.id,
                                    user_id=user.id,
                                    login_time=datetime.utcnow(),
                                    status="active"
                                )
                                db.session.add(log_entry)
                                logger.info(f"User {user.name} logged in to machine {machine.name}")
                        
                        # Handle null RFID as logout
                        elif not current_rfid and machine.current_user_id:
                            _logout_previous_user(machine)
                        
                        # Update counters if available
                        if 'activity_count' in state:
                            # This would update a counter field in the machine model or log
                            pass
        
        db.session.commit()
        
        return jsonify({
            'success': True,
            'message': 'Status update received',
            'timestamp': datetime.utcnow().isoformat() + 'Z',
            'node_id': node_id
        })
    except Exception as e:
        db.session.rollback()
        logger.error(f"Error processing device status: {str(e)}")
        raise AppError(code=5000, message=f"Error processing device status: {str(e)}")

def _logout_previous_user(machine):
    """Helper function to log out previous user from a machine"""
    # Find the active log entry
    active_log = MachineLog.query.filter_by(
        machine_id=machine.id,
        user_id=machine.current_user_id,
        logout_time=None
    ).first()
    
    if active_log:
        active_log.logout_time = datetime.utcnow()
        active_log.total_time = (active_log.logout_time - active_log.login_time).total_seconds()
        active_log.status = "completed"
        logger.info(f"User {machine.current_user.name if machine.current_user else 'Unknown'} logged out from {machine.name}")
    
    # Reset machine user
    machine.current_user_id = None
    machine.status = 'idle'

# Add the token endpoint for node authentication
@integration_bp.route('/api/auth/token', methods=['POST'])
def get_api_token():
    """Generate an authentication token for node devices
    
    The ESP32 nodes will call this endpoint with their node_id and secret
    to receive a JWT token for use with subsequent API requests.
    """
    if not request.is_json:
        return jsonify({"error": "Missing JSON in request"}), 400
    
    data = request.json
    node_id = data.get('node_id')
    secret = data.get('secret')
    device_info = data.get('device_info', '')
    
    if not node_id or not secret:
        return jsonify({"error": "Missing node_id or secret"}), 400
    
    # Verify node identity and secret
    node = Node.query.filter_by(node_id=node_id).first()
    if not node:
        current_app.logger.warning(f"Token request from unknown node: {node_id}")
        return jsonify({"error": "Invalid node ID"}), 403
    
    # Verify the node secret using a constant-time comparison to prevent timing attacks
    if not hmac.compare_digest(node.secret_hash, security.hash_secret(secret, node.salt)):
        current_app.logger.warning(f"Invalid secret for node: {node_id}")
        return jsonify({"error": "Invalid credentials"}), 403
    
    # Generate JWT token with 30-day expiration by default
    expires_in = 30 * 24 * 60 * 60  # 30 days in seconds
    token_payload = {
        'sub': node_id,
        'type': 'node',
        'iat': datetime.utcnow(),
        'exp': datetime.utcnow() + timedelta(seconds=expires_in),
        'device_info': device_info
    }
    
    # Sign the token with the app's secret key
    token = security.create_access_token(token_payload)
    
    # Log the successful token issuance
    current_app.logger.info(f"API token issued to node: {node_id}, expires in {expires_in} seconds")
    
    # Record this authentication in node history
    node.last_token_issued = datetime.utcnow()
    if device_info:
        node.device_info = device_info
    db.session.commit()
    
    return jsonify({
        "access_token": token,
        "token_type": "Bearer",
        "expires_in": expires_in
    }), 201