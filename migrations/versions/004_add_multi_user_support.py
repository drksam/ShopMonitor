"""Add multi-user support and lead operator enhancements

Revision ID: 004
Revises: 003
Create Date: 2025-05-10

"""
from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision = '004'
down_revision = '003'
branch_labels = None
depends_on = None


def upgrade() -> None:
    # Add multi-user fields to MachineAuthorization
    op.add_column('machine_authorization',
        sa.Column('multi_user_allowed', sa.Boolean(), nullable=True, server_default='1')
    )
    op.add_column('machine_authorization',
        sa.Column('max_concurrent_users', sa.Integer(), nullable=True, server_default='3')
    )
    
    # Add relationship to machine in MachineAuthorization
    # (this is handled by SQLAlchemy at runtime, no actual column needed)
    
    # Create an index on MachineAuthorization for faster lookups
    op.create_index('ix_machine_authorization_machine_user', 
                   'machine_authorization', 
                   ['machine_id', 'user_id'], 
                   unique=False)
    
    # Create index on MachineSession for active sessions lookup
    op.create_index('ix_machine_session_active', 
                   'machine_session', 
                   ['machine_id', 'logout_time', 'is_lead'], 
                   unique=False)


def downgrade() -> None:
    # Drop the indexes
    op.drop_index('ix_machine_session_active', table_name='machine_session')
    op.drop_index('ix_machine_authorization_machine_user', table_name='machine_authorization')
    
    # Drop the new columns
    op.drop_column('machine_authorization', 'max_concurrent_users')
    op.drop_column('machine_authorization', 'multi_user_allowed')