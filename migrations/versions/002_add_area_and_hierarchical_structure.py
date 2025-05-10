"""Add Area model and hierarchical structure

Revision ID: 002
Revises: 001
Create Date: 2025-05-08

"""
from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision = '002'
down_revision = '001'
branch_labels = None
depends_on = None


def upgrade() -> None:
    # Create Area table
    op.create_table('area',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('name', sa.String(length=64), nullable=False),
        sa.Column('description', sa.String(length=255), nullable=True),
        sa.Column('code', sa.String(length=10), nullable=False),
        sa.Column('active', sa.Boolean(), nullable=True, default=True),
        sa.Column('created_at', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.PrimaryKeyConstraint('id'),
        sa.UniqueConstraint('code')
    )
    
    # Create AreaPermission table
    op.create_table('area_permission',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('user_id', sa.Integer(), nullable=False),
        sa.Column('area_id', sa.Integer(), nullable=False),
        sa.Column('permission_level', sa.String(length=20), nullable=True, default='view'),
        sa.Column('created_at', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.ForeignKeyConstraint(['user_id'], ['user.id'], ),
        sa.ForeignKeyConstraint(['area_id'], ['area.id'], ),
        sa.PrimaryKeyConstraint('id'),
        sa.UniqueConstraint('user_id', 'area_id')
    )
    
    # Add area_id column to Zone table
    op.add_column('zone', sa.Column('area_id', sa.Integer(), nullable=True))
    op.add_column('zone', sa.Column('code', sa.String(length=10), nullable=True))
    op.add_column('zone', sa.Column('active', sa.Boolean(), nullable=True, default=True))
    op.add_column('zone', sa.Column('created_at', sa.DateTime(), nullable=True, default=sa.func.now()))
    op.create_foreign_key('fk_zone_area', 'zone', 'area', ['area_id'], ['id'])
    
    # Update Node table to include area and zone relationships
    op.add_column('node', sa.Column('area_id', sa.Integer(), nullable=True))
    op.add_column('node', sa.Column('zone_id', sa.Integer(), nullable=True))
    op.add_column('node', sa.Column('has_display', sa.Boolean(), nullable=True, default=False))
    op.create_foreign_key('fk_node_area', 'node', 'area', ['area_id'], ['id'])
    op.create_foreign_key('fk_node_zone', 'node', 'zone', ['zone_id'], ['id'])
    
    # Update Machine table to support lead operators and multi-user
    op.add_column('machine', sa.Column('lead_operator_id', sa.Integer(), nullable=True))
    op.create_foreign_key('fk_machine_lead_operator', 'machine', 'rfid_user', ['lead_operator_id'], ['id'])
    
    # Create MachineSession table for multi-user support
    op.create_table('machine_session',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('machine_id', sa.Integer(), nullable=False),
        sa.Column('user_id', sa.Integer(), nullable=False),
        sa.Column('is_lead', sa.Boolean(), nullable=True, default=False),
        sa.Column('login_time', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.Column('logout_time', sa.DateTime(), nullable=True),
        sa.Column('rework_qty', sa.Integer(), nullable=True, default=0),
        sa.Column('scrap_qty', sa.Integer(), nullable=True, default=0),
        sa.ForeignKeyConstraint(['machine_id'], ['machine.id'], ),
        sa.ForeignKeyConstraint(['user_id'], ['rfid_user.id'], ),
        sa.PrimaryKeyConstraint('id')
    )
    
    # Create LeadOperatorHistory table
    op.create_table('lead_operator_history',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('machine_id', sa.Integer(), nullable=False),
        sa.Column('user_id', sa.Integer(), nullable=False),
        sa.Column('assigned_time', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.Column('removed_time', sa.DateTime(), nullable=True),
        sa.Column('assigned_by_id', sa.Integer(), nullable=True),
        sa.Column('removal_reason', sa.String(length=20), nullable=True),
        sa.ForeignKeyConstraint(['machine_id'], ['machine.id'], ),
        sa.ForeignKeyConstraint(['user_id'], ['rfid_user.id'], ),
        sa.ForeignKeyConstraint(['assigned_by_id'], ['rfid_user.id'], ),
        sa.PrimaryKeyConstraint('id')
    )
    
    # Create EStopEvent table for emergency stop system
    op.create_table('e_stop_event',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('area_id', sa.Integer(), nullable=True),
        sa.Column('zone_id', sa.Integer(), nullable=True),
        sa.Column('node_id', sa.Integer(), nullable=True),
        sa.Column('trigger_time', sa.DateTime(), nullable=True, default=sa.func.now()),
        sa.Column('reset_time', sa.DateTime(), nullable=True),
        sa.Column('triggered_by', sa.String(length=64), nullable=False),
        sa.Column('reset_by', sa.String(length=64), nullable=True),
        sa.Column('affected_machines', sa.Text(), nullable=True),
        sa.ForeignKeyConstraint(['area_id'], ['area.id'], ),
        sa.ForeignKeyConstraint(['zone_id'], ['zone.id'], ),
        sa.ForeignKeyConstraint(['node_id'], ['node.id'], ),
        sa.PrimaryKeyConstraint('id')
    )
    
    # Create BluetoothDevice table for CYD nodes
    op.create_table('bluetooth_device',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('device_name', sa.String(length=64), nullable=False),
        sa.Column('mac_address', sa.String(length=17), nullable=False),
        sa.Column('last_connected', sa.DateTime(), nullable=True),
        sa.Column('is_paired', sa.Boolean(), nullable=True, default=False),
        sa.Column('status', sa.String(length=20), nullable=True, default='disconnected'),
        sa.ForeignKeyConstraint(['node_id'], ['node.id'], ),
        sa.PrimaryKeyConstraint('id'),
        sa.UniqueConstraint('mac_address')
    )
    
    # Add area_id to Alert table
    op.add_column('alert', sa.Column('area_id', sa.Integer(), nullable=True))
    op.create_foreign_key('fk_alert_area', 'alert', 'area', ['area_id'], ['id'])
    
    # Add can_be_lead flag to RFIDUser
    op.add_column('rfid_user', sa.Column('can_be_lead', sa.Boolean(), nullable=True, default=False))
    
    # Add can_be_lead flag to MachineAuthorization
    op.add_column('machine_authorization', sa.Column('can_be_lead', sa.Boolean(), nullable=True, default=False))
    
    # Add fields to MachineLog for tracking lead operator and quality data
    op.add_column('machine_log', sa.Column('was_lead', sa.Boolean(), nullable=True, default=False))
    op.add_column('machine_log', sa.Column('rework_qty', sa.Integer(), nullable=True, default=0))
    op.add_column('machine_log', sa.Column('scrap_qty', sa.Integer(), nullable=True, default=0))
    
    # Create a default area
    conn = op.get_bind()
    conn.execute(
        sa.text("INSERT INTO area (name, description, code, active) VALUES (:name, :desc, :code, :active)"),
        {"name": "Main Facility", "desc": "Default area created during migration", "code": "MAIN", "active": True}
    )
    
    # Get the ID of the default area
    area = conn.execute(sa.text("SELECT id FROM area WHERE code = 'MAIN'")).fetchone()
    if area:
        area_id = area[0]
        
        # Associate existing zones with the default area
        conn.execute(
            sa.text("UPDATE zone SET area_id = :area_id WHERE area_id IS NULL"),
            {"area_id": area_id}
        )
        
        # Associate existing nodes with the default area
        conn.execute(
            sa.text("UPDATE node SET area_id = :area_id WHERE area_id IS NULL"),
            {"area_id": area_id}
        )


def downgrade() -> None:
    # Remove columns and constraints from existing tables
    op.drop_constraint('fk_alert_area', 'alert', type_='foreignkey')
    op.drop_column('alert', 'area_id')
    
    op.drop_column('machine_log', 'scrap_qty')
    op.drop_column('machine_log', 'rework_qty')
    op.drop_column('machine_log', 'was_lead')
    
    op.drop_column('machine_authorization', 'can_be_lead')
    
    op.drop_column('rfid_user', 'can_be_lead')
    
    op.drop_constraint('fk_machine_lead_operator', 'machine', type_='foreignkey')
    op.drop_column('machine', 'lead_operator_id')
    
    op.drop_constraint('fk_node_zone', 'node', type_='foreignkey')
    op.drop_constraint('fk_node_area', 'node', type_='foreignkey')
    op.drop_column('node', 'has_display')
    op.drop_column('node', 'zone_id')
    op.drop_column('node', 'area_id')
    
    op.drop_constraint('fk_zone_area', 'zone', type_='foreignkey')
    op.drop_column('zone', 'created_at')
    op.drop_column('zone', 'active')
    op.drop_column('zone', 'code')
    op.drop_column('zone', 'area_id')
    
    # Drop new tables
    op.drop_table('bluetooth_device')
    op.drop_table('e_stop_event')
    op.drop_table('lead_operator_history')
    op.drop_table('machine_session')
    op.drop_table('area_permission')
    op.drop_table('area')