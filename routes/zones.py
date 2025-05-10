from flask import Blueprint, render_template, request, redirect, url_for, flash, jsonify
from flask_login import login_required, current_user
from app import db
from models import Zone, Area, Machine, Node, User
from forms import ZoneForm

# Create a blueprint for zone/location management routes
zones_bp = Blueprint('zones', __name__)

@zones_bp.route('/zones')
@login_required
def list_zones():
    """List all zones/locations the user has access to"""
    # Check if user is admin
    is_admin = current_user.role and current_user.role.name == 'admin'
    
    if is_admin:
        # Admin sees all zones
        zones = Zone.query.all()
    else:
        # Regular users see only zones in areas they have permission for
        from models import AreaPermission
        
        # Get areas user has permission for
        permissions = AreaPermission.query.filter_by(user_id=current_user.id).all()
        area_ids = [p.area_id for p in permissions]
        
        # Get zones in those areas
        zones = Zone.query.filter(Zone.area_id.in_(area_ids)).all()
    
    # Group zones by area for display
    areas = {}
    for zone in zones:
        if zone.area:
            if zone.area.id not in areas:
                areas[zone.area.id] = {
                    'name': zone.area.name,
                    'code': zone.area.code,
                    'zones': []
                }
            areas[zone.area.id]['zones'].append(zone)
    
    return render_template('zones/zone_list.html', areas=areas, is_admin=is_admin)

@zones_bp.route('/areas/<int:area_id>/zones/new', methods=['GET', 'POST'])
@login_required
def new_zone(area_id):
    """Create a new zone/location in an area"""
    area = Area.query.get_or_404(area_id)
    
    # Check if user has permission for this area
    is_admin = current_user.role and current_user.role.name == 'admin'
    has_permission = is_admin or current_user.has_area_permission(area_id)
    
    if not has_permission:
        flash('You do not have permission to add locations to this area.', 'danger')
        return redirect(url_for('areas.view_area', area_id=area_id))
    
    form = ZoneForm()
    
    # Set the area_id to the current area and limit choices
    form.area_id.data = area_id
    form.area_id.choices = [(area.id, f"{area.name} ({area.code})")]
    
    if form.validate_on_submit():
        zone = Zone(
            name=form.name.data,
            description=form.description.data,
            code=form.code.data,
            area_id=form.area_id.data,
            active=True
        )
        db.session.add(zone)
        db.session.commit()
        
        flash(f'Location "{zone.name}" created successfully!', 'success')
        return redirect(url_for('areas.view_area', area_id=area_id))
    
    return render_template('zones/zone_form.html', form=form, zone=None, area=area)

@zones_bp.route('/zones/new', methods=['GET', 'POST'])
@login_required
def new_zone_global():
    """Create a new zone/location (global view)"""
    # Check if user is admin
    is_admin = current_user.role and current_user.role.name == 'admin'
    
    if not is_admin:
        flash('You do not have permission to create locations.', 'danger')
        return redirect(url_for('zones.list_zones'))
    
    form = ZoneForm()
    
    if form.validate_on_submit():
        zone = Zone(
            name=form.name.data,
            description=form.description.data,
            code=form.code.data,
            area_id=form.area_id.data,
            active=True
        )
        db.session.add(zone)
        db.session.commit()
        
        flash(f'Location "{zone.name}" created successfully!', 'success')
        return redirect(url_for('zones.list_zones'))
    
    return render_template('zones/zone_form.html', form=form, zone=None, area=None)

@zones_bp.route('/zones/<int:zone_id>/edit', methods=['GET', 'POST'])
@login_required
def edit_zone(zone_id):
    """Edit an existing zone/location"""
    zone = Zone.query.get_or_404(zone_id)
    
    # Check if user has permission for this area
    is_admin = current_user.role and current_user.role.name == 'admin'
    has_permission = is_admin or (zone.area and current_user.has_area_permission(zone.area_id))
    
    if not has_permission:
        flash('You do not have permission to edit this location.', 'danger')
        return redirect(url_for('zones.list_zones'))
    
    form = ZoneForm(obj=zone)
    
    if form.validate_on_submit():
        zone.name = form.name.data
        zone.description = form.description.data
        zone.code = form.code.data
        zone.area_id = form.area_id.data
        zone.active = form.active.data
        
        db.session.commit()
        flash(f'Location "{zone.name}" updated successfully!', 'success')
        
        if zone.area:
            return redirect(url_for('areas.view_area', area_id=zone.area_id))
        else:
            return redirect(url_for('zones.list_zones'))
    
    return render_template('zones/zone_form.html', form=form, zone=zone, area=zone.area)

@zones_bp.route('/zones/<int:zone_id>')
@login_required
def view_zone(zone_id):
    """View zone/location details and contained machines"""
    zone = Zone.query.get_or_404(zone_id)
    
    # Check if user has permission for this area
    is_admin = current_user.role and current_user.role.name == 'admin'
    has_permission = is_admin or (zone.area and current_user.has_area_permission(zone.area_id))
    
    if not has_permission:
        flash('You do not have permission to view this location.', 'danger')
        return redirect(url_for('zones.list_zones'))
    
    machines = Machine.query.filter_by(zone_id=zone_id).all()
    nodes = Node.query.filter_by(zone_id=zone_id).all()
    
    return render_template(
        'zones/zone_detail.html', 
        zone=zone, 
        machines=machines,
        nodes=nodes,
        is_admin=is_admin
    )

@zones_bp.route('/api/zones')
@login_required
def api_list_zones():
    """API endpoint to list zones/locations (for dropdown menus, etc.)"""
    # Check if area_id filter is provided
    area_id = request.args.get('area_id', type=int)
    
    # Check if user is admin
    is_admin = current_user.role and current_user.role.name == 'admin'
    
    if area_id:
        # Check if user has permission for this area
        has_permission = is_admin or current_user.has_area_permission(area_id)
        
        if not has_permission:
            return jsonify({'error': 'Permission denied'}), 403
            
        zones = Zone.query.filter_by(area_id=area_id, active=True).all()
    elif is_admin:
        # Admin sees all zones
        zones = Zone.query.filter_by(active=True).all()
    else:
        # Regular users see only zones in areas they have permission for
        from models import AreaPermission
        
        # Get areas user has permission for
        permissions = AreaPermission.query.filter_by(user_id=current_user.id).all()
        area_ids = [p.area_id for p in permissions]
        
        # Get zones in those areas
        zones = Zone.query.filter(Zone.area_id.in_(area_ids), Zone.active == True).all()
    
    return jsonify([{
        'id': zone.id,
        'name': zone.name,
        'code': zone.code,
        'full_code': zone.full_code,
        'area_id': zone.area_id,
        'area_name': zone.area.name if zone.area else 'Unassigned',
        'machine_count': zone.machines.count()
    } for zone in zones])

@zones_bp.route('/api/zones/<int:zone_id>/machines')
@login_required
def api_list_zone_machines(zone_id):
    """API endpoint to list machines in a zone/location (for dropdown menus, etc.)"""
    zone = Zone.query.get_or_404(zone_id)
    
    # Check if user has permission for this area
    is_admin = current_user.role and current_user.role.name == 'admin'
    has_permission = is_admin or (zone.area and current_user.has_area_permission(zone.area_id))
    
    if not has_permission:
        return jsonify({'error': 'Permission denied'}), 403
    
    machines = Machine.query.filter_by(zone_id=zone_id).all()
    
    return jsonify([{
        'id': machine.id,
        'name': machine.name,
        'machine_id': machine.machine_id,
        'hierarchical_id': machine.hierarchical_id,
        'node_id': machine.node_id,
        'has_lead': machine.has_lead_operator
    } for machine in machines])