"""
Shop Suite Authentication
------------------------
Cross-application authentication and SSO functionality for Shop Suite
"""

import datetime
import secrets
import base64
import hashlib
import json
import jwt
from flask import current_app, request, redirect, url_for, flash, session, g
from flask_login import current_user, login_user
from functools import wraps

from app import db
from shop_suite.models import SuiteUser, UserSession

# Constants
SESSION_COOKIE_NAME = "shop_suite_session"
APP_NAME_MONITOR = "shop_monitor"
APP_NAME_TRACKER = "shop_tracker"
TOKEN_VALIDITY = 24 * 60 * 60  # 24 hours in seconds


class ShopSuiteAuth:
    """Authentication handler for Shop Suite applications"""
    
    def __init__(self, app=None):
        """Initialize auth module, optionally with app"""
        self.app = app
        self.secret = None
        self.host_app = None
        
        if app is not None:
            self.init_app(app)
            
    def init_app(self, app):
        """Initialize auth with Flask app"""
        self.app = app
        self.secret = app.config.get('SHOP_SUITE_SECRET_KEY', app.config.get('SECRET_KEY'))
        self.host_app = app.config.get('SHOP_SUITE_APP_NAME', APP_NAME_MONITOR)
        
        # Set up cross-app cookie settings
        app.config['SESSION_COOKIE_NAME'] = SESSION_COOKIE_NAME
        app.config['SESSION_COOKIE_DOMAIN'] = app.config.get('SHOP_SUITE_COOKIE_DOMAIN', None)
        app.config['SESSION_COOKIE_SAMESITE'] = 'Lax'
        
        # Register before_request handler
        app.before_request(self._check_sso_token)
        
    def _check_sso_token(self):
        """Check for SSO token in request"""
        # Skip for static files and when user is already authenticated
        if request.path.startswith('/static/') or current_user.is_authenticated:
            return
            
        # Check for token in URL
        token = request.args.get('sso_token')
        if token:
            user = self._authenticate_token(token)
            if user:
                login_user(user)
                # Store the return URL if any
                next_url = request.args.get('next')
                if next_url:
                    session['next_url'] = next_url
                
                # Redirect to remove token from URL (security best practice)
                return redirect(request.path)
    
    def _authenticate_token(self, token):
        """Authenticate a user from an SSO token"""
        try:
            # Verify token
            data = jwt.decode(token, self.secret, algorithms=['HS256'])
            
            # Validate token data
            if 'user_id' not in data or 'exp' not in data:
                return None
                
            # Check expiration
            if data['exp'] < datetime.datetime.utcnow().timestamp():
                return None
                
            # Check session validity
            session = UserSession.query.filter_by(
                session_token=data.get('session_id'),
                invalidated=False
            ).first()
            
            if not session or not session.is_active:
                return None
                
            # Get the user
            user = SuiteUser.query.get(data['user_id'])
            if user and user.active:
                # Update session
                session.update_activity()
                session.current_app = self.host_app
                db.session.commit()
                return user
                
        except (jwt.InvalidTokenError, Exception) as e:
            current_app.logger.error(f"SSO token authentication error: {str(e)}")
            
        return None
        
    def generate_sso_url(self, user, target_app, target_url=None):
        """Generate a URL with SSO token for cross-app navigation
        
        Args:
            user: SuiteUser to authenticate
            target_app: Target application name (shop_monitor or shop_tracker)
            target_url: Specific URL path in target app (optional)
            
        Returns:
            str: Full URL with SSO token
        """
        # Get app URL from config
        if target_app == APP_NAME_MONITOR:
            base_url = current_app.config.get('SHOP_MONITOR_URL', 'http://localhost:5000')
        elif target_app == APP_NAME_TRACKER:
            base_url = current_app.config.get('SHOP_TRACKER_URL', 'http://localhost:5001')
        else:
            raise ValueError(f"Unknown target app: {target_app}")
            
        # Get or create user session
        session_token = user.get_cross_app_token()
        
        # Create JWT token
        token_data = {
            'user_id': user.id,
            'username': user.username,
            'session_id': session_token,
            'source_app': self.host_app,
            'iat': datetime.datetime.utcnow(),
            'exp': datetime.datetime.utcnow() + datetime.timedelta(seconds=TOKEN_VALIDITY)
        }
        
        token = jwt.encode(token_data, self.secret, algorithm='HS256')
        
        # Construct URL
        url = f"{base_url.rstrip('/')}"
        if target_url:
            # Ensure target_url starts with slash
            if not target_url.startswith('/'):
                target_url = f"/{target_url}"
            url += target_url
            
        # Add token to URL
        url += f"{'&' if '?' in url else '?'}sso_token={token}"
        
        return url
        
    def get_current_app(self):
        """Get the current app name"""
        return self.host_app
        

# Flask view decorators
def cross_app_aware(f):
    """Decorator to make views aware of cross-app context"""
    @wraps(f)
    def decorated_function(*args, **kwargs):
        # Store source app info if coming from another app
        source_app = request.args.get('source_app')
        if source_app:
            g.source_app = source_app
            
        # Store any next URL from request args
        next_url = request.args.get('next')
        if next_url:
            session['next_url'] = next_url
            
        return f(*args, **kwargs)
    return decorated_function


def cross_app_login_required(f):
    """Enhanced login_required that handles cross-app redirects"""
    from flask_login import login_required, current_app
    
    @wraps(f)
    @login_required
    def decorated_function(*args, **kwargs):
        # Check if user has active session in current app
        if hasattr(current_user, 'current_app') and current_user.current_app != current_app.config.get('SHOP_SUITE_APP_NAME'):
            # Update user's current app
            current_user.current_app = current_app.config.get('SHOP_SUITE_APP_NAME')
            db.session.commit()
            
        return f(*args, **kwargs)
    return decorated_function