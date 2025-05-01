import logging
from datetime import datetime, timedelta
from functools import wraps

from flask import render_template, request, redirect, url_for, flash, jsonify, abort, session
from flask_login import login_user, logout_user, login_required, current_user
from werkzeug.security import check_password_hash, generate_password_hash

from app import app, db
from models import User, Role, Zone, Machine, RFIDUser, MachineAuthorization, MachineLog, Node, AccessoryIO, Alert

# Helper decorator for admin access
def admin_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if not current_user.is_authenticated or current_user.role.name != 'admin':
            flash('Admin access required', 'danger')
            return redirect(url_for('login'))
        return f(*args, **kwargs)
    return decorated_function

# Route for API docs
@app.route('/api-docs')
@login_required
@admin_required
def api_docs_redirect():
    """Redirect to the API documentation page"""
    return redirect(url_for('integration_web.api_docs'))

# Authentication routes
@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form.get('username')
        password = request.form.get('password')
        
        user = User.query.filter_by(username=username).first()
        
        if user and check_password_hash(user.password_hash, password):
            login_user(user)
            next_page = request.args.get('next')
            return redirect(next_page or url_for('dashboard'))
        else:
            flash('Invalid username or password', 'danger')
    
    return render_template('login.html')

@app.route('/logout')
@login_required
def logout():
    logout_user()
    flash('You have been logged out', 'success')
    return redirect(url_for('login'))

# Dashboard route
@app.route('/')
@login_required
def dashboard():
    zones = Zone.query.all()
    machines = Machine.query.all()
    
    # Group machines by zone
    machines_by_zone = {}
    for zone in zones:
        machines_by_zone[zone.id] = Machine.query.filter_by(zone_id=zone.id).all()
    
    # Get active user count
    active_users_count = db.session.query(Machine).filter(Machine.current_user_id.isnot(None)).count()
    
    # Get today's logs
    today = datetime.utcnow().date()
    today_logs = MachineLog.query.filter(
        MachineLog.login_time >= today,
        MachineLog.login_time < today + timedelta(days=1)
    ).count()
    
    # Get machines by status
    idle_count = Machine.query.filter_by(status='idle').count()
    active_count = Machine.query.filter_by(status='active').count()
    warning_count = Machine.query.filter_by(status='warning').count()
    offline_count = Machine.query.filter_by(status='offline').count()
    
    return render_template(
        'dashboard.html',
        zones=zones,
        machines=machines,
        machines_by_zone=machines_by_zone,
        active_users=active_users_count,
        today_logs=today_logs,
        idle_count=idle_count,
        active_count=active_count,
        warning_count=warning_count,
        offline_count=offline_count
    )

# Machine status update via AJAX
@app.route('/api/machines/status')
@login_required
def machine_status():
    machines = Machine.query.all()
    result = []
    
    for machine in machines:
        # Calculate warning/timeout status
        warning_status = "normal"
        
        if machine.current_user_id and machine.last_activity:
            inactive_time = datetime.utcnow() - machine.last_activity
            if inactive_time > timedelta(minutes=55):  # Almost at 1 hour timeout
                warning_status = "warning"
            elif inactive_time > timedelta(hours=1):  # Past timeout
                warning_status = "timeout"
        
        user_name = None
        if machine.current_user:
            user_name = machine.current_user.name
            
        result.append({
            'id': machine.id,
            'machine_id': machine.machine_id,
            'name': machine.name,
            'status': machine.status,
            'warning_status': warning_status,
            'zone_id': machine.zone_id,
            'current_user': user_name,
            'last_activity': machine.last_activity.isoformat() if machine.last_activity else None
        })
    
    return jsonify(result)

# User management routes
@app.route('/users')
@login_required
@admin_required
def users():
    rfid_users = RFIDUser.query.all()
    admin_users = User.query.all()
    return render_template('users.html', rfid_users=rfid_users, admin_users=admin_users)

@app.route('/users/add_rfid', methods=['POST'])
@login_required
@admin_required
def add_rfid_user():
    rfid_tag = request.form.get('rfid_tag')
    name = request.form.get('name')
    email = request.form.get('email')
    
    if not rfid_tag or not name:
        flash('RFID tag and name are required', 'danger')
        return redirect(url_for('users'))
    
    # Check if RFID tag already exists
    existing_user = RFIDUser.query.filter_by(rfid_tag=rfid_tag).first()
    if existing_user:
        flash('RFID tag already registered', 'danger')
        return redirect(url_for('users'))
    
    is_offline_access = 'is_offline_access' in request.form
    is_admin_override = 'is_admin_override' in request.form
    
    new_user = RFIDUser(
        rfid_tag=rfid_tag,
        name=name,
        email=email,
        is_offline_access=is_offline_access,
        is_admin_override=is_admin_override
    )
    db.session.add(new_user)
    db.session.commit()
    
    # Push the new user to NooyenUSATracker if integration is configured
    try:
        from integration.nooyen_integration import push_user_changes
        push_user_changes(new_user)
    except Exception as e:
        app.logger.error(f"Failed to push user changes: {str(e)}")
    
    flash('RFID user added successfully', 'success')
    return redirect(url_for('users'))

@app.route('/users/edit_rfid/<int:user_id>', methods=['POST'])
@login_required
@admin_required
def edit_rfid_user(user_id):
    user = RFIDUser.query.get_or_404(user_id)
    
    user.name = request.form.get('name')
    user.email = request.form.get('email')
    user.active = 'active' in request.form
    user.is_offline_access = 'is_offline_access' in request.form
    user.is_admin_override = 'is_admin_override' in request.form
    
    db.session.commit()
    
    # Push the updated user to NooyenUSATracker if integration is configured
    try:
        from integration.nooyen_integration import push_user_changes
        push_user_changes(user)
    except Exception as e:
        app.logger.error(f"Failed to push user changes: {str(e)}")
    
    flash('User updated successfully', 'success')
    return redirect(url_for('users'))

@app.route('/users/delete_rfid/<int:user_id>', methods=['POST'])
@login_required
@admin_required
def delete_rfid_user(user_id):
    user = RFIDUser.query.get_or_404(user_id)
    
    # Check if user is currently using any machine
    if Machine.query.filter_by(current_user_id=user.id).first():
        flash('Cannot delete user who is currently using a machine', 'danger')
        return redirect(url_for('users'))
    
    # Delete associated authorizations and logs
    MachineAuthorization.query.filter_by(user_id=user.id).delete()
    MachineLog.query.filter_by(user_id=user.id).delete()
    
    db.session.delete(user)
    db.session.commit()
    
    flash('User deleted successfully', 'success')
    return redirect(url_for('users'))

@app.route('/users/add_admin', methods=['POST'])
@login_required
@admin_required
def add_admin_user():
    username = request.form.get('username')
    email = request.form.get('email')
    password = request.form.get('password')
    
    if not username or not email or not password:
        flash('All fields are required', 'danger')
        return redirect(url_for('users'))
    
    # Check if username or email already exists
    if User.query.filter((User.username == username) | (User.email == email)).first():
        flash('Username or email already exists', 'danger')
        return redirect(url_for('users'))
    
    # Get admin role
    admin_role = Role.query.filter_by(name='admin').first()
    if not admin_role:
        flash('Admin role not found', 'danger')
        return redirect(url_for('users'))
    
    new_user = User(
        username=username,
        email=email,
        password_hash=generate_password_hash(password),
        role_id=admin_role.id
    )
    
    db.session.add(new_user)
    db.session.commit()
    
    flash('Admin user added successfully', 'success')
    return redirect(url_for('users'))

# Machine management routes
@app.route('/machines')
@login_required
@admin_required
def machines():
    machines = Machine.query.all()
    zones = Zone.query.all()
    # Add nodes for node-based machine configuration
    from models import Node
    nodes = Node.query.all()
    return render_template('machines.html', machines=machines, zones=zones, nodes=nodes)

@app.route('/machines/add', methods=['POST'])
@login_required
@admin_required
def add_machine():
    machine_id = request.form.get('machine_id')
    name = request.form.get('name')
    description = request.form.get('description', '')
    zone_id = request.form.get('zone_id')
    controller_type = request.form.get('controller_type', 'node')
    
    if not machine_id or not name or not zone_id:
        flash('Machine ID, name, and location are required', 'danger')
        return redirect(url_for('machines'))
    
    # Check if machine ID already exists
    if Machine.query.filter_by(machine_id=machine_id).first():
        flash('Machine ID already exists', 'danger')
        return redirect(url_for('machines'))
    
    # Set up the new machine with common fields
    new_machine = Machine(
        machine_id=machine_id,
        name=name,
        description=description,
        zone_id=zone_id,
        status='offline'  # New machines start as offline
    )
    
    # Handle controller type-specific fields
    if controller_type == 'direct':
        # Direct IP connection (legacy)
        ip_address = request.form.get('ip_address', '')
        new_machine.ip_address = ip_address
        new_machine.node_id = None
        new_machine.node_port = 0
    else:
        # Node-based connection
        node_id = request.form.get('node_id')
        node_port = request.form.get('node_port', 0)
        
        if not node_id:
            flash('Node is required for node-based connection', 'danger')
            return redirect(url_for('machines'))
        
        new_machine.node_id = node_id
        new_machine.node_port = node_port
        new_machine.ip_address = None
        
        # Check if this node port is already in use
        port_in_use = Machine.query.filter_by(node_id=node_id, node_port=node_port).first()
        if port_in_use:
            flash(f'Port {node_port} is already assigned to machine {port_in_use.name}', 'danger')
            return redirect(url_for('machines'))
    
    db.session.add(new_machine)
    db.session.commit()
    
    flash('Machine added successfully', 'success')
    return redirect(url_for('machines'))

@app.route('/machines/edit/<int:machine_id>', methods=['POST'])
@login_required
@admin_required
def edit_machine(machine_id):
    machine = Machine.query.get_or_404(machine_id)
    
    # Update common fields
    machine.name = request.form.get('name')
    machine.description = request.form.get('description', '')
    machine.zone_id = request.form.get('zone_id')
    
    # Handle controller type
    controller_type = request.form.get('controller_type', 'node')
    
    if controller_type == 'direct':
        # Direct IP connection (legacy)
        machine.ip_address = request.form.get('ip_address', '')
        machine.node_id = None
        machine.node_port = 0
    else:
        # Node-based connection
        node_id = request.form.get('node_id')
        node_port = request.form.get('node_port', 0)
        
        if not node_id:
            flash('Node is required for node-based connection', 'danger')
            return redirect(url_for('machines'))
        
        # Check if this node port is already in use by another machine
        port_in_use = Machine.query.filter(
            Machine.node_id == node_id, 
            Machine.node_port == node_port,
            Machine.id != machine.id  # Exclude current machine
        ).first()
        
        if port_in_use:
            flash(f'Port {node_port} is already assigned to machine {port_in_use.name}', 'danger')
            return redirect(url_for('machines'))
        
        # Update node connection settings
        machine.node_id = node_id
        machine.node_port = node_port
        machine.ip_address = None
    
    db.session.commit()
    flash('Machine updated successfully', 'success')
    return redirect(url_for('machines'))

@app.route('/machines/delete/<int:machine_id>', methods=['POST'])
@login_required
@admin_required
def delete_machine(machine_id):
    machine = Machine.query.get_or_404(machine_id)
    
    # Check if machine is currently in use
    if machine.current_user_id:
        flash('Cannot delete a machine that is currently in use', 'danger')
        return redirect(url_for('machines'))
    
    # Delete associated authorizations and logs
    MachineAuthorization.query.filter_by(machine_id=machine.id).delete()
    MachineLog.query.filter_by(machine_id=machine.id).delete()
    
    db.session.delete(machine)
    db.session.commit()
    
    flash('Machine deleted successfully', 'success')
    return redirect(url_for('machines'))

# Zone management
@app.route('/zones/add', methods=['POST'])
@login_required
@admin_required
def add_zone():
    name = request.form.get('name')
    description = request.form.get('description', '')
    
    if not name:
        flash('Zone name is required', 'danger')
        return redirect(url_for('machines'))
    
    new_zone = Zone(name=name, description=description)
    db.session.add(new_zone)
    db.session.commit()
    
    flash('Zone added successfully', 'success')
    return redirect(url_for('machines'))

@app.route('/zones/edit/<int:zone_id>', methods=['POST'])
@login_required
@admin_required
def edit_zone(zone_id):
    zone = Zone.query.get_or_404(zone_id)
    
    zone.name = request.form.get('name')
    zone.description = request.form.get('description', '')
    
    db.session.commit()
    flash('Zone updated successfully', 'success')
    return redirect(url_for('machines'))

@app.route('/zones/delete/<int:zone_id>', methods=['POST'])
@login_required
@admin_required
def delete_zone(zone_id):
    zone = Zone.query.get_or_404(zone_id)
    
    # Check if zone has machines
    if Machine.query.filter_by(zone_id=zone.id).first():
        flash('Cannot delete a zone that has machines', 'danger')
        return redirect(url_for('machines'))
    
    db.session.delete(zone)
    db.session.commit()
    
    flash('Zone deleted successfully', 'success')
    return redirect(url_for('machines'))

# Machine authorization
@app.route('/authorizations/<int:user_id>', methods=['GET', 'POST'])
@login_required
@admin_required
def manage_authorizations(user_id):
    user = RFIDUser.query.get_or_404(user_id)
    
    if request.method == 'POST':
        # Delete old authorizations
        MachineAuthorization.query.filter_by(user_id=user.id).delete()
        
        # Add new authorizations
        machine_ids = request.form.getlist('machine_ids')
        for machine_id in machine_ids:
            auth = MachineAuthorization(user_id=user.id, machine_id=machine_id)
            db.session.add(auth)
        
        db.session.commit()
        
        # Push authorization changes to the main app if integration is configured
        try:
            from integration.nooyen_integration import push_authorization_changes
            push_authorization_changes(user.id)
        except Exception as e:
            app.logger.error(f"Failed to push authorization changes: {str(e)}")
        
        flash('Machine authorizations updated', 'success')
        return redirect(url_for('users'))
    
    # Redirect to users page with modal
    flash('Please use the Machine Authorizations modal to manage machine access', 'info')
    return redirect(url_for('users'))


# Node management routes
@app.route('/nodes')
@login_required
@admin_required
def nodes():
    """List all nodes"""
    nodes = Node.query.all()
    return render_template('nodes/node_list.html', nodes=nodes)

@app.route('/nodes/add', methods=['GET', 'POST'])
@login_required
@admin_required
def add_node():
    """Add a new node"""
    if request.method == 'POST':
        node_id = request.form.get('node_id')
        name = request.form.get('name')
        description = request.form.get('description')
        ip_address = request.form.get('ip_address')
        node_type = request.form.get('node_type', 'machine_monitor')
        
        # Check if node already exists
        existing_node = Node.query.filter_by(node_id=node_id).first()
        if existing_node:
            flash(f'Node with ID {node_id} already exists', 'warning')
            return redirect(url_for('nodes'))
        
        # Create new node
        new_node = Node(
            node_id=node_id,
            name=name,
            description=description,
            ip_address=ip_address,
            node_type=node_type,
            is_esp32=True,  # Assume all nodes are ESP32 for now
            last_seen=datetime.utcnow()
        )
        
        db.session.add(new_node)
        db.session.commit()
        
        flash(f'Node {name} added successfully', 'success')
        return redirect(url_for('nodes'))
    
    # Handle redirects from device discovery
    node_id = request.args.get('node_id')
    name = request.args.get('name')
    description = request.args.get('description')
    ip_address = request.args.get('ip_address')
    
    return render_template('nodes/add_node.html', 
                          node_id=node_id,
                          name=name,
                          description=description,
                          ip_address=ip_address)

@app.route('/nodes/<int:node_id>/edit', methods=['GET', 'POST'])
@login_required
@admin_required
def edit_node(node_id):
    """Edit an existing node"""
    node = Node.query.get_or_404(node_id)
    
    if request.method == 'POST':
        node.name = request.form.get('name')
        node.description = request.form.get('description')
        node.ip_address = request.form.get('ip_address')
        node.node_type = request.form.get('node_type')
        
        db.session.commit()
        
        flash(f'Node {node.name} updated successfully', 'success')
        return redirect(url_for('nodes'))
    
    return render_template('nodes/edit_node.html', node=node)

@app.route('/nodes/<int:node_id>/delete', methods=['POST'])
@login_required
@admin_required
def delete_node(node_id):
    """Delete a node"""
    node = Node.query.get_or_404(node_id)
    
    # Check if node has machines
    if node.machines.count() > 0:
        flash('Cannot delete node with machines attached', 'danger')
        return redirect(url_for('nodes'))
    
    # Check if node has accessories
    if hasattr(node, 'accessories') and node.accessories.count() > 0:
        flash('Cannot delete node with accessories attached', 'danger')
        return redirect(url_for('nodes'))
    
    # Delete node
    db.session.delete(node)
    db.session.commit()
    
    flash(f'Node {node.name} deleted successfully', 'success')
    return redirect(url_for('nodes'))

# Devices discovery routes - these connect to the device_discovery module
@app.route('/devices')
@login_required
@admin_required
def devices_list():
    """List discovered devices"""
    # This is routed to the device_discovery module
    from integration.device_discovery import device_manager
    return device_manager.devices_list()

@app.route('/devices/<device_id>')
@login_required
@admin_required
def device_detail(device_id):
    """View device details"""
    # This is routed to the device_discovery module
    from integration.device_discovery import device_manager
    return device_manager.device_detail(device_id)

@app.route('/devices/<device_id>/update', methods=['GET', 'POST'])
@login_required
@admin_required
def device_update(device_id):
    """Update device firmware"""
    # This is routed to the device_discovery module
    from integration.device_discovery import device_manager
    return device_manager.device_update(device_id)

# API endpoint to get user machine authorizations
@app.route('/api/user_authorizations/<int:user_id>')
@login_required
@admin_required
def user_authorizations(user_id):
    user = RFIDUser.query.get_or_404(user_id)
    
    # Get all machines
    machines = Machine.query.all()
    
    # Get user's authorized machines
    authorized_machines = [auth.machine_id for auth in user.authorized_machines]
    
    # Format machine data for JSON response with additional fields
    machine_data = []
    for machine in machines:
        node_name = machine.node.name if machine.node else None
        
        machine_data.append({
            'id': machine.id,
            'machine_id': machine.machine_id,
            'name': machine.name,
            'zone_id': machine.zone_id,
            'zone_name': machine.zone.name if machine.zone else "Unassigned",
            'node_id': machine.node_id,
            'node_name': node_name,
            'node_port': machine.node_port,
            'ip_address': machine.ip_address,
            'authorized': machine.id in authorized_machines
        })
    
    return jsonify({
        'user': {
            'id': user.id,
            'name': user.name
        },
        'machines': machine_data
    })

# User profile page
@app.route('/profile')
@login_required
def user_profile():
    # Get integration status for admin users
    integration_configured = False
    last_sync_time = "Never"
    tracker_url = "#"
    
    if current_user.role and current_user.role.name == 'admin':
        try:
            from integration.nooyen_integration import get_last_sync_time
            last_sync_time = get_last_sync_time()
            
            # Check if API URL is configured
            if app.config.get('NOOYEN_API_BASE_URL'):
                integration_configured = True
                tracker_url = app.config.get('NOOYEN_API_BASE_URL')
        except Exception as e:
            app.logger.error(f"Error getting integration status: {str(e)}")
    
    # For demo purposes, just create some sample activity logs
    # In a real implementation, this would come from a real activity log table
    recent_logs = [
        {
            'timestamp': datetime.utcnow() - timedelta(hours=2),
            'action': 'Login',
            'details': 'Successful login from 192.168.1.100'
        },
        {
            'timestamp': datetime.utcnow() - timedelta(days=1),
            'action': 'Password Changed',
            'details': 'Password was successfully updated'
        },
        {
            'timestamp': datetime.utcnow() - timedelta(days=3),
            'action': 'Login',
            'details': 'Successful login from 192.168.1.100'
        }
    ]
    
    return render_template('user_profile.html', 
                          integration_configured=integration_configured,
                          last_sync_time=last_sync_time,
                          tracker_url=tracker_url,
                          recent_logs=recent_logs)

# Password update route
@app.route('/update_password', methods=['POST'])
@login_required
def update_password():
    current_password = request.form.get('current_password')
    new_password = request.form.get('new_password')
    confirm_password = request.form.get('confirm_password')
    
    if not current_password or not new_password or not confirm_password:
        flash('All fields are required', 'danger')
        return redirect(url_for('user_profile'))
    
    if new_password != confirm_password:
        flash('New passwords do not match', 'danger')
        return redirect(url_for('user_profile'))
    
    # Verify current password
    if not check_password_hash(current_user.password_hash, current_password):
        flash('Current password is incorrect', 'danger')
        return redirect(url_for('user_profile'))
    
    # Update password
    current_user.password_hash = generate_password_hash(new_password)
    db.session.commit()
    
    flash('Password updated successfully', 'success')
    return redirect(url_for('user_profile'))

# Logs view
@app.route('/logs')
@login_required
def logs():
    page = request.args.get('page', 1, type=int)
    per_page = 20
    
    logs = MachineLog.query.order_by(MachineLog.login_time.desc()).paginate(
        page=page, per_page=per_page, error_out=False)
    
    return render_template('logs.html', logs=logs)

# API endpoints for Arduino communication
@app.route('/api/check_user', methods=['GET'])
def check_user():
    rfid = request.args.get('rfid')
    machine_id = request.args.get('machine_id')
    
    if not rfid or not machine_id:
        return "DENY", 400
    
    # Find the machine and user
    machine = Machine.query.filter_by(machine_id=machine_id).first()
    user = RFIDUser.query.filter_by(rfid_tag=rfid).first()
    
    if not machine:
        logging.error(f"Machine not found: {machine_id}")
        return "DENY", 404
    
    # Update machine status to online
    if machine.status == 'offline':
        machine.status = 'idle'
        db.session.commit()
    
    if not user:
        logging.error(f"User not found for RFID: {rfid}")
        return "DENY", 401
    
    if not user.active:
        logging.error(f"User inactive: {user.name}")
        return "DENY", 403
    
    # Check authorization with consideration for admin override
    if user.is_admin_override:
        # Admin override users can access any machine
        app.logger.info(f"Admin override access granted for {user.name} on {machine.name}")
    else:
        # Check if user is authorized for this machine
        authorization = MachineAuthorization.query.filter_by(
            user_id=user.id, machine_id=machine.id).first()
        
        if not authorization:
            app.logger.error(f"User {user.name} not authorized for machine {machine.name}")
            return "DENY", 403
    
    # If another user is already using this machine, log them out
    if machine.current_user_id and machine.current_user_id != user.id:
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
    
    # If user already logged in on this machine, continue session
    if machine.current_user_id == user.id:
        machine.last_activity = datetime.utcnow()
        machine.status = 'active'
        db.session.commit()
        return "ALLOW", 200
    
    # Start new session
    machine.current_user_id = user.id
    machine.last_activity = datetime.utcnow()
    machine.status = 'active'
    
    # Create log entry
    log_entry = MachineLog(
        machine_id=machine.id,
        user_id=user.id,
        login_time=datetime.utcnow(),
        status="active"
    )
    
    db.session.add(log_entry)
    db.session.commit()
    
    return "ALLOW", 200

@app.route('/api/logout', methods=['GET'])
def logout_user():
    rfid = request.args.get('rfid')
    machine_id = request.args.get('machine_id')
    
    if not rfid or not machine_id:
        return "ERROR", 400
    
    # Find the machine and user
    machine = Machine.query.filter_by(machine_id=machine_id).first()
    user = RFIDUser.query.filter_by(rfid_tag=rfid).first()
    
    if not machine or not user:
        return "ERROR", 404
    
    # Only log out if this user is actually using the machine
    if machine.current_user_id == user.id:
        # Find the active log entry
        active_log = MachineLog.query.filter_by(
            machine_id=machine.id,
            user_id=user.id,
            logout_time=None
        ).first()
        
        if active_log:
            active_log.logout_time = datetime.utcnow()
            active_log.total_time = (active_log.logout_time - active_log.login_time).total_seconds()
            active_log.status = "completed"
        
        machine.current_user_id = None
        machine.status = 'idle'
        db.session.commit()
        
        return "LOGOUT", 200
    
    return "ERROR", 403

@app.route('/api/heartbeat', methods=['GET'])
def heartbeat():
    machine_id = request.args.get('machine_id')
    activity = request.args.get('activity', '0')
    
    if not machine_id:
        return "ERROR", 400
    
    machine = Machine.query.filter_by(machine_id=machine_id).first()
    
    if not machine:
        return "ERROR", 404
    
    # Update machine status
    machine.last_activity = datetime.utcnow()
    
    # Update status based on activity
    if activity == '1' and machine.current_user_id:
        machine.status = 'active'
    elif machine.current_user_id:
        # If no activity but user logged in, maintain active status
        machine.status = 'active'
    else:
        machine.status = 'idle'
    
    db.session.commit()
    
    return "OK"
    
@app.route('/api/update_count', methods=['GET'])
def update_count():
    machine_id = request.args.get('machine_id')
    rfid = request.args.get('rfid')
    count = request.args.get('count', '0')
    
    if not machine_id or not rfid:
        return "ERROR", 400
    
    # Find the machine and user
    machine = Machine.query.filter_by(machine_id=machine_id).first()
    user = RFIDUser.query.filter_by(rfid_tag=rfid).first()
    
    if not machine or not user:
        return "ERROR", 404
    
    # Check if there's an active session
    active_log = MachineLog.query.filter_by(
        machine_id=machine.id,
        user_id=user.id,
        logout_time=None
    ).first()
    
    if active_log:
        # Update the log with the latest count
        app.logger.info(f"Updating activity count for machine {machine_id}, user {rfid}: {count}")
        
        # Here you can store the count in the database if needed
        # For example, you might add a count field to MachineLog or create a new table
        
        # For now, we'll just log it
        machine.last_activity = datetime.utcnow()
        db.session.commit()
        
        return "OK"
    else:
        return "ERROR: No active session", 400

# Error handlers
@app.errorhandler(404)
def page_not_found(e):
    return render_template('404.html', error=e), 404

@app.errorhandler(500)
def server_error(e):
    app.logger.error(f"Server error: {str(e)}")
    return render_template('500.html', error=e), 500
    
# Accessory IO management
@app.route('/accessory_io')
@login_required
@admin_required
def accessory_io():
    from models import AccessoryIO, Node, Machine
    
    accessories = AccessoryIO.query.all()
    nodes = Node.query.filter_by(node_type='accessory_io').all()
    machines = Machine.query.all()
    
    return render_template('accessory_io.html', 
                         accessories=accessories, 
                         nodes=nodes, 
                         machines=machines)

@app.route('/accessory/add', methods=['POST'])
@login_required
@admin_required
def add_accessory():
    from models import AccessoryIO, Node
    
    name = request.form.get('name')
    description = request.form.get('description', '')
    node_id = request.form.get('node_id')
    io_type = request.form.get('io_type')
    io_number = request.form.get('io_number')
    linked_machine_id = request.form.get('linked_machine_id') or None
    activation_delay = request.form.get('activation_delay', 0)
    deactivation_delay = request.form.get('deactivation_delay', 0)
    active = 'active' in request.form
    
    if not name or not node_id:
        flash('Name and node are required', 'danger')
        return redirect(url_for('accessory_io'))
    
    # Validate that this IO port isn't already in use
    existing_io = AccessoryIO.query.filter_by(
        node_id=node_id, 
        io_type=io_type, 
        io_number=io_number
    ).first()
    
    if existing_io:
        flash(f'This {io_type} port ({io_number}) is already in use on this node', 'danger')
        return redirect(url_for('accessory_io'))
    
    # Create new accessory
    new_accessory = AccessoryIO(
        name=name,
        description=description,
        node_id=node_id,
        io_type=io_type,
        io_number=io_number,
        linked_machine_id=linked_machine_id,
        activation_delay=activation_delay,
        deactivation_delay=deactivation_delay,
        active=active
    )
    
    db.session.add(new_accessory)
    db.session.commit()
    
    flash('Accessory added successfully', 'success')
    return redirect(url_for('accessory_io'))

@app.route('/accessory/edit/<int:accessory_id>', methods=['POST'])
@login_required
@admin_required
def edit_accessory(accessory_id):
    from models import AccessoryIO
    
    accessory = AccessoryIO.query.get_or_404(accessory_id)
    
    # Store original values for comparison
    original_node_id = accessory.node_id
    original_io_type = accessory.io_type
    original_io_number = accessory.io_number
    
    # Update fields
    accessory.name = request.form.get('name')
    accessory.description = request.form.get('description', '')
    accessory.node_id = request.form.get('node_id')
    accessory.io_type = request.form.get('io_type')
    accessory.io_number = request.form.get('io_number')
    accessory.linked_machine_id = request.form.get('linked_machine_id') or None
    accessory.activation_delay = request.form.get('activation_delay', 0)
    accessory.deactivation_delay = request.form.get('deactivation_delay', 0)
    accessory.active = 'active' in request.form
    
    # Check if IO configuration changed and validate it doesn't conflict
    if (accessory.node_id != original_node_id or 
        accessory.io_type != original_io_type or 
        accessory.io_number != original_io_number):
        
        # Check for conflicts
        existing_io = AccessoryIO.query.filter_by(
            node_id=accessory.node_id, 
            io_type=accessory.io_type, 
            io_number=accessory.io_number
        ).filter(AccessoryIO.id != accessory_id).first()
        
        if existing_io:
            flash(f'This {accessory.io_type} port ({accessory.io_number}) is already in use on this node', 'danger')
            return redirect(url_for('accessory_io'))
    
    db.session.commit()
    flash('Accessory updated successfully', 'success')
    return redirect(url_for('accessory_io'))

@app.route('/accessory/delete/<int:accessory_id>', methods=['POST'])
@login_required
@admin_required
def delete_accessory(accessory_id):
    from models import AccessoryIO
    
    accessory = AccessoryIO.query.get_or_404(accessory_id)
    
    db.session.delete(accessory)
    db.session.commit()
    
    flash('Accessory deleted successfully', 'success')
    return redirect(url_for('accessory_io'))

@app.route('/accessory/control/<int:accessory_id>/<int:state>', methods=['POST'])
@login_required
@admin_required
def control_accessory(accessory_id, state):
    """Control an accessory's state (on/off)
    
    This route is called via AJAX to control independent accessories
    """
    from models import AccessoryIO
    
    accessory = AccessoryIO.query.get_or_404(accessory_id)
    
    # Only control outputs that aren't linked to machines
    if accessory.io_type != 'output' or accessory.linked_machine_id is not None:
        return jsonify({
            'success': False,
            'message': 'This accessory cannot be manually controlled'
        })
    
    try:
        # Update accessory state
        accessory.active = bool(state)
        db.session.commit()
        
        # Send control command to the node
        # TODO: Add actual node communication once hardware is ready
        # This would send a command to the node to update the output
        
        return jsonify({
            'success': True,
            'message': f'Accessory {accessory.name} turned {"on" if state else "off"}'
        })
    except Exception as e:
        db.session.rollback()
        return jsonify({
            'success': False,
            'message': f'Error controlling accessory: {str(e)}'
        })

# Arduino sketches and deployment page
@app.route('/node_sketches', methods=['GET', 'POST'])
@login_required
@admin_required
def node_sketches():
    # Default configuration values
    server_ip = request.environ.get('HTTP_HOST', '10.4.1.7').split(':')[0]
    server_port = "5000"
    
    # If form submitted, update configuration
    if request.method == 'POST':
        server_ip = request.form.get('server_ip')
        server_port = request.form.get('server_port')
        
        # Store in session for persistence
        session['arduino_server_ip'] = server_ip
        session['arduino_server_port'] = server_port
        
        flash('Arduino configuration updated successfully', 'success')
    else:
        # Get stored config if available
        server_ip = session.get('arduino_server_ip', server_ip)
        server_port = session.get('arduino_server_port', server_port)
    
    # Read the Arduino and ESP32 sketch files
    try:
        with open('arduino/machine_monitor/machine_monitor.ino', 'r') as f:
            machine_sketch = f.read()
        
        with open('arduino/machine_monitor/wiring_diagram.txt', 'r') as f:
            machine_wiring = f.read()
        
        with open('arduino/office_reader/office_reader.ino', 'r') as f:
            office_sketch = f.read()
            
        with open('arduino/office_reader/wiring_diagram.txt', 'r') as f:
            office_wiring = f.read()
            
        # Add ESP32 version
        with open('arduino/esp32_machine_monitor/esp32_machine_monitor.ino', 'r') as f:
            esp32_sketch = f.read()
            
        with open('arduino/esp32_machine_monitor/wiring_diagram.txt', 'r') as f:
            esp32_wiring = f.read()
            
        esp32_available = True
    except FileNotFoundError:
        esp32_available = False
        machine_wiring = "Wiring diagram not available"
        office_wiring = "Wiring diagram not available"
        # Create placeholders if files don't exist
        machine_sketch = """#include <SPI.h>
#include <Ethernet.h>
#include <FastLED.h>
#include <NTPClient.h>
#include <EthernetUdp.h>
#include <TimeLib.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

// Pin definitions for all 4 zones
#define NUM_ZONES 4  // Maximum of 4 zones/machines per Arduino

// LED configuration
#define LED_PIN 2          // Pin for WS2812B LED status indicators
#define NUM_LEDS 4         // One LED per zone/machine

// Define pins for each zone (machine)
// Relay pins for each machine (to control power)
const int RELAY_PINS[NUM_ZONES] = {7, 8, 9, 10};  

// Activity pins for each machine (to detect usage)
const int ACTIVITY_PINS[NUM_ZONES] = {A0, A1, A2, A3};  

// RFID reader connections using SoftwareSerial (RX, TX)
SoftwareSerial rfidReader1(22, 23); // Zone 0 RFID Reader
SoftwareSerial rfidReader2(24, 25); // Zone 1 RFID Reader
SoftwareSerial rfidReader3(26, 27); // Zone 2 RFID Reader
SoftwareSerial rfidReader4(28, 29); // Zone 3 RFID Reader
SoftwareSerial* rfidReaders[NUM_ZONES] = {&rfidReader1, &rfidReader2, &rfidReader3, &rfidReader4};

// Machine IDs for each zone (configure as needed)
String machineIDs[NUM_ZONES] = {"01", "02", "03", "04"};

// States for each machine
enum Status { IDLE, ACCESS_GRANTED, ACCESS_DENIED, LOGGED_OUT, WARNING };
Status machineStatus[NUM_ZONES] = {IDLE, IDLE, IDLE, IDLE};

// RFID tracking for each machine
String activeRFIDs[NUM_ZONES] = {"", "", "", ""};

// Activity timing for each machine
unsigned long lastActivityTimes[NUM_ZONES] = {0, 0, 0, 0};
unsigned long relayOffTimes[NUM_ZONES] = {0, 0, 0, 0};
bool flashingYellow[NUM_ZONES] = {false, false, false, false};

// LED color definitions
CRGB leds[NUM_LEDS];
CRGB statusColors[5] = {
  CRGB::Blue,    // IDLE
  CRGB::Green,   // ACCESS_GRANTED
  CRGB::Red,     // ACCESS_DENIED
  CRGB::Blue,    // LOGGED_OUT
  CRGB::Yellow   // WARNING
};

// Timing constants
const unsigned long activityTimeout = 3600000;  // 1-hour inactivity timeout
const unsigned long warningTimeout = 300000;    // 5-minute warning before timeout
const unsigned long relayOffDelay = 10000;      // 10-second delay for relay off

// Network configuration
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC Address for Ethernet
IPAddress server($SERVER_IP$); // Server IP address - will be replaced dynamically
EthernetClient client;
EthernetUDP udp;
NTPClient timeClient(udp); // NTP Client to get time

// Flash patterns
unsigned long lastYellowFlashTimes[NUM_ZONES] = {0, 0, 0, 0};
bool yellowLedStates[NUM_ZONES] = {false, false, false, false};

void setup() {
  Serial.begin(9600);
  Serial.println("Initializing RFID Machine Monitor...");

  // Initialize RFID readers
  for (int i = 0; i < NUM_ZONES; i++) {
    rfidReaders[i]->begin(9600);
  }

  // Initialize relay pins
  for (int i = 0; i < NUM_ZONES; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW); // Ensure relays are off by default
  }

  // Initialize activity pins
  for (int i = 0; i < NUM_ZONES; i++) {
    pinMode(ACTIVITY_PINS[i], INPUT);
  }

  // Initialize WS2812B LEDs
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  for (int i = 0; i < NUM_ZONES; i++) {
    leds[i] = statusColors[IDLE]; // Default blue for all zones
  }
  FastLED.show();

  // Start Ethernet with DHCP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Flash all LEDs red to indicate failure
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Red;
    }
    FastLED.show();
    while (true);  // Stop here if DHCP fails
  }

  delay(1000); // Allow some time for DHCP to assign IP

  // Print network information
  printNetworkInfo();

  // Start NTP Client
  timeClient.begin();
  timeClient.setTimeOffset(0);  // Set time offset (UTC)

  // Initialize offline access list from server
  Serial.println("Initializing offline access list from server...");
  syncOfflineAccessList();
  
  // Initial setup complete
  Serial.println("Machine monitor initialized. Ready for RFID inputs on all zones.");
}

void loop() {
  // Check all RFID readers
  for (int zone = 0; zone < NUM_ZONES; zone++) {
    checkRFIDReader(zone);
  }

  // Monitor activity on all machines
  for (int zone = 0; zone < NUM_ZONES; zone++) {
    monitorActivity(zone);
  }

  // Check for timeouts and update LEDs for all machines
  for (int zone = 0; zone < NUM_ZONES; zone++) {
    checkTimeout(zone);
    updateLEDs(zone);
  }

  // Periodically update NTP time
  static unsigned long lastNtpUpdate = 0;
  if (millis() - lastNtpUpdate > 3600000) { // Once per hour
    timeClient.update();
    lastNtpUpdate = millis();
  }
  
  // Periodically synchronize the offline access card list
  static unsigned long lastOfflineSync = 0;
  if (millis() - lastOfflineSync > 3600000) { // Once per hour
    syncOfflineAccessList(); // Update offline access cards from server
    lastOfflineSync = millis();
  }
}

// Check RFID reader for a specific zone
void checkRFIDReader(int zone) {
  if (rfidReaders[zone]->available()) {
    // Buffer to store RFID data
    char rfidBuffer[16];
    int idx = 0;
    
    // Read until we get a newline or a timeout
    unsigned long startTime = millis();
    while (millis() - startTime < 500 && idx < 15) {
      if (rfidReaders[zone]->available()) {
        char c = rfidReaders[zone]->read();
        if (c == '\\n' || c == '\\r') {
          break;
        }
        rfidBuffer[idx++] = c;
      }
    }
    rfidBuffer[idx] = '\\0'; // Null-terminate the string
    
    // Convert to a String
    String rfid = String(rfidBuffer);
    rfid.trim();
    
    if (rfid.length() > 0) {
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.print(" RFID scanned: ");
      Serial.println(rfid);
      
      // Check with the server for access permission
      checkUserAccess(zone, rfid);
    }
  }
}

// Monitor activity on a specific machine
void monitorActivity(int zone) {
  // Check if the activity pin is high (machine in use)
  if (digitalRead(ACTIVITY_PINS[zone]) == HIGH) {
    // If the machine is active and has a user, reset timeout and send heartbeat
    if (machineStatus[zone] == ACCESS_GRANTED) {
      lastActivityTimes[zone] = millis();
      sendHeartbeat(zone);
    }
  }
}

// Check timeout status for a specific machine
void checkTimeout(int zone) {
  // Only check machines that have active users
  if (machineStatus[zone] == ACCESS_GRANTED) {
    unsigned long currentTime = millis();
    
    // Check for warning condition (approaching timeout)
    if (currentTime - lastActivityTimes[zone] > (activityTimeout - warningTimeout)) {
      flashingYellow[zone] = true;
    }
    
    // Check for timeout condition
    if (currentTime - lastActivityTimes[zone] > activityTimeout) {
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(" timed out due to inactivity.");
      logOutUser(zone);
    }
  }
  
  // Handle relay off delay
  if (machineStatus[zone] == LOGGED_OUT) {
    if (millis() - relayOffTimes[zone] > relayOffDelay) {
      digitalWrite(RELAY_PINS[zone], LOW);  // Deactivate relay after delay
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(" relay deactivated after delay.");
      
      // Back to idle state
      machineStatus[zone] = IDLE;
      leds[zone] = statusColors[IDLE];
      FastLED.show();
    }
  }
}

// Update LEDs for each zone
void updateLEDs(int zone) {
  // Handle flashing yellow warning
  if (flashingYellow[zone]) {
    // Flash the LED yellow for warning
    if (millis() - lastYellowFlashTimes[zone] > 500) {
      yellowLedStates[zone] = !yellowLedStates[zone];
      leds[zone] = yellowLedStates[zone] ? statusColors[4] : CRGB::Black;
      FastLED.show();
      lastYellowFlashTimes[zone] = millis();
    }
  }
}

// Function to check user access with the server
void checkUserAccess(int zone, String rfid) {
  if (client.connect(server, $SERVER_PORT$)) { // Connect to server port
    // Send a GET request to the server
    client.print("GET /api/check_user?rfid=" + rfid + "&machine_id=" + machineIDs[zone] + " HTTP/1.1\\r\\n");
    client.print("Host: ");
    client.print(server);
    client.print("\\r\\n");
    client.print("Connection: close\\r\\n\\r\\n");

    // Wait for the server's response
    String response = "";
    bool responseFound = false;
    
    unsigned long responseTimeout = millis() + 5000; // 5-second timeout
    while (client.connected() && millis() < responseTimeout) {
      if (client.available()) {
        char c = client.read();
        response += c;
        
        // Check if we've found our response code
        if (response.indexOf("ALLOW") != -1) {
          responseFound = true;
          
          // If another user is logged in, log them out first
          if (activeRFIDs[zone].length() > 0 && activeRFIDs[zone] != rfid) {
            sendLogoutRequest(zone);
            logOutUser(zone);
            delay(500); // Small delay to ensure logout is processed
          }
          
          // Grant access to the new user
          grantAccess(zone, rfid);
          break;
        } else if (response.indexOf("DENY") != -1) {
          responseFound = true;
          denyAccess(zone);
          break;
        } else if (response.indexOf("LOGOUT") != -1) {
          responseFound = true;
          logOutUser(zone);
          break;
        }
      }
    }
    
    client.stop();
    
    // If we didn't get a valid response
    if (!responseFound) {
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(" - No valid response from server");
      denyAccess(zone);
    }
  } else {
    Serial.print("Zone ");
    Serial.print(zone);
    Serial.println(" - Failed to connect to server, checking for offline access...");
    
    // Check if this RFID is stored in EEPROM as having offline access
    if (checkOfflineAccess(rfid, zone)) {
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(" - Offline access granted");
      grantAccess(zone, rfid);
    } else {
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(" - No offline access privileges");
      denyAccess(zone);
    }
  }
}

// Send a logout request to the server
void sendLogoutRequest(int zone) {
  if (activeRFIDs[zone].length() > 0) {
    if (client.connect(server, $SERVER_PORT$)) {
      client.print("GET /api/logout?rfid=" + activeRFIDs[zone] + "&machine_id=" + machineIDs[zone] + " HTTP/1.1\\r\\n");
      client.print("Host: ");
      client.print(server);
      client.print("\\r\\n");
      client.print("Connection: close\\r\\n\\r\\n");

      // Wait briefly for the server's response
      delay(200);
      client.stop();
      
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.print(" - Logout request sent for RFID: ");
      Serial.println(activeRFIDs[zone]);
    }
  }
}

// Send a heartbeat to the server to indicate machine activity
void sendHeartbeat(int zone) {
  if (activeRFIDs[zone].length() > 0 && machineStatus[zone] == ACCESS_GRANTED) {
    if (client.connect(server, $SERVER_PORT$)) {
      client.print("GET /api/heartbeat?machine_id=" + machineIDs[zone] + "&activity=1 HTTP/1.1\\r\\n");
      client.print("Host: ");
      client.print(server);
      client.print("\\r\\n");
      client.print("Connection: close\\r\\n\\r\\n");

      // Don't wait for response, just send and continue
      client.stop();
    }
  }
}

// Grant access to a machine
void grantAccess(int zone, String rfid) {
  machineStatus[zone] = ACCESS_GRANTED;
  activeRFIDs[zone] = rfid;        // Store the active RFID
  digitalWrite(RELAY_PINS[zone], HIGH); // Activate relay (turn on)
  
  // Set LED to green for access granted
  leds[zone] = statusColors[ACCESS_GRANTED];
  flashingYellow[zone] = false;
  FastLED.show();
  
  Serial.print("Zone ");
  Serial.print(zone);
  Serial.print(" - Access granted for RFID: ");
  Serial.println(rfid);
  
  lastActivityTimes[zone] = millis();   // Reset the activity timer
}

// Deny access to a machine
void denyAccess(int zone) {
  machineStatus[zone] = ACCESS_DENIED;
  
  // Set LED to red for access denied
  leds[zone] = statusColors[ACCESS_DENIED];
  FastLED.show();
  
  Serial.print("Zone ");
  Serial.print(zone);
  Serial.println(" - Access denied");
  
  // Flash red LED briefly
  for (int i = 0; i < 5; i++) {
    leds[zone] = CRGB::Black;
    FastLED.show();
    delay(100);
    leds[zone] = CRGB::Red;
    FastLED.show();
    delay(100);
  }
  
  // Return to blue idle state after denial
  delay(1000);
  leds[zone] = statusColors[IDLE];
  machineStatus[zone] = IDLE;
  FastLED.show();
}

// Log out the user from a machine
void logOutUser(int zone) {
  if (activeRFIDs[zone].length() > 0) {
    machineStatus[zone] = LOGGED_OUT;
    relayOffTimes[zone] = millis();  // Record the time when logout happens
    
    // Set LED to blue (waiting for next user)
    leds[zone] = statusColors[LOGGED_OUT];
    flashingYellow[zone] = false;
    FastLED.show();
    
    Serial.print("Zone ");
    Serial.print(zone);
    Serial.print(" - User with RFID: ");
    Serial.print(activeRFIDs[zone]);
    Serial.println(" logged out");
    
    activeRFIDs[zone] = "";  // Clear the active RFID
  }
}

// Check if an RFID card has offline access for a zone
bool checkOfflineAccess(String rfid, int zone) {
  // In a real implementation, this would check EEPROM or flash memory
  // for a list of offline access cards and their authorized zones
  
  // For emergency use, check if card has offline access and is authorized for this zone
  // We'll implement a simple hash-based checking system for demo purposes
  
  // Read cards with offline access from EEPROM
  // Format of EEPROM:
  // Each card entry is stored as a hash in EEPROM:
  // - Bytes 0-9: First 10 offline access cards (10 bytes total)
  // - Bytes 10-19: Corresponding machine authorizations (bit flags, 1 byte per card)
  
  // Demo implementation using a simple hash as a consistency check
  // (In a real implementation, you would use a proper hash function and verify credentials)
  byte cardHash = calculateCardHash(rfid);
  
  // Check through stored offline access cards in EEPROM
  for (int i = 0; i < 10; i++) {
    byte storedHash = EEPROM.read(i);
    
    // Skip empty entries (0xFF means uninitialized EEPROM)
    if (storedHash == 0xFF) {
      continue;
    }
    
    // If we find a matching hash
    if (storedHash == cardHash) {
      // Read machine authorizations (each bit represents a zone)
      byte auth = EEPROM.read(10 + i);
      
      // Check if this zone bit is set (0 = zone 0, 1 = zone 1, etc.)
      if (auth & (1 << zone)) {
        return true;  // Card has offline access for this zone
      }
      
      // Check for admin override (0x80 = high bit set)
      if (auth & 0x80) {
        return true;  // Admin override - access to all zones
      }
    }
  }
  
  return false; // No offline access
}

// Calculate a simple hash for a card ID (for demo purposes only)
byte calculateCardHash(String rfid) {
  byte hash = 0;
  for (int i = 0; i < rfid.length(); i++) {
    hash = (hash + rfid.charAt(i)) % 256;
  }
  return hash;
}

// This function is called periodically to sync server-side offline access lists
// during normal operation - ensuring offline access works when needed
void syncOfflineAccessList() {
  if (client.connect(server, $SERVER_PORT$)) {
    Serial.println("Connecting to server to sync offline access list...");
    
    client.print("GET /api/offline_cards HTTP/1.1\\r\\n");
    client.print("Host: ");
    client.print(server);
    client.print("\\r\\n");
    client.print("Connection: close\\r\\n\\r\\n");
    
    // Wait for server response
    unsigned long timeout = millis();
    String response = "";
    bool jsonStarted = false;
    
    while (client.connected() && millis() - timeout < 10000) {
      if (client.available()) {
        char c = client.read();
        
        // Only start recording when we reach the JSON part (after headers)
        if (c == '{') {
          jsonStarted = true;
        }
        
        if (jsonStarted) {
          response += c;
        }
      }
    }
    
    client.stop();
    
    // Process JSON if we have a valid response
    if (response.length() > 0 && response[0] == '{') {
      Serial.println("Received offline cards data from server");
      
      // Find the offline_cards array in the JSON
      int startPos = response.indexOf("\"offline_cards\":[");
      if (startPos != -1) {
        // Clear the current EEPROM storage for offline cards
        for (int i = 0; i < 20; i++) {
          EEPROM.write(i, 0xFF);  // 0xFF indicates empty slot
        }
        
        Serial.println("Processing offline cards...");
        
        // Simple JSON parsing - extract each card entry
        startPos = response.indexOf('{', startPos);
        int cardCount = 0;
        
        while (startPos != -1 && cardCount < 10) {
          int endPos = response.indexOf('}', startPos);
          if (endPos == -1) break;
          
          // Extract this card's JSON object
          String cardJson = response.substring(startPos, endPos + 1);
          
          // Extract hash and auth byte
          int hashPos = cardJson.indexOf("\"hash\":");
          int authPos = cardJson.indexOf("\"auth_byte\":");
          
          if (hashPos != -1 && authPos != -1) {
            // Extract hash value
            hashPos += 7;  // Skip "hash":
            int hashEndPos = cardJson.indexOf(',', hashPos);
            int hash = cardJson.substring(hashPos, hashEndPos).toInt();
            
            // Extract auth byte value
            authPos += 11;  // Skip "auth_byte":
            int authEndPos = cardJson.indexOf(',', authPos);
            if (authEndPos == -1) authEndPos = cardJson.indexOf('}', authPos);
            int auth = cardJson.substring(authPos, authEndPos).toInt();
            
            // Store in EEPROM if valid
            if (hash >= 0 && hash < 256 && auth >= 0 && auth < 256) {
              EEPROM.write(cardCount, (byte)hash);         // Store hash in first 10 bytes
              EEPROM.write(cardCount + 10, (byte)auth);    // Store auth in next 10 bytes
              Serial.print("Added offline card #");
              Serial.print(cardCount);
              Serial.print(" - Hash: ");
              Serial.print(hash);
              Serial.print(", Auth: ");
              Serial.println(auth);
              cardCount++;
            }
          }
          
          // Find the next card entry
          startPos = response.indexOf('{', endPos);
        }
        
        Serial.print("Successfully synchronized ");
        Serial.print(cardCount);
        Serial.println(" offline access cards");
      }
    } else {
      Serial.println("Failed to parse server response");
    }
  } else {
    Serial.println("Failed to connect to server for offline cards sync");
  }
}

// Print network information (IP, subnet, gateway, DNS)
void printNetworkInfo() {
  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());
  Serial.print("Subnet Mask: ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("DNS Server: ");
  Serial.println(Ethernet.dnsServerIP());
  Serial.print("Server: ");
  Serial.print(server);
  Serial.print(":");
  Serial.println($SERVER_PORT$);
}"""

        office_sketch = """#include <SPI.h>
#include <Ethernet.h>
#include <FastLED.h>

#define LED_PIN 6          // Pin for WS2812B LED status indicator
#define NUM_LEDS 1         // Number of WS2812B LEDs
#define BUTTON_PIN 4       // Pin for the registration button
#define RFID_SERIAL Serial // Simulating RFID using Serial input for testing

CRGB leds[NUM_LEDS];

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC Address for Ethernet
IPAddress server($SERVER_IP$); // Server IP address - will be replaced dynamically
EthernetClient client;

String lastScannedRFID = "";
bool readyForRegistration = false;
unsigned long buttonPressTime = 0;
unsigned long lastLedUpdate = 0;

void setup() {
  Serial.begin(9600);
  
  // Initialize button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Start Ethernet with DHCP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    while (true) {
      // Flash red LED to indicate failure
      leds[0] = CRGB::Red;
      FastLED.show();
      delay(300);
      leds[0] = CRGB::Black;
      FastLED.show();
      delay(300);
    }
  }

  delay(1000); // Allow some time for DHCP to assign IP

  // Print network information
  printNetworkInfo();

  // Initialize WS2812B LED
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  setLedColor(CRGB::Blue);  // Default color (waiting for button press)

  Serial.println("Office RFID Reader Ready");
  Serial.println("Press the button to enter registration mode.");
}

void loop() {
  // Check for button press to enter registration mode
  if (digitalRead(BUTTON_PIN) == LOW) {
    buttonPressTime = millis();
    while (digitalRead(BUTTON_PIN) == LOW) {
      // Wait for button release or long press
      if (millis() - buttonPressTime > 2000) {
        // Long press detected - cancel registration mode
        if (readyForRegistration) {
          readyForRegistration = false;
          lastScannedRFID = "";
          setLedColor(CRGB::Blue);
          Serial.println("Registration mode canceled");
          
          // Wait for button release
          while (digitalRead(BUTTON_PIN) == LOW) {
            delay(10);
          }
          
          break;
        }
      }
      delay(10);
    }
    
    // If it was a short press and we're not in registration mode, enter registration mode
    if (millis() - buttonPressTime < 2000 && !readyForRegistration) {
      readyForRegistration = true;
      Serial.println("Registration mode activated! Scan an RFID card now.");
      // Start flashing green indicating ready to scan
      setLedColor(CRGB::Green);
    }
  }
  
  // Flash LED when in registration mode
  if (readyForRegistration && millis() - lastLedUpdate > 500) {
    static bool ledOn = true;
    ledOn = !ledOn;
    if (ledOn) {
      setLedColor(CRGB::Green);
    } else {
      setLedColor(CRGB::Black);
    }
    lastLedUpdate = millis();
  }
  
  // Check for RFID scan
  if (RFID_SERIAL.available() && readyForRegistration) {
    // Read RFID from serial
    String rfid = RFID_SERIAL.readStringUntil('\\n');
    rfid.trim();  // Remove any extra spaces/newlines
    
    if (rfid.length() > 0) {
      lastScannedRFID = rfid;
      Serial.println("RFID Card Scanned: " + lastScannedRFID);
      Serial.println("Press the button again to confirm and send to server.");
      
      // Show solid purple to indicate card scanned, waiting for confirmation
      setLedColor(CRGB::Purple);
      readyForRegistration = false;
      
      // Wait for confirmation button press
      bool waiting = true;
      unsigned long waitStartTime = millis();
      
      while (waiting && millis() - waitStartTime < 30000) { // 30 second timeout
        // Check for button press to confirm
        if (digitalRead(BUTTON_PIN) == LOW) {
          delay(50); // Debounce
          
          // Make sure it's a real press
          if (digitalRead(BUTTON_PIN) == LOW) {
            // Wait for release
            while (digitalRead(BUTTON_PIN) == LOW) {
              delay(10);
            }
            
            // Send the RFID to server
            sendRFIDtoServer(lastScannedRFID);
            waiting = false;
          }
        }
        delay(10);
      }
      
      // If we timed out, reset
      if (waiting) {
        Serial.println("Confirmation timeout. Registration canceled.");
        lastScannedRFID = "";
        setLedColor(CRGB::Blue);
      }
    }
  }
}

// Send the RFID card to the server for registration
void sendRFIDtoServer(String rfid) {
  setLedColor(CRGB::Yellow); // Indicate we're connecting to server
  
  if (client.connect(server, $SERVER_PORT$)) { // Connect to server port
    Serial.println("Connected to server, sending RFID for registration...");
    
    // Send a POST request to the server
    client.println("POST /api/register_rfid HTTP/1.1");
    client.print("Host: ");
    client.println(server);
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    
    // Calculate content length
    String postData = "rfid=" + rfid;
    client.println(postData.length());
    client.println();
    client.println(postData);
    
    // Wait for response
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 10000) {
      if (client.available()) {
        String response = client.readString();
        
        if (response.indexOf("SUCCESS") != -1) {
          Serial.println("RFID registration successful!");
          successAnimation();
        } else {
          Serial.println("Failed to register RFID:");
          Serial.println(response);
          failureAnimation();
        }
        break;
      }
      delay(50);
    }
    
    // If we timed out
    if (millis() - timeout >= 10000) {
      Serial.println("Timeout waiting for server response");
      failureAnimation();
    }
    
    client.stop();
  } else {
    Serial.println("Failed to connect to server");
    failureAnimation();
  }
  
  // Reset to ready state
  setLedColor(CRGB::Blue);
}

// Success animation - flash green 3 times
void successAnimation() {
  for (int i = 0; i < 3; i++) {
    setLedColor(CRGB::Green);
    delay(200);
    setLedColor(CRGB::Black);
    delay(200);
  }
  setLedColor(CRGB::Green);
  delay(1000);
}

// Failure animation - flash red 3 times
void failureAnimation() {
  for (int i = 0; i < 3; i++) {
    setLedColor(CRGB::Red);
    delay(200);
    setLedColor(CRGB::Black);
    delay(200);
  }
  setLedColor(CRGB::Red);
  delay(1000);
}

// Set the color of the WS2812B LED
void setLedColor(CRGB color) {
  leds[0] = color;
  FastLED.show();
}

// Print network information (IP, subnet, gateway, DNS)
void printNetworkInfo() {
  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());
  Serial.print("Subnet Mask: ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("DNS Server: ");
  Serial.println(Ethernet.dnsServerIP());
}"""
    
    # Create example Raspberry Pi API code
    raspberry_api_example = """\"\"\"
Raspberry Pi Node Example for NooyenUSA RFID Machine Monitor

This example implements a simple machine monitor node that can be
integrated with the NooyenUSA RFID Machine Monitor system.

Requirements:
- Flask
- zeroconf
- requests
- RPi.GPIO (for Raspberry Pi GPIO access)
\"\"\"

import json
import os
import socket
import threading
import time
from datetime import datetime

import requests
from flask import Flask, jsonify, request
from zeroconf import ServiceInfo, Zeroconf

# Configuration
NODE_ID = "rpi-machine-1"
NODE_TYPE = "machine_monitor"
NODE_NAME = "Raspberry Pi Machine Monitor"
SERVER_URL = "http://{server_ip}:{server_port}"
LISTEN_PORT = 5050

# Node state
node_config = {{
    "node_id": NODE_ID,
    "node_type": NODE_TYPE,
    "name": NODE_NAME,
    "server_url": SERVER_URL,
    "machines": [
        {{
            "machine_id": "01",
            "zone": 0,
            "name": "Table Saw"
        }}
    ]
}}

node_status = {{
    "node_id": NODE_ID,
    "node_type": NODE_TYPE,
    "name": NODE_NAME,
    "ip_address": "0.0.0.0",  # Will be updated on startup
    "status": "online",
    "machines": [
        {{
            "zone": 0,
            "machine_id": "01",
            "status": "idle",
            "rfid": "",
            "activity_count": 0
        }}
    ]
}}

# Flask application
app = Flask(__name__)

# API Endpoints
@app.route('/api/status', methods=['GET'])
def get_status():
    \"\"\"Return the current status of the node\"\"\"
    return jsonify(node_status)

@app.route('/api/config', methods=['GET'])
def get_config():
    \"\"\"Return the current configuration of the node\"\"\"
    return jsonify(node_config)

# More API endpoints and implementation details...

if __name__ == '__main__':
    # Register mDNS service for discovery
    # Start background tasks
    # Run the Flask application
    app.run(host='0.0.0.0', port=LISTEN_PORT)
""".format(server_ip=server_ip, server_port=server_port)
    
    return render_template(
        'node_sketches.html',
        server_ip=server_ip,
        server_port=server_port,
        esp32_available=True,
        esp32_sketch=esp32_sketch,
        esp32_wiring=esp32_wiring,
        raspberry_api_example=raspberry_api_example
    )

# API endpoint to get the server's IP address
@app.route('/api/server_ip', methods=['GET'])
@login_required
@admin_required
def get_server_ip():
    # Get the server's IP address
    host = request.environ.get('HTTP_HOST', '127.0.0.1').split(':')[0]
    return jsonify({'ip': host})
    
# RFID registration web interface
@app.route('/register_rfid')
@login_required
@admin_required
def web_register_rfid():
    return render_template('register_rfid.html')

# API endpoint to check office reader for new RFID tags
@app.route('/api/check_office_reader')
@login_required
@admin_required
def check_office_reader():
    """Check if the office RFID reader has detected a new tag"""
    # In a real implementation, this would check the hardware reader
    # For now, we'll use the session to simulate tag detection
    pending_rfid = session.get('pending_rfid')
    
    # Check if the tag has already been reported
    reported = session.get('rfid_reported', False)
    
    # Reset the reported status and clear tag if requested
    if request.args.get('reset') == 'true':
        session['rfid_reported'] = False
        session.pop('pending_rfid', None)  # Clear the pending RFID tag
        pending_rfid = None
        reported = False
    
    # For demo purposes, simulate occasional RFID detections
    if not pending_rfid:
        import random
        if random.random() < 0.2:  # 20% chance of detecting a tag
            new_tag = f"RFID-{random.randint(1000, 9999)}"
            session['pending_rfid'] = new_tag
            session['rfid_reported'] = False  # New tag, not reported yet
            return jsonify({
                'status': 'detected',
                'rfid_tag': new_tag
            })
    
    if pending_rfid and not reported:
        # Mark this tag as reported to prevent repeated notifications
        session['rfid_reported'] = True
        return jsonify({
            'status': 'detected',
            'rfid_tag': pending_rfid
        })
    elif pending_rfid and reported:
        # Tag already reported, just report it as ready for registration
        return jsonify({
            'status': 'ready',
            'rfid_tag': pending_rfid
        })
    else:
        return jsonify({
            'status': 'waiting'
        })
    
# API endpoint to get offline access cards for Arduino
@app.route('/api/offline_cards', methods=['GET'])
def offline_cards():
    # Get all offline access and admin override cards
    offline_cards = RFIDUser.query.filter(
        (RFIDUser.is_offline_access == True) | 
        (RFIDUser.is_admin_override == True)
    ).filter_by(active=True).all()
    
    # Format for Arduino EEPROM storage
    result = []
    for i, card in enumerate(offline_cards[:10]):  # Limit to 10 cards
        # Get machine authorizations for this card
        auth_machines = [auth.machine_id for auth in card.authorized_machines]
        
        # Create authorization byte (bit field)
        auth_byte = 0
        for machine in auth_machines:
            # Get the zone (0-3) for this machine
            machine_obj = Machine.query.get(machine)
            if machine_obj and machine_obj.machine_id.isdigit():
                zone = int(machine_obj.machine_id) - 1  # Convert from 1-based to 0-based
                if 0 <= zone < 4:  # Only 4 zones supported
                    auth_byte |= (1 << zone)  # Set bit for this zone
        
        # Set high bit for admin override cards
        if card.is_admin_override:
            auth_byte |= 0x80  # Set high bit (admin override)
        
        # Calculate hash for this card
        card_hash = sum(ord(c) for c in card.rfid_tag) % 256
        
        result.append({
            "index": i,
            "rfid": card.rfid_tag,
            "hash": card_hash,
            "auth_byte": auth_byte,
            "admin_override": card.is_admin_override
        })
    
    # Return as JSON
    return jsonify({"offline_cards": result})

# API endpoint for registering RFID cards from office reader
@app.route('/api/register_rfid', methods=['POST'])
def api_register_rfid():
    rfid = request.form.get('rfid')
    
    if not rfid:
        return "ERROR: No RFID provided", 400
    
    # Check if RFID already exists
    existing_user = RFIDUser.query.filter_by(rfid_tag=rfid).first()
    if existing_user:
        return "ERROR: RFID already registered", 409
    
    # Store the RFID in the session for admin to complete registration
    session['pending_rfid'] = rfid
    
    return "SUCCESS: RFID pending registration", 200

# Page for completing RFID registration
@app.route('/complete_rfid_registration', methods=['GET', 'POST'])
@login_required
@admin_required
def complete_rfid_registration():
    pending_rfid = session.get('pending_rfid')
    
    if not pending_rfid:
        flash('No RFID card waiting for registration', 'warning')
        return redirect(url_for('users'))
    
    if request.method == 'POST':
        name = request.form.get('name')
        email = request.form.get('email', '')
        
        if not name:
            flash('Name is required', 'danger')
            return render_template('complete_registration.html', pending_rfid=pending_rfid)
        
        # Get offline access options
        is_offline_access = 'is_offline_access' in request.form
        is_admin_override = 'is_admin_override' in request.form
        
        # Create new RFID user
        new_user = RFIDUser(
            rfid_tag=pending_rfid,
            name=name,
            email=email,
            active=True,
            is_offline_access=is_offline_access,
            is_admin_override=is_admin_override
        )
        
        db.session.add(new_user)
        db.session.commit()
        
        # Push the new user to NooyenUSATracker if integration is configured
        try:
            from integration.nooyen_integration import push_user_changes
            push_user_changes(new_user)
        except Exception as e:
            app.logger.error(f"Failed to push user changes: {str(e)}")
        
        # Clear the pending RFID from session
        session.pop('pending_rfid', None)
        
        flash(f'User {name} registered successfully with RFID: {pending_rfid}', 'success')
        return redirect(url_for('users'))
    
    return render_template('complete_registration.html', pending_rfid=pending_rfid)
