from flask import Blueprint, render_template, request, redirect, url_for, flash, jsonify
from flask_login import login_required, current_user
import datetime
from app import db
from models import Area, Zone, AreaPermission, Machine, Node, MachineSession
from forms import AreaForm, AreaPermissionForm

# Create a blueprint for area management routes
areas_bp = Blueprint('areas', __name__)

@areas_bp.route('/areas')
@login_required
def list_areas():
    """List all areas the user has access to"""
    # Check if user is admin
    is_admin = current_user.role and current_user.role.name == 'admin'
    
    if is_admin:
        # Admin sees all areas
        areas = Area.query.all()
    else:
        # Regular users see only areas they have permission for
        permissions = AreaPermission.query.filter_by(user_id=current_user.id).all()
        area_ids = [p.area_id for p in permissions]
        areas = Area.query.filter(Area.id.in_(area_ids)).all()
    
    return render_template('areas/area_list.html', areas=areas, is_admin=is_admin)

@areas_bp.route('/areas/new', methods=['GET', 'POST'])
@login_required
def new_area():
    """Create a new area"""
    # Check if user is admin
    if not current_user.role or current_user.role.name != 'admin':
        flash('You do not have permission to create areas.', 'danger')
        return redirect(url_for('areas.list_areas'))
    
    form = AreaForm()
    
    if form.validate_on_submit():
        area = Area(
            name=form.name.data,
            description=form.description.data,
            code=form.code.data,
            active=True
        )
        db.session.add(area)
        db.session.commit()
        
        flash(f'Area "{area.name}" created successfully!', 'success')
        return redirect(url_for('areas.list_areas'))
    
    return render_template('areas/area_form.html', form=form, area=None)

@areas_bp.route('/areas/<int:area_id>/edit', methods=['GET', 'POST'])
@login_required
def edit_area(area_id):
    """Edit an existing area"""
    # Check if user is admin
    if not current_user.role or current_user.role.name != 'admin':
        flash('You do not have permission to edit areas.', 'danger')
        return redirect(url_for('areas.list_areas'))
    
    area = Area.query.get_or_404(area_id)
    form = AreaForm(obj=area)
    
    if form.validate_on_submit():
        area.name = form.name.data
        area.description = form.description.data
        area.code = form.code.data
        area.active = form.active.data
        
        db.session.commit()
        flash(f'Area "{area.name}" updated successfully!', 'success')
        return redirect(url_for('areas.list_areas'))
    
    return render_template('areas/area_form.html', form=form, area=area)

@areas_bp.route('/areas/<int:area_id>')
@login_required
def view_area(area_id):
    """View area details and contained locations/zones"""
    area = Area.query.get_or_404(area_id)
    
    # Check permission
    is_admin = current_user.role and current_user.role.name == 'admin'
    has_permission = is_admin or current_user.has_area_permission(area_id)
    
    if not has_permission:
        flash('You do not have permission to view this area.', 'danger')
        return redirect(url_for('areas.list_areas'))
    
    zones = Zone.query.filter_by(area_id=area_id).all()
    nodes = Node.query.filter_by(area_id=area_id).all()
    
    # Count machines in this area
    machine_count = 0
    for zone in zones:
        machine_count += Machine.query.filter_by(zone_id=zone.id).count()
    
    return render_template(
        'areas/area_detail.html', 
        area=area, 
        zones=zones, 
        nodes=nodes, 
        machine_count=machine_count,
        is_admin=is_admin
    )

@areas_bp.route('/areas/<int:area_id>/permissions', methods=['GET', 'POST'])
@login_required
def manage_permissions(area_id):
    """Manage user permissions for this area"""
    # Check if user is admin
    if not current_user.role or current_user.role.name != 'admin':
        flash('You do not have permission to manage area permissions.', 'danger')
        return redirect(url_for('areas.view_area', area_id=area_id))
    
    area = Area.query.get_or_404(area_id)
    permissions = AreaPermission.query.filter_by(area_id=area_id).all()
    
    form = AreaPermissionForm()
    
    if form.validate_on_submit():
        # Check if permission already exists
        existing = AreaPermission.query.filter_by(
            user_id=form.user_id.data,
            area_id=area_id
        ).first()
        
        if existing:
            existing.permission_level = form.permission_level.data
            flash(f'Permission updated for user.', 'success')
        else:
            permission = AreaPermission(
                user_id=form.user_id.data,
                area_id=area_id,
                permission_level=form.permission_level.data
            )
            db.session.add(permission)
            flash(f'Permission added for user.', 'success')
        
        db.session.commit()
        return redirect(url_for('areas.manage_permissions', area_id=area_id))
    
    return render_template(
        'areas/area_permissions.html',
        area=area,
        permissions=permissions,
        form=form
    )

@areas_bp.route('/areas/<int:area_id>/permissions/<int:permission_id>/delete', methods=['POST'])
@login_required
def delete_permission(area_id, permission_id):
    """Delete a permission for this area"""
    # Check if user is admin
    if not current_user.role or current_user.role.name != 'admin':
        flash('You do not have permission to manage area permissions.', 'danger')
        return redirect(url_for('areas.view_area', area_id=area_id))
    
    permission = AreaPermission.query.get_or_404(permission_id)
    
    # Ensure permission belongs to this area
    if permission.area_id != area_id:
        flash('Invalid permission ID for this area.', 'danger')
        return redirect(url_for('areas.manage_permissions', area_id=area_id))
    
    db.session.delete(permission)
    db.session.commit()
    
    flash('Permission removed.', 'success')
    return redirect(url_for('areas.manage_permissions', area_id=area_id))

@areas_bp.route('/api/areas')
@login_required
def api_list_areas():
    """API endpoint to list areas (for dropdown menus, etc.)"""
    # Check if user is admin
    is_admin = current_user.role and current_user.role.name == 'admin'
    
    if is_admin:
        # Admin sees all areas
        areas = Area.query.all()
    else:
        # Regular users see only areas they have permission for
        permissions = AreaPermission.query.filter_by(user_id=current_user.id).all()
        area_ids = [p.area_id for p in permissions]
        areas = Area.query.filter(Area.id.in_(area_ids)).all()
    
    return jsonify([{
        'id': area.id,
        'name': area.name,
        'code': area.code,
        'active': area.active,
        'zone_count': area.zones.count(),
        'machine_count': area.machine_count
    } for area in areas])

@areas_bp.route('/api/areas/<int:area_id>/zones')
@login_required
def api_list_area_zones(area_id):
    """API endpoint to list zones in an area (for dropdown menus, etc.)"""
    # Check permission
    is_admin = current_user.role and current_user.role.name == 'admin'
    has_permission = is_admin or current_user.has_area_permission(area_id)
    
    if not has_permission:
        return jsonify({'error': 'Permission denied'}), 403
    
    zones = Zone.query.filter_by(area_id=area_id).all()
    
    return jsonify([{
        'id': zone.id,
        'name': zone.name,
        'code': zone.code,
        'active': zone.active,
        'machine_count': zone.machines.count()
    } for zone in zones])

@areas_bp.route('/areas/<int:area_id>/analytics')
@login_required
def area_analytics(area_id):
    """View analytics dashboard for a specific area"""
    area = Area.query.get_or_404(area_id)
    
    # Check permission
    is_admin = current_user.role and current_user.role.name == 'admin'
    has_permission = is_admin or current_user.has_area_permission(area_id)
    
    if not has_permission:
        flash('You do not have permission to view analytics for this area.', 'danger')
        return redirect(url_for('areas.list_areas'))
    
    return render_template('areas/area_analytics.html', area=area, is_admin=is_admin)

@areas_bp.route('/api/areas/<int:area_id>/analytics')
@login_required
def api_area_analytics(area_id):
    """API endpoint for area analytics data"""
    # Check permission
    is_admin = current_user.role and current_user.role.name == 'admin'
    has_permission = is_admin or current_user.has_area_permission(area_id)
    
    if not has_permission:
        return jsonify({'error': 'Permission denied'}), 403
    
    # Get query parameters
    days = request.args.get('days', default=30, type=int)
    view_type = request.args.get('view', default='daily', type=str)
    
    # Calculate start date based on requested days
    end_date = datetime.datetime.utcnow()
    start_date = end_date - datetime.timedelta(days=days)
    
    # Get all zones in this area
    zones = Zone.query.filter_by(area_id=area_id).all()
    zone_ids = [zone.id for zone in zones]
    
    # Get machines in these zones
    machines = Machine.query.filter(Machine.zone_id.in_(zone_ids)).all()
    machine_ids = [machine.id for machine in machines]
    
    # Get machine session data for analytics
    from sqlalchemy import func
    from models import MachineLog

    # Base query for sessions in this area during the time period
    base_query = MachineLog.query.join(Machine).filter(
        Machine.id.in_(machine_ids),
        MachineLog.login_time >= start_date,
        MachineLog.login_time <= end_date
    )
    
    # Calculate summary metrics
    total_sessions = base_query.count()
    
    # Initialize with default values in case there are no sessions
    total_hours = 0
    avg_duration = 0
    
    if total_sessions > 0:
        # For completed sessions, use the actual duration
        completed_logs = base_query.filter(MachineLog.logout_time.isnot(None))
        completed_hours = db.session.query(
            func.sum(MachineLog.total_time / 3600)
        ).select_from(MachineLog).filter(
            MachineLog.machine_id.in_(machine_ids),
            MachineLog.login_time >= start_date,
            MachineLog.logout_time.isnot(None)
        ).scalar() or 0
        
        # For active sessions, calculate duration up to now
        active_logs = base_query.filter(MachineLog.logout_time.is_(None)).all()
        active_hours = sum((end_date - log.login_time).total_seconds() / 3600 for log in active_logs)
        
        # Total hours is the sum of completed and active hours
        total_hours = completed_hours + active_hours
        
        # Average duration in minutes
        if total_sessions > 0:
            avg_duration = (total_hours * 60) / total_sessions
    
    # Count active machines (those with current sessions)
    active_machine_count = db.session.query(func.count(func.distinct(MachineSession.machine_id)))\
        .join(Machine)\
        .filter(
            Machine.id.in_(machine_ids),
            MachineSession.logout_time.is_(None)
        ).scalar() or 0
    
    # Usage time series data (daily or hourly)
    time_series = generate_time_series(machine_ids, start_date, end_date, view_type)
    
    # Usage by location (zone)
    location_usage = generate_location_usage(zone_ids, start_date, end_date)
    
    # Machine-specific metrics
    machine_metrics = generate_machine_metrics(machine_ids, start_date, end_date)
    
    return jsonify({
        'summary': {
            'total_sessions': total_sessions,
            'total_hours': float(total_hours),
            'avg_duration': float(avg_duration),
            'active_machines': active_machine_count
        },
        'time_series': time_series,
        'location_usage': location_usage,
        'machines': machine_metrics
    })

def generate_time_series(machine_ids, start_date, end_date, view_type='daily'):
    """Generate time series data for machine usage"""
    from sqlalchemy import func
    from models import MachineLog
    import pandas as pd
    
    # Format string for grouping by date
    if view_type == 'hourly':
        date_format = '%Y-%m-%d %H:00'  # Group by hour
        date_trunc = func.strftime('%Y-%m-%d %H:00', MachineLog.login_time)
    else:
        date_format = '%Y-%m-%d'  # Group by day
        date_trunc = func.strftime('%Y-%m-%d', MachineLog.login_time)
    
    # Query for completed sessions
    completed_results = db.session.query(
        date_trunc.label('date'),
        func.sum(MachineLog.total_time / 3600).label('hours')
    ).filter(
        MachineLog.machine_id.in_(machine_ids),
        MachineLog.login_time >= start_date,
        MachineLog.login_time <= end_date,
        MachineLog.logout_time.isnot(None)
    ).group_by('date').all()
    
    # Convert to dictionary for easy lookup
    time_data = {row.date: row.hours for row in completed_results}
    
    # Create the full date range for the requested period
    date_range = []
    if view_type == 'hourly':
        # For hourly view, generate all hours in the requested period
        current = start_date
        while current <= end_date:
            date_str = current.strftime(date_format)
            date_range.append(date_str)
            current += datetime.timedelta(hours=1)
    else:
        # For daily view, generate all days in the requested period
        current = start_date
        while current <= end_date:
            date_str = current.strftime(date_format)
            date_range.append(date_str)
            current += datetime.timedelta(days=1)
    
    # Fill in missing dates with zeros
    values = [float(time_data.get(date, 0)) for date in date_range]
    
    return {
        'labels': date_range,
        'values': values
    }

def generate_location_usage(zone_ids, start_date, end_date):
    """Generate usage data by location (zone)"""
    from sqlalchemy import func
    from models import MachineLog, Zone
    
    # Query for hours by zone
    results = db.session.query(
        Zone.name,
        func.sum(MachineLog.total_time / 3600).label('hours')
    ).join(
        Machine, Machine.id == MachineLog.machine_id
    ).join(
        Zone, Zone.id == Machine.zone_id
    ).filter(
        Zone.id.in_(zone_ids),
        MachineLog.login_time >= start_date,
        MachineLog.login_time <= end_date,
        MachineLog.logout_time.isnot(None)
    ).group_by(Zone.id).all()
    
    # Convert to lists for Chart.js
    labels = [row.name for row in results]
    values = [float(row.hours) for row in results]
    
    return {
        'labels': labels,
        'values': values
    }

def generate_machine_metrics(machine_ids, start_date, end_date):
    """Generate metrics for each machine in the area"""
    from sqlalchemy import func
    from models import MachineLog, MachineSession
    
    # Get machines
    machines = Machine.query.filter(Machine.id.in_(machine_ids)).all()
    machine_metrics = []
    
    for machine in machines:
        # Count sessions
        session_count = MachineLog.query.filter(
            MachineLog.machine_id == machine.id,
            MachineLog.login_time >= start_date,
            MachineLog.login_time <= end_date
        ).count()
        
        # Calculate hours
        total_hours = db.session.query(
            func.sum(MachineLog.total_time / 3600)
        ).filter(
            MachineLog.machine_id == machine.id,
            MachineLog.login_time >= start_date,
            MachineLog.login_time <= end_date,
            MachineLog.logout_time.isnot(None)
        ).scalar() or 0
        
        # Calculate average session duration (in minutes)
        avg_duration = 0
        if session_count > 0:
            avg_seconds = db.session.query(
                func.avg(MachineLog.total_time)
            ).filter(
                MachineLog.machine_id == machine.id,
                MachineLog.login_time >= start_date,
                MachineLog.logout_time.isnot(None)
            ).scalar() or 0
            avg_duration = avg_seconds / 60  # Convert to minutes
        
        # Get last used timestamp
        last_used = db.session.query(
            func.max(MachineLog.login_time)
        ).filter(
            MachineLog.machine_id == machine.id
        ).scalar()
        
        # Determine if machine is currently active
        is_active = db.session.query(MachineSession).filter(
            MachineSession.machine_id == machine.id,
            MachineSession.logout_time.is_(None)
        ).count() > 0
        
        status = machine.status
        if is_active and status == 'idle':
            status = 'active'
        
        machine_metrics.append({
            'id': machine.id,
            'machine_id': machine.machine_id,
            'name': machine.name,
            'zone_name': machine.zone_name,
            'sessions': session_count,
            'total_hours': float(total_hours),
            'avg_duration': float(avg_duration),
            'last_used': last_used.isoformat() if last_used else None,
            'status': status
        })
    
    return machine_metrics

@areas_bp.route('/areas/<int:area_id>/lead_operators')
@login_required
def area_lead_operators(area_id):
    """View lead operators dashboard for a specific area"""
    area = Area.query.get_or_404(area_id)
    
    # Check permission
    is_admin = current_user.role and current_user.role.name == 'admin'
    has_permission = is_admin or current_user.has_area_permission(area_id)
    
    if not has_permission:
        flash('You do not have permission to view lead operators for this area.', 'danger')
        return redirect(url_for('areas.list_areas'))
    
    return render_template('areas/area_lead_operators.html', area=area, is_admin=is_admin)

@areas_bp.route('/api/areas/<int:area_id>/machines/lead_status')
@login_required
def api_area_machine_lead_status(area_id):
    """API endpoint to get lead operator status for all machines in an area"""
    # Check permission
    is_admin = current_user.role and current_user.role.name == 'admin'
    has_permission = is_admin or current_user.has_area_permission(area_id)
    
    if not has_permission:
        return jsonify({'error': 'Permission denied'}), 403
    
    # Get all zones in this area
    zones = Zone.query.filter_by(area_id=area_id).all()
    zone_ids = [zone.id for zone in zones]
    
    # Get all machines in these zones
    machines = Machine.query.filter(Machine.zone_id.in_(zone_ids)).all()
    
    # Collect machine data with lead operator information
    machine_data = []
    machines_with_lead = 0
    active_users_count = 0
    
    for machine in machines:
        # Get active sessions for this machine
        active_sessions = MachineSession.query.filter_by(
            machine_id=machine.id,
            logout_time=None
        ).all()
        
        # Get lead operator details if available
        lead_operator = None
        lead_since = None
        
        if machine.lead_operator_id:
            machines_with_lead += 1
            lead = machine.lead_operator
            lead_operator = {
                'id': lead.id,
                'name': lead.name,
                'rfid_tag': lead.rfid_tag
            }
            
            # Find when this lead was assigned
            lead_history = LeadOperatorHistory.query.filter_by(
                machine_id=machine.id,
                user_id=lead.id,
                removed_time=None
            ).order_by(LeadOperatorHistory.assigned_time.desc()).first()
            
            if lead_history:
                lead_since = lead_history.assigned_time.isoformat()
        
        # Track active users
        active_users_count += len(active_sessions)
        
        # Format active sessions
        session_data = []
        for session in active_sessions:
            session_data.append({
                'id': session.id,
                'user_id': session.user_id,
                'user_name': session.user.name,
                'is_lead': session.is_lead
            })
        
        machine_data.append({
            'id': machine.id,
            'machine_id': machine.machine_id,
            'name': machine.name,
            'zone_id': machine.zone_id,
            'zone_name': machine.zone.name if machine.zone else "Unassigned",
            'status': machine.status,
            'lead_operator': lead_operator,
            'lead_since': lead_since,
            'active_sessions': session_data
        })
    
    # Get all locations/zones for filtering
    locations = [{
        'id': zone.id,
        'name': zone.name,
        'code': zone.code,
        'machine_count': Machine.query.filter_by(zone_id=zone.id).count()
    } for zone in zones]
    
    return jsonify({
        'total_machines': len(machines),
        'machines_with_lead': machines_with_lead,
        'machines_without_lead': len(machines) - machines_with_lead,
        'active_users': active_users_count,
        'machines': machine_data,
        'locations': locations
    })

@areas_bp.route('/api/areas/<int:area_id>/lead_history')
@login_required
def api_area_lead_history(area_id):
    """API endpoint to get lead operator history for an area"""
    # Check permission
    is_admin = current_user.role and current_user.role.name == 'admin'
    has_permission = is_admin or current_user.has_area_permission(area_id)
    
    if not has_permission:
        return jsonify({'error': 'Permission denied'}), 403
    
    # Get all zones in this area
    zones = Zone.query.filter_by(area_id=area_id).all()
    zone_ids = [zone.id for zone in zones]
    
    # Get all machines in these zones
    machines = Machine.query.filter(Machine.zone_id.in_(zone_ids)).all()
    machine_ids = [machine.id for machine in machines]
    
    # Get recent lead operator history
    history_entries = LeadOperatorHistory.query.filter(
        LeadOperatorHistory.machine_id.in_(machine_ids)
    ).order_by(LeadOperatorHistory.assigned_time.desc()).limit(50).all()
    
    # Format history data
    history_data = []
    for entry in history_entries:
        # Find the machine
        machine = next((m for m in machines if m.id == entry.machine_id), None)
        if not machine:
            continue
            
        # Find previous lead operator
        previous_lead_name = None
        if entry.removal_reason != 'login':  # If not initial login, there was a previous lead
            previous_entry = LeadOperatorHistory.query.filter(
                LeadOperatorHistory.machine_id == entry.machine_id,
                LeadOperatorHistory.assigned_time < entry.assigned_time
            ).order_by(LeadOperatorHistory.assigned_time.desc()).first()
            
            if previous_entry:
                previous_lead = RFIDUser.query.get(previous_entry.user_id)
                if previous_lead:
                    previous_lead_name = previous_lead.name
        
        history_data.append({
            'machine_id': machine.machine_id,
            'machine_name': machine.name,
            'assigned_time': entry.assigned_time.isoformat(),
            'new_lead': entry.user.name,
            'previous_lead': previous_lead_name,
            'assigned_by': entry.assigned_by.name if entry.assigned_by else None,
            'reason': entry.removal_reason
        })
    
    return jsonify({
        'history': history_data
    })

@areas_bp.route('/api/machines/<int:machine_id>/eligible_leads')
@login_required
def api_machine_eligible_leads(machine_id):
    """API endpoint to get eligible lead operators for a machine"""
    machine = Machine.query.get_or_404(machine_id)
    
    # Check permission
    is_admin = current_user.role and current_user.role.name == 'admin'
    if not is_admin and machine.zone and machine.zone.area:
        has_permission = current_user.has_area_permission(machine.zone.area.id)
        if not has_permission:
            return jsonify({'error': 'Permission denied'}), 403
    
    # Get users who can be lead operators for this machine
    eligible_leads = db.session.query(RFIDUser).join(
        MachineAuthorization,
        MachineAuthorization.user_id == RFIDUser.id
    ).filter(
        MachineAuthorization.machine_id == machine.id,
        MachineAuthorization.can_be_lead == True,
        RFIDUser.can_be_lead == True,
        RFIDUser.active == True
    ).all()
    
    return jsonify({
        'machine_id': machine.id,
        'machine_name': machine.name,
        'current_lead_id': machine.lead_operator_id,
        'eligible_leads': [{
            'id': user.id,
            'name': user.name,
            'rfid_tag': user.rfid_tag
        } for user in eligible_leads]
    })

@areas_bp.route('/api/machines/<int:machine_id>/assign_lead', methods=['POST'])
@login_required
def api_assign_lead_operator(machine_id):
    """API endpoint to assign a lead operator to a machine"""
    machine = Machine.query.get_or_404(machine_id)
    
    # Check permission
    is_admin = current_user.role and current_user.role.name == 'admin'
    can_manage = is_admin
    
    if machine.zone and machine.zone.area:
        can_manage = can_manage or current_user.has_area_permission(machine.zone.area.id, 'manage')
    
    if not can_manage:
        return jsonify({'error': 'Permission denied'}), 403
    
    data = request.json
    if not data or 'user_id' not in data:
        return jsonify({'error': 'Missing user_id parameter'}), 400
    
    user_id = data['user_id']
    user = RFIDUser.query.get(user_id)
    
    if not user:
        return jsonify({'error': 'User not found'}), 404
    
    # Check if user can be lead operator
    auth = MachineAuthorization.query.filter_by(
        machine_id=machine.id,
        user_id=user.id
    ).first()
    
    if not auth or not auth.can_be_lead or not user.can_be_lead:
        return jsonify({'error': 'User cannot be a lead operator for this machine'}), 400
    
    # Check if user is already logged in
    session = MachineSession.query.filter_by(
        machine_id=machine.id,
        user_id=user.id,
        logout_time=None
    ).first()
    
    if not session:
        # Create a session for the user
        session = MachineSession(
            machine_id=machine.id,
            user_id=user.id,
            is_lead=True,
            login_time=datetime.datetime.now()
        )
        db.session.add(session)
    else:
        # Update existing session to make user a lead
        session.is_lead = True
    
    # If there's a current lead operator, demote them
    if machine.lead_operator_id and machine.lead_operator_id != user.id:
        current_lead_session = MachineSession.query.filter_by(
            machine_id=machine.id,
            user_id=machine.lead_operator_id,
            logout_time=None
        ).first()
        
        if current_lead_session:
            current_lead_session.is_lead = False
            
            # Complete the lead history entry
            lead_history = LeadOperatorHistory.query.filter_by(
                machine_id=machine.id,
                user_id=machine.lead_operator_id,
                removed_time=None
            ).order_by(LeadOperatorHistory.assigned_time.desc()).first()
            
            if lead_history:
                lead_history.removed_time = datetime.datetime.now()
                lead_history.removal_reason = 'reassigned'
        
        previous_lead_name = machine.lead_operator.name
    else:
        previous_lead_name = None
    
    # Update machine's lead operator
    machine.lead_operator_id = user.id
    
    # Create lead history entry
    lead_history = LeadOperatorHistory(
        machine_id=machine.id,
        user_id=user.id,
        assigned_time=datetime.datetime.now(),
        assigned_by_id=current_user.id
    )
    db.session.add(lead_history)
    
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'message': f'{user.name} assigned as lead operator for {machine.name}',
        'previous_lead': previous_lead_name,
        'new_lead': user.name
    })