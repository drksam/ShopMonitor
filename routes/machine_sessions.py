from flask import Blueprint, render_template, request, redirect, url_for, flash, jsonify
from flask_login import login_required, current_user
from app import db
from models import Machine, RFIDUser, MachineSession, LeadOperatorHistory, MachineAuthorization
from forms import MultiUserLoginForm, LeadOperatorForm, QualityReportForm
from datetime import datetime

# Create a blueprint for machine session management
machine_sessions_bp = Blueprint('machine_sessions', __name__)

@machine_sessions_bp.route('/machines/<int:machine_id>/login', methods=['GET', 'POST'])
@login_required
def login_to_machine(machine_id):
    """Log a user into a machine (can be multi-user)"""
    machine = Machine.query.get_or_404(machine_id)
    
    # Check if user has permission to view this machine's area/zone
    is_admin = current_user.role and current_user.role.name == 'admin'
    if not is_admin and machine.zone and machine.zone.area:
        has_permission = current_user.has_area_permission(machine.zone.area.id)
        if not has_permission:
            flash('You do not have permission to access this machine.', 'danger')
            return redirect(url_for('machines.list_machines'))
    
    form = MultiUserLoginForm()
    form.machine_id.data = machine.id
    
    if form.validate_on_submit():
        # Get user by RFID tag
        user = RFIDUser.query.filter_by(rfid_tag=form.rfid_tag.data).first()
        
        if not user:
            flash('Invalid RFID tag. User not found.', 'danger')
            return redirect(url_for('machine_sessions.login_to_machine', machine_id=machine.id))
        
        # Check if user is authorized to use this machine
        auth = MachineAuthorization.query.filter_by(
            machine_id=machine.id,
            user_id=user.id
        ).first()
        
        if not auth:
            flash(f'User {user.name} is not authorized to use this machine.', 'danger')
            return redirect(url_for('machine_sessions.login_to_machine', machine_id=machine.id))
        
        # Check if user is already logged in to this machine
        existing_session = MachineSession.query.filter_by(
            machine_id=machine.id,
            user_id=user.id,
            logout_time=None
        ).first()
        
        if existing_session:
            flash(f'User {user.name} is already logged in to this machine.', 'info')
            return redirect(url_for('machine_sessions.view_machine_sessions', machine_id=machine.id))
        
        # Get current number of active users
        current_user_count = machine.current_users.count()
        
        # Check if user can log in with existing users (multi-user support)
        if current_user_count > 0 and not user.can_login_with_others(machine.id, current_user_count):
            flash(f'Machine {machine.name} is already in use and cannot support additional users at this time.', 'danger')
            return redirect(url_for('machine_sessions.view_machine_sessions', machine_id=machine.id))
        
        # If user wants to be lead operator, check if they can be
        if form.is_lead.data:
            if not user.can_be_lead_on_machine(machine.id):
                flash(f'User {user.name} is not authorized to be a lead operator on this machine.', 'danger')
                return redirect(url_for('machine_sessions.login_to_machine', machine_id=machine.id))
            
            # Create a new session with lead status
            session = MachineSession(
                machine_id=machine.id,
                user_id=user.id,
                is_lead=True,
                login_time=datetime.now()
            )
            
            # If there's a current lead operator, demote them
            if machine.lead_operator_id and machine.lead_operator_id != user.id:
                # Find their session and update it
                current_lead_session = MachineSession.query.filter_by(
                    machine_id=machine.id,
                    user_id=machine.lead_operator_id,
                    logout_time=None,
                    is_lead=True
                ).first()
                
                if current_lead_session:
                    current_lead_session.is_lead = False
                
                # Close their lead history
                lead_history = LeadOperatorHistory.query.filter_by(
                    machine_id=machine.id,
                    user_id=machine.lead_operator_id,
                    removed_time=None
                ).order_by(LeadOperatorHistory.assigned_time.desc()).first()
                
                if lead_history:
                    lead_history.removed_time = datetime.now()
                    lead_history.removal_reason = 'reassigned'
            
            # Update machine's lead operator
            machine.lead_operator_id = user.id
            
            # Log the lead operator assignment
            lead_history = LeadOperatorHistory(
                machine_id=machine.id,
                user_id=user.id,
                assigned_time=datetime.now(),
                assigned_by_id=current_user.id if hasattr(current_user, 'id') else None
            )
            
            db.session.add(lead_history)
            
        else:
            # Create regular session
            session = MachineSession(
                machine_id=machine.id,
                user_id=user.id,
                is_lead=False,
                login_time=datetime.now()
            )
        
        db.session.add(session)
        db.session.commit()
        
        flash(f'{user.name} successfully logged in to {machine.name}.', 'success')
        return redirect(url_for('machine_sessions.view_machine_sessions', machine_id=machine.id))
    
    return render_template(
        'machines/machine_login.html',
        machine=machine,
        form=form
    )

@machine_sessions_bp.route('/machines/<int:machine_id>/logout/<int:session_id>', methods=['POST'])
@login_required
def logout_from_machine(machine_id, session_id):
    """Log a user out from a machine session"""
    machine = Machine.query.get_or_404(machine_id)
    session = MachineSession.query.get_or_404(session_id)
    
    # Make sure the session belongs to this machine
    if session.machine_id != machine.id:
        flash('Invalid session for this machine.', 'danger')
        return redirect(url_for('machine_sessions.view_machine_sessions', machine_id=machine.id))
    
    # Check if user has permission
    is_admin = current_user.role and current_user.role.name == 'admin'
    can_manage = is_admin or (machine.zone and machine.zone.area and 
                            current_user.has_area_permission(machine.zone.area.id, 'manage'))
    
    # Users can log themselves out
    if not can_manage and not (hasattr(current_user, 'id') and current_user.id == session.user_id):
        flash('You do not have permission to log out this user.', 'danger')
        return redirect(url_for('machine_sessions.view_machine_sessions', machine_id=machine.id))
    
    # Handle quality reporting form if submitted
    if 'rework_qty' in request.form and 'scrap_qty' in request.form:
        try:
            session.rework_qty = int(request.form.get('rework_qty', 0))
            session.scrap_qty = int(request.form.get('scrap_qty', 0))
        except ValueError:
            flash('Invalid quantity values provided.', 'danger')
            return redirect(url_for('machine_sessions.view_machine_sessions', machine_id=machine.id))
    
    # Complete the session
    session.logout_time = datetime.now()
    
    # If this was a lead operator, update the machine's lead_operator_id
    if session.is_lead and machine.lead_operator_id == session.user_id:
        # Find the lead operator history entry and complete it
        lead_history = LeadOperatorHistory.query.filter_by(
            machine_id=machine.id,
            user_id=session.user_id,
            removed_time=None
        ).order_by(LeadOperatorHistory.assigned_time.desc()).first()
        
        if lead_history:
            lead_history.removed_time = datetime.now()
            lead_history.removal_reason = 'logout'
        
        # Assign a new lead operator if there are other active operators
        new_lead = MachineSession.query.filter(
            MachineSession.machine_id == machine.id,
            MachineSession.user_id != session.user_id,
            MachineSession.logout_time == None
        ).join(RFIDUser).filter(
            RFIDUser.can_be_lead == True
        ).join(MachineAuthorization).filter(
            MachineAuthorization.machine_id == machine.id,
            MachineAuthorization.user_id == MachineSession.user_id,
            MachineAuthorization.can_be_lead == True
        ).first()
        
        if new_lead:
            machine.lead_operator_id = new_lead.user_id
            new_lead.is_lead = True
            
            # Create lead history entry for new lead
            new_lead_history = LeadOperatorHistory(
                machine_id=machine.id,
                user_id=new_lead.user_id,
                assigned_time=datetime.now(),
                assigned_by_id=current_user.id if hasattr(current_user, 'id') else None,
                removal_reason='auto_assigned'
            )
            db.session.add(new_lead_history)
            
            flash(f'{new_lead.user.name} has been automatically assigned as the new lead operator.', 'info')
        else:
            machine.lead_operator_id = None
    
    db.session.commit()
    
    # Create machine log entry for the session
    from models import MachineLog
    
    log = MachineLog(
        machine_id=machine.id,
        user_id=session.user_id,
        log_time=datetime.now(),
        event='logout',
        was_lead=session.is_lead,
        rework_qty=session.rework_qty,
        scrap_qty=session.scrap_qty
    )
    db.session.add(log)
    db.session.commit()
    
    flash(f'Successfully logged out from {machine.name}.', 'success')
    return redirect(url_for('machine_sessions.view_machine_sessions', machine_id=machine.id))

@machine_sessions_bp.route('/machines/<int:machine_id>/sessions')
@login_required
def view_machine_sessions(machine_id):
    """View active sessions for a machine"""
    machine = Machine.query.get_or_404(machine_id)
    
    # Check if user has permission to view this machine's area/zone
    is_admin = current_user.role and current_user.role.name == 'admin'
    if not is_admin and machine.zone and machine.zone.area:
        has_permission = current_user.has_area_permission(machine.zone.area.id)
        if not has_permission:
            flash('You do not have permission to access this machine.', 'danger')
            return redirect(url_for('machines.list_machines'))
    
    # Get active sessions
    active_sessions = MachineSession.query.filter_by(
        machine_id=machine.id,
        logout_time=None
    ).all()
    
    # Get recent sessions (ended in the last 24 hours)
    from datetime import timedelta
    recent_cutoff = datetime.now() - timedelta(days=1)
    
    recent_sessions = MachineSession.query.filter(
        MachineSession.machine_id == machine.id,
        MachineSession.logout_time != None,
        MachineSession.logout_time > recent_cutoff
    ).order_by(MachineSession.logout_time.desc()).all()
    
    # Check if current user is logged in to this machine
    current_user_session = None
    if hasattr(current_user, 'id'):
        current_user_session = MachineSession.query.filter_by(
            machine_id=machine.id,
            user_id=current_user.id,
            logout_time=None
        ).first()
    
    # Get quality report form for logout
    quality_form = QualityReportForm()
    
    return render_template(
        'machines/machine_sessions.html',
        machine=machine,
        active_sessions=active_sessions,
        recent_sessions=recent_sessions,
        current_user_session=current_user_session,
        quality_form=quality_form,
        is_admin=is_admin
    )

@machine_sessions_bp.route('/machines/<int:machine_id>/lead', methods=['GET', 'POST'])
@login_required
def assign_lead_operator(machine_id):
    """Assign a lead operator to a machine"""
    machine = Machine.query.get_or_404(machine_id)
    
    # Check if user has permission to manage this machine
    is_admin = current_user.role and current_user.role.name == 'admin'
    can_manage = is_admin or (machine.zone and machine.zone.area and 
                            current_user.has_area_permission(machine.zone.area.id, 'manage'))
    
    if not can_manage:
        flash('You do not have permission to assign lead operators.', 'danger')
        return redirect(url_for('machine_sessions.view_machine_sessions', machine_id=machine.id))
    
    form = LeadOperatorForm(machine_id=machine.id)
    
    if form.validate_on_submit():
        # Check if selected user is valid
        if form.user_id.data == -1:
            flash('No eligible lead operators available.', 'danger')
            return redirect(url_for('machine_sessions.view_machine_sessions', machine_id=machine.id))
        
        user = RFIDUser.query.get(form.user_id.data)
        if not user:
            flash('Invalid user selected.', 'danger')
            return redirect(url_for('machine_sessions.assign_lead_operator', machine_id=machine.id))
        
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
                login_time=datetime.now()
            )
            db.session.add(session)
        else:
            # Update existing session to make user a lead
            session.is_lead = True
        
        # If there's a current lead operator, demote them
        current_lead_session = None
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
                    lead_history.removed_time = datetime.now()
                    lead_history.removal_reason = 'reassigned'
        
        # Update machine's lead operator
        machine.lead_operator_id = user.id
        
        # Create lead history entry
        lead_history = LeadOperatorHistory(
            machine_id=machine.id,
            user_id=user.id,
            assigned_time=datetime.now(),
            assigned_by_id=current_user.id
        )
        db.session.add(lead_history)
        
        db.session.commit()
        
        flash(f'{user.name} has been assigned as lead operator for {machine.name}.', 'success')
        return redirect(url_for('machine_sessions.view_machine_sessions', machine_id=machine.id))
    
    return render_template(
        'machines/assign_lead.html',
        machine=machine,
        form=form
    )

@machine_sessions_bp.route('/api/machines/<int:machine_id>/sessions')
@login_required
def api_machine_sessions(machine_id):
    """API endpoint to get active sessions for a machine"""
    machine = Machine.query.get_or_404(machine_id)
    
    # Check permission
    is_admin = current_user.role and current_user.role.name == 'admin'
    if not is_admin and machine.zone and machine.zone.area:
        has_permission = current_user.has_area_permission(machine.zone.area.id)
        if not has_permission:
            return jsonify({'error': 'Permission denied'}), 403
    
    active_sessions = MachineSession.query.filter_by(
        machine_id=machine.id,
        logout_time=None
    ).all()
    
    return jsonify({
        'machine': {
            'id': machine.id,
            'name': machine.name,
            'machine_id': machine.machine_id,
            'hierarchical_id': machine.hierarchical_id,
            'lead_operator_id': machine.lead_operator_id,
            'lead_operator_name': machine.lead_operator.name if machine.lead_operator else None
        },
        'sessions': [{
            'id': session.id,
            'user_id': session.user_id,
            'user_name': session.user.name,
            'login_time': session.login_time.isoformat(),
            'is_lead': session.is_lead
        } for session in active_sessions]
    })

@machine_sessions_bp.route('/api/machines/<int:machine_id>/rfid_login', methods=['POST'])
def api_rfid_login(machine_id):
    """API endpoint for RFID-based machine login"""
    # This endpoint doesn't require login as it will be called by the hardware
    machine = Machine.query.get_or_404(machine_id)
    
    data = request.json
    if not data or 'rfid_tag' not in data:
        return jsonify({'error': 'Missing RFID tag'}), 400
    
    rfid_tag = data['rfid_tag']
    user = RFIDUser.query.filter_by(rfid_tag=rfid_tag).first()
    
    if not user:
        return jsonify({
            'status': 'error',
            'error': 'User not found',
            'message': 'RFID card not recognized'
        }), 404
    
    # Check if user is authorized to use this machine
    auth = MachineAuthorization.query.filter_by(
        machine_id=machine.id,
        user_id=user.id
    ).first()
    
    if not auth:
        return jsonify({
            'status': 'error',
            'error': 'User not authorized',
            'message': f'{user.name} is not authorized to use this machine'
        }), 403
    
    # Check if user is already logged in
    existing_session = MachineSession.query.filter_by(
        machine_id=machine.id,
        user_id=user.id,
        logout_time=None
    ).first()
    
    if existing_session:
        # User is already logged in, so log them out
        existing_session.logout_time = datetime.now()
        
        # If this was a lead operator, handle lead transition
        if existing_session.is_lead and machine.lead_operator_id == user.id:
            # Find the lead operator history entry and complete it
            lead_history = LeadOperatorHistory.query.filter_by(
                machine_id=machine.id,
                user_id=user.id,
                removed_time=None
            ).order_by(LeadOperatorHistory.assigned_time.desc()).first()
            
            if lead_history:
                lead_history.removed_time = datetime.now()
                lead_history.removal_reason = 'logout'
            
            # Find a new lead operator if possible
            new_lead = MachineSession.query.filter(
                MachineSession.machine_id == machine.id,
                MachineSession.user_id != user.id,
                MachineSession.logout_time == None
            ).join(RFIDUser).filter(
                RFIDUser.can_be_lead == True
            ).join(MachineAuthorization).filter(
                MachineAuthorization.machine_id == machine.id,
                MachineAuthorization.user_id == MachineSession.user_id,
                MachineAuthorization.can_be_lead == True
            ).first()
            
            if new_lead:
                machine.lead_operator_id = new_lead.user_id
                new_lead.is_lead = True
                
                # Create lead history entry
                new_lead_history = LeadOperatorHistory(
                    machine_id=machine.id,
                    user_id=new_lead.user_id,
                    assigned_time=datetime.now(),
                    assigned_by_id=None,
                    removal_reason='auto_assigned'
                )
                db.session.add(new_lead_history)
            else:
                machine.lead_operator_id = None
        
        db.session.commit()
        
        # Create machine log entry
        from models import MachineLog
        
        log = MachineLog(
            machine_id=machine.id,
            user_id=user.id,
            login_time=datetime.now(),
            logout_time=datetime.now(),
            total_time=0,
            status='completed',
            was_lead=existing_session.is_lead
        )
        db.session.add(log)
        db.session.commit()
        
        can_start = machine.lead_operator_id is not None
        
        return jsonify({
            'status': 'success',
            'action': 'logout',
            'user': {
                'id': user.id,
                'name': user.name
            },
            'machine_status': {
                'has_lead': machine.lead_operator_id is not None,
                'can_start': can_start,
                'lead_operator': machine.lead_operator.name if machine.lead_operator_id else None
            },
            'warning': None if can_start else 'Machine cannot start without a lead operator'
        })
    else:
        # User is not logged in, so log them in
        # Get current number of active users
        current_user_count = machine.current_users.count()
        
        # Check if user can log in with existing users
        if current_user_count > 0 and not user.can_login_with_others(machine.id, current_user_count):
            return jsonify({
                'status': 'error',
                'error': 'Multi-user limit exceeded',
                'message': f'Maximum number of concurrent users ({auth.max_concurrent_users}) reached for this machine'
            }), 403
        
        # Determine if they should be a lead operator
        should_be_lead = False
        
        # If they can be a lead and there's no current lead, make them the lead
        if user.can_be_lead_on_machine(machine.id) and not machine.lead_operator_id:
            should_be_lead = True
        
        # Create a new session
        session = MachineSession(
            machine_id=machine.id,
            user_id=user.id,
            is_lead=should_be_lead,
            login_time=datetime.now()
        )
        db.session.add(session)
        
        # If they're becoming a lead, update the machine and create history
        if should_be_lead:
            machine.lead_operator_id = user.id
            
            lead_history = LeadOperatorHistory(
                machine_id=machine.id,
                user_id=user.id,
                assigned_time=datetime.now(),
                assigned_by_id=None
            )
            db.session.add(lead_history)
        
        db.session.commit()
        
        # Create machine log entry
        from models import MachineLog
        
        log = MachineLog(
            machine_id=machine.id,
            user_id=user.id,
            login_time=datetime.now(),
            status='active',
            was_lead=should_be_lead
        )
        db.session.add(log)
        db.session.commit()
        
        # Check if machine can start (requires lead operator)
        can_start = machine.lead_operator_id is not None
        
        return jsonify({
            'status': 'success',
            'action': 'login',
            'user': {
                'id': user.id,
                'name': user.name
            },
            'is_lead': should_be_lead,
            'machine_status': {
                'has_lead': machine.lead_operator_id is not None,
                'can_start': can_start,
                'lead_operator': machine.lead_operator.name if machine.lead_operator_id else None,
                'active_users': current_user_count + 1
            },
            'warning': None if can_start else 'Machine cannot start without a lead operator'
        })

@machine_sessions_bp.route('/api/machines/<int:machine_id>/report_quality', methods=['POST'])
def api_report_quality(machine_id):
    """API endpoint for reporting quality metrics"""
    # This can be called by hardware or web interface
    machine = Machine.query.get_or_404(machine_id)
    
    data = request.json
    if not data or 'rfid_tag' not in data or 'rework_qty' not in data or 'scrap_qty' not in data:
        return jsonify({'error': 'Missing required fields'}), 400
    
    rfid_tag = data['rfid_tag']
    try:
        rework_qty = int(data['rework_qty'])
        scrap_qty = int(data['scrap_qty'])
    except ValueError:
        return jsonify({'error': 'Invalid quantity values'}), 400
    
    user = RFIDUser.query.filter_by(rfid_tag=rfid_tag).first()
    if not user:
        return jsonify({'error': 'User not found'}), 404
    
    # Check if user has an active session
    session = MachineSession.query.filter_by(
        machine_id=machine.id,
        user_id=user.id,
        logout_time=None
    ).first()
    
    if not session:
        return jsonify({'error': 'User does not have an active session'}), 400
    
    # Update the session with quality data
    session.rework_qty = (session.rework_qty or 0) + rework_qty
    session.scrap_qty = (session.scrap_qty or 0) + scrap_qty
    
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'machine_id': machine.id,
        'user_id': user.id,
        'rework_qty': session.rework_qty,
        'scrap_qty': session.scrap_qty
    })

@machine_sessions_bp.route('/api/machines/<int:machine_id>/can_start')
def api_machine_can_start(machine_id):
    """API endpoint to check if a machine can be started (requires lead operator)"""
    machine = Machine.query.get_or_404(machine_id)
    
    # Check if machine has a lead operator
    has_lead = machine.lead_operator_id is not None
    
    # Get number of active sessions
    active_session_count = MachineSession.query.filter_by(
        machine_id=machine.id,
        logout_time=None
    ).count()
    
    # Build response with detailed status
    response = {
        'can_start': has_lead,
        'has_lead_operator': has_lead,
        'active_sessions': active_session_count,
        'error': None
    }
    
    if not has_lead:
        response['error'] = 'Machine cannot start without a lead operator'
        
        # Include eligible users who could become lead operators
        eligible_leads = db.session.query(RFIDUser).join(
            MachineAuthorization,
            MachineAuthorization.user_id == RFIDUser.id
        ).filter(
            MachineAuthorization.machine_id == machine.id,
            MachineAuthorization.can_be_lead == True,
            RFIDUser.can_be_lead == True,
            RFIDUser.active == True
        ).all()
        
        response['eligible_leads'] = [{
            'id': user.id,
            'name': user.name,
            'rfid_tag': user.rfid_tag
        } for user in eligible_leads]
    
    return jsonify(response)

@machine_sessions_bp.route('/api/machines/<int:machine_id>/lead_transfer', methods=['POST'])
def api_transfer_lead(machine_id):
    """API endpoint for transferring lead operator status"""
    machine = Machine.query.get_or_404(machine_id)
    
    data = request.json
    if not data or 'current_lead_rfid' not in data or 'new_lead_rfid' not in data:
        return jsonify({'error': 'Missing required RFID tags'}), 400
    
    current_lead_rfid = data['current_lead_rfid']
    new_lead_rfid = data['new_lead_rfid']
    
    # Verify current lead
    current_lead = RFIDUser.query.filter_by(rfid_tag=current_lead_rfid).first()
    if not current_lead or current_lead.id != machine.lead_operator_id:
        return jsonify({'error': 'Invalid current lead operator'}), 403
    
    # Verify new lead
    new_lead = RFIDUser.query.filter_by(rfid_tag=new_lead_rfid).first()
    if not new_lead:
        return jsonify({'error': 'New lead operator not found'}), 404
    
    # Check if new lead is authorized
    auth = MachineAuthorization.query.filter_by(
        machine_id=machine.id,
        user_id=new_lead.id,
        can_be_lead=True
    ).first()
    
    if not auth or not new_lead.can_be_lead:
        return jsonify({'error': 'User not authorized to be lead operator'}), 403
    
    # Check if new lead has an active session
    new_lead_session = MachineSession.query.filter_by(
        machine_id=machine.id,
        user_id=new_lead.id,
        logout_time=None
    ).first()
    
    if not new_lead_session:
        # Create session for new lead
        new_lead_session = MachineSession(
            machine_id=machine.id,
            user_id=new_lead.id,
            is_lead=True,
            login_time=datetime.now()
        )
        db.session.add(new_lead_session)
    else:
        # Update existing session
        new_lead_session.is_lead = True
    
    # Find current lead's session and update it
    current_lead_session = MachineSession.query.filter_by(
        machine_id=machine.id,
        user_id=current_lead.id,
        logout_time=None
    ).first()
    
    if current_lead_session:
        current_lead_session.is_lead = False
    
    # Close current lead history entry
    lead_history = LeadOperatorHistory.query.filter_by(
        machine_id=machine.id,
        user_id=current_lead.id,
        removed_time=None
    ).order_by(LeadOperatorHistory.assigned_time.desc()).first()
    
    if lead_history:
        lead_history.removed_time = datetime.now()
        lead_history.removal_reason = 'transfer'
    
    # Create new lead history entry
    new_lead_history = LeadOperatorHistory(
        machine_id=machine.id,
        user_id=new_lead.id,
        assigned_time=datetime.now(),
        assigned_by_id=current_lead.id,
        removal_reason='transfer'
    )
    db.session.add(new_lead_history)
    
    # Update machine lead operator
    machine.lead_operator_id = new_lead.id
    
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'message': f'Lead operator transferred from {current_lead.name} to {new_lead.name}',
        'new_lead': {
            'id': new_lead.id,
            'name': new_lead.name
        }
    })

@machine_sessions_bp.route('/api/machines/<int:machine_id>/lead_status')
def api_lead_status(machine_id):
    """API endpoint to get lead operator status for a machine"""
    machine = Machine.query.get_or_404(machine_id)
    
    has_lead = machine.lead_operator_id is not None
    lead_operator = None
    
    if has_lead:
        lead = machine.lead_operator
        lead_operator = {
            'id': lead.id,
            'name': lead.name,
            'rfid_tag': lead.rfid_tag
        }
        
        # Get lead session duration
        lead_session = MachineSession.query.filter_by(
            machine_id=machine.id,
            user_id=lead.id,
            logout_time=None,
            is_lead=True
        ).first()
        
        if lead_session:
            duration = (datetime.now() - lead_session.login_time).total_seconds() / 60
            lead_operator['session_duration_minutes'] = round(duration, 1)
    
    # Get active operators count
    active_operators = MachineSession.query.filter_by(
        machine_id=machine.id,
        logout_time=None
    ).count()
    
    # Get lead history
    recent_lead_changes = LeadOperatorHistory.query.filter_by(
        machine_id=machine.id
    ).order_by(LeadOperatorHistory.assigned_time.desc()).limit(5).all()
    
    lead_history = []
    for history in recent_lead_changes:
        entry = {
            'user_id': history.user_id,
            'user_name': history.user.name,
            'assigned_time': history.assigned_time.isoformat(),
            'removed_time': history.removed_time.isoformat() if history.removed_time else None,
            'assigned_by': history.assigned_by.name if history.assigned_by else 'System',
            'reason': history.removal_reason
        }
        lead_history.append(entry)
    
    return jsonify({
        'has_lead': has_lead,
        'machine_id': machine.id,
        'machine_name': machine.name,
        'lead_operator': lead_operator,
        'active_operators': active_operators,
        'recent_lead_changes': lead_history
    })