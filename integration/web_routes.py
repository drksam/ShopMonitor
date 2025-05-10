"""
Web routes for ShopMonitor integration
Provides the configuration interface for the integration
"""

import os
from flask import Blueprint, render_template, request, redirect, url_for, flash, current_app
from flask_login import login_required, current_user
from .shop_integration import integration, get_last_sync_time
from models import Alert, RFIDUser, Zone, Machine, MachineAuthorization

web_routes_bp = Blueprint('integration_web', __name__, url_prefix='/integration')

@web_routes_bp.route('/', methods=['GET'])
@login_required
def config_page():
    """Configuration page for ShopMonitor integration"""
    # Check if user is admin
    if not current_user.role or current_user.role.name.lower() != 'admin':
        flash('You do not have permission to access this page.', 'danger')
        return redirect(url_for('dashboard'))
    
    # Get current configuration
    api_url = current_app.config.get('SHOP_API_BASE_URL') or current_app.config.get('Shop_API_BASE_URL', '')
    has_api_key = bool(current_app.config.get('SHOP_API_KEY') or current_app.config.get('Shop_API_KEY', ''))
    sync_interval = current_app.config.get('SYNC_INTERVAL', 3600)
    last_sync = get_last_sync_time()
    
    status = 'configured' if api_url and has_api_key else 'not_configured'
    
    return render_template('integration/configuration.html',
                           status=status,
                           api_url=api_url,
                           has_api_key=has_api_key,
                           sync_interval=sync_interval,
                           last_sync=last_sync)

@web_routes_bp.route('/save', methods=['POST'])
@login_required
def save_config():
    """Save integration configuration"""
    # Check if user is admin
    if not current_user.role or current_user.role.name.lower() != 'admin':
        flash('You do not have permission to access this page.', 'danger')
        return redirect(url_for('dashboard'))
    
    api_url = request.form.get('api_url', '').strip()
    api_key = request.form.get('api_key', '').strip()
    
    try:
        sync_interval = int(request.form.get('sync_interval', 60))
        sync_interval = max(5, min(sync_interval, 1440)) * 60  # Convert to seconds and clamp
    except ValueError:
        sync_interval = 3600  # Default to 1 hour
    
    # Save configuration
    if api_url:
        os.environ['SHOP_API_BASE_URL'] = api_url
        current_app.config['SHOP_API_BASE_URL'] = api_url
    
    if api_key:
        os.environ['SHOP_API_KEY'] = api_key
        current_app.config['SHOP_API_KEY'] = api_key
    
    os.environ['SYNC_INTERVAL'] = str(sync_interval)
    current_app.config['SYNC_INTERVAL'] = sync_interval
    
    # Restart integration with new config if needed
    if api_url and (api_key or current_app.config.get('SHOP_API_KEY') or current_app.config.get('Shop_API_KEY')):
        if integration.app:
            integration.stop_background_sync()
        integration.init_app(current_app)
    
    flash('Integration configuration saved successfully.', 'success')
    return redirect(url_for('integration_web.config_page'))


@web_routes_bp.route('/api-docs', methods=['GET'])
@login_required
def api_docs():
    """API documentation page for ShopMonitor integration"""
    # Check if user is admin
    if not current_user.role or current_user.role.name.lower() != 'admin':
        flash('You do not have permission to access this page.', 'danger')
        return redirect(url_for('dashboard'))
    
    # Get recent alerts
    recent_alerts = Alert.query.order_by(Alert.created_at.desc()).limit(5).all()
    
    # Get API configuration status
    api_url = current_app.config.get('SHOP_API_BASE_URL') or current_app.config.get('Shop_API_BASE_URL', '')
    has_api_key = bool(current_app.config.get('SHOP_API_KEY') or current_app.config.get('Shop_API_KEY', ''))
    status = 'configured' if api_url and has_api_key else 'not_configured'
    
    return render_template('integration/api_docs.html',
                           status=status,
                           api_url=api_url,
                           has_api_key=has_api_key,
                           recent_alerts=recent_alerts)

@web_routes_bp.route('/available-users', methods=['GET'])
@login_required
def available_users():
    """Display available users from ShopMonitor"""
    # Check if user is admin
    if not current_user.role or current_user.role.name.lower() != 'admin':
        flash('You do not have permission to access this page.', 'danger')
        return redirect(url_for('dashboard'))
    
    # Check if integration is configured
    api_url = current_app.config.get('SHOP_API_BASE_URL') or current_app.config.get('Shop_API_BASE_URL', '')
    has_api_key = bool(current_app.config.get('SHOP_API_KEY') or current_app.config.get('Shop_API_KEY', ''))
    
    if not api_url or not has_api_key:
        flash('ShopMonitor integration is not configured. Please configure it first.', 'warning')
        return redirect(url_for('integration_web.config_page'))
    
    # Render the available users page
    return render_template('integration/available_users.html')

@web_routes_bp.route('/user-permissions/<int:user_id>', methods=['GET', 'POST'])
@login_required
def manage_user_permissions(user_id):
    """Manage user permissions with ShopMonitor integration"""
    # Check if user is admin
    if not current_user.role or current_user.role.name.lower() != 'admin':
        flash('You do not have permission to access this page.', 'danger')
        return redirect(url_for('dashboard'))
    
    # Check if integration is configured
    api_url = current_app.config.get('SHOP_API_BASE_URL') or current_app.config.get('Shop_API_BASE_URL', '')
    has_api_key = bool(current_app.config.get('SHOP_API_KEY') or current_app.config.get('Shop_API_KEY', ''))
    
    if not api_url or not has_api_key:
        flash('ShopMonitor integration is not configured. Please configure it first.', 'warning')
        return redirect(url_for('integration_web.config_page'))
    
    # Get user
    user = RFIDUser.query.get_or_404(user_id)
    
    # Get all machines by zone
    zones = Zone.query.all()
    
    # Get user's authorized machines
    authorizations = MachineAuthorization.query.filter_by(user_id=user.id).all()
    authorized_machine_ids = [auth.machine_id for auth in authorizations]
    
    # Render the permissions management page
    return render_template('integration/manage_permissions.html',
                           user=user,
                           zones=zones,
                           authorized_machine_ids=authorized_machine_ids,
                           permissions=authorizations)