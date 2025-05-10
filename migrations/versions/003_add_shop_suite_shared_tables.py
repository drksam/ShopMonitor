"""Add Shop Suite shared tables for cross-app functionality

Revision ID: 003
Revises: 002
Create Date: 2025-05-08

"""
from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision = '003'
down_revision = '002'
branch_labels = None
depends_on = None


def upgrade() -> None:
    # Create SuiteUser table for cross-app user management
    op.create_table('suite_users',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('username', sa.String(length=64), nullable=False),
        sa.Column('email', sa.String(length=120), nullable=False),
        sa.Column('password_hash', sa.String(length=256), nullable=False),
        sa.Column('display_name', sa.String(length=100), nullable=True),
        sa.Column('active', sa.Boolean(), nullable=True, default=True),
        sa.Column('auth_provider', sa.String(length=50), nullable=True, default='local'),
        sa.Column('external_id', sa.String(length=100), nullable=True),
        sa.Column('is_admin', sa.Boolean(), nullable=True, default=False),
        sa.Column('rfid_tag', sa.String(length=64), nullable=True),
        sa.Column('created_by_app', sa.String(length=50), nullable=True),
        sa.Column('managed_by_app', sa.String(length=50), nullable=True),
        sa.Column('current_app', sa.String(length=50), nullable=True),
        sa.Column('last_login', sa.DateTime(), nullable=True),
        sa.Column('created_at', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.Column('updated_at', sa.DateTime(), nullable=True, onupdate=sa.func.now()),
        sa.PrimaryKeyConstraint('id'),
        sa.UniqueConstraint('username'),
        sa.UniqueConstraint('email')
    )
    
    # Create SuitePermission table for cross-app permissions
    op.create_table('suite_permissions',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('user_id', sa.Integer(), nullable=False),
        sa.Column('resource_type', sa.String(length=50), nullable=False),
        sa.Column('resource_id', sa.Integer(), nullable=False),
        sa.Column('app_context', sa.String(length=50), nullable=False),
        sa.Column('permission_level', sa.String(length=20), nullable=False),
        sa.Column('valid_from', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.Column('valid_until', sa.DateTime(), nullable=True),
        sa.Column('granted_by', sa.Integer(), nullable=True),
        sa.Column('created_at', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.Column('updated_at', sa.DateTime(), nullable=True, onupdate=sa.func.now()),
        sa.ForeignKeyConstraint(['user_id'], ['suite_users.id'], ),
        sa.ForeignKeyConstraint(['granted_by'], ['suite_users.id'], ),
        sa.PrimaryKeyConstraint('id')
    )
    
    # Create UserSession table for tracking sessions across apps
    op.create_table('user_sessions',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('user_id', sa.Integer(), nullable=False),
        sa.Column('session_token', sa.String(length=128), nullable=False),
        sa.Column('ip_address', sa.String(length=45), nullable=True),
        sa.Column('user_agent', sa.String(length=255), nullable=True),
        sa.Column('current_app', sa.String(length=50), nullable=True),
        sa.Column('created_at', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.Column('expires_at', sa.DateTime(), nullable=False),
        sa.Column('last_active', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.Column('invalidated', sa.Boolean(), nullable=True, default=False),
        sa.ForeignKeyConstraint(['user_id'], ['suite_users.id'], ),
        sa.PrimaryKeyConstraint('id'),
        sa.UniqueConstraint('session_token')
    )
    
    # Create SyncEvent table for cross-app synchronization
    op.create_table('sync_events',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('event_type', sa.String(length=50), nullable=False),
        sa.Column('resource_type', sa.String(length=50), nullable=False),
        sa.Column('resource_id', sa.Integer(), nullable=False),
        sa.Column('source_app', sa.String(length=50), nullable=False),
        sa.Column('target_app', sa.String(length=50), nullable=False),
        sa.Column('status', sa.String(length=20), nullable=True, default='pending'),
        sa.Column('payload', sa.Text(), nullable=True),
        sa.Column('attempts', sa.Integer(), nullable=True, default=0),
        sa.Column('last_attempt', sa.DateTime(), nullable=True),
        sa.Column('processed_at', sa.DateTime(), nullable=True),
        sa.Column('error_message', sa.Text(), nullable=True),
        sa.Column('created_at', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.Column('updated_at', sa.DateTime(), nullable=True, onupdate=sa.func.now()),
        sa.PrimaryKeyConstraint('id')
    )
    
    # Create mapping table between existing users and suite users
    op.create_table('user_suite_mapping',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('legacy_user_id', sa.Integer(), nullable=False),
        sa.Column('suite_user_id', sa.Integer(), nullable=False),
        sa.Column('created_at', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.ForeignKeyConstraint(['legacy_user_id'], ['user.id'], ),
        sa.ForeignKeyConstraint(['suite_user_id'], ['suite_users.id'], ),
        sa.PrimaryKeyConstraint('id'),
        sa.UniqueConstraint('legacy_user_id'),
        sa.UniqueConstraint('suite_user_id')
    )
    
    # Create mapping table between RFID users and suite users
    op.create_table('rfid_suite_mapping',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('rfid_user_id', sa.Integer(), nullable=False),
        sa.Column('suite_user_id', sa.Integer(), nullable=False),
        sa.Column('created_at', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.ForeignKeyConstraint(['rfid_user_id'], ['rfid_user.id'], ),
        sa.ForeignKeyConstraint(['suite_user_id'], ['suite_users.id'], ),
        sa.PrimaryKeyConstraint('id'),
        sa.UniqueConstraint('rfid_user_id'),
        sa.UniqueConstraint('suite_user_id')
    )
    
    # Add index for session token lookup
    op.create_index('ix_user_sessions_token', 'user_sessions', ['session_token'], unique=True)
    
    # Add index for user lookup by email
    op.create_index('ix_suite_users_email', 'suite_users', ['email'], unique=True)
    
    # Add index for sync events status
    op.create_index('ix_sync_events_status', 'sync_events', ['status'], unique=False)
    
    # Add index for resource type and id in permissions
    op.create_index('ix_suite_permissions_resource', 
                   'suite_permissions', 
                   ['resource_type', 'resource_id', 'app_context'], 
                   unique=False)
    
    # Modify the Alert model to reference ShopTracker instead of ShopTracker
    op.execute("UPDATE alert SET origin = 'shop_tracker' WHERE origin = 'Shop_tracker'")
    
    # Modify the RFIDUser model to change external reference
    op.execute("COMMENT ON COLUMN rfid_user.external_id IS 'ID in ShopTracker system, for synchronization'")
    op.execute("COMMENT ON COLUMN rfid_user.last_synced IS 'Last time this user was synced with ShopTracker'")


def downgrade() -> None:
    # Drop indexes
    op.drop_index('ix_suite_permissions_resource', table_name='suite_permissions')
    op.drop_index('ix_sync_events_status', table_name='sync_events')
    op.drop_index('ix_suite_users_email', table_name='suite_users')
    op.drop_index('ix_user_sessions_token', table_name='user_sessions')
    
    # Drop mapping tables
    op.drop_table('rfid_suite_mapping')
    op.drop_table('user_suite_mapping')
    
    # Drop new tables
    op.drop_table('sync_events')
    op.drop_table('user_sessions')
    op.drop_table('suite_permissions')
    op.drop_table('suite_users')
    
    # Revert origin field in alerts
    op.execute("UPDATE alert SET origin = 'Shop_tracker' WHERE origin = 'shop_tracker'")