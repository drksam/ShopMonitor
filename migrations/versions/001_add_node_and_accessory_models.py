"""Add Node and Accessory models

Revision ID: 001
Revises: 
Create Date: 2025-04-16

"""
from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision = '001'
down_revision = None
branch_labels = None
depends_on = None


def upgrade() -> None:
    # Create Node table
    op.create_table('node',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('node_id', sa.String(length=32), nullable=False),
        sa.Column('name', sa.String(length=64), nullable=False),
        sa.Column('description', sa.String(length=255), nullable=True),
        sa.Column('ip_address', sa.String(length=15), nullable=True),
        sa.Column('node_type', sa.String(length=20), nullable=True, default='machine_monitor'),
        sa.Column('is_esp32', sa.Boolean(), nullable=True, default=False),
        sa.Column('last_seen', sa.DateTime(), nullable=True),
        sa.Column('firmware_version', sa.String(length=20), nullable=True),
        sa.Column('config', sa.Text(), nullable=True),
        sa.PrimaryKeyConstraint('id'),
        sa.UniqueConstraint('node_id')
    )
    
    # Add node_id and node_port columns to machine table
    op.add_column('machine', sa.Column('node_id', sa.Integer(), nullable=True))
    op.add_column('machine', sa.Column('node_port', sa.Integer(), nullable=True, default=0))
    op.create_foreign_key('fk_machine_node', 'machine', 'node', ['node_id'], ['id'])
    
    # Create AccessoryIO table
    op.create_table('accessory_io',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('name', sa.String(length=64), nullable=False),
        sa.Column('description', sa.String(length=255), nullable=True),
        sa.Column('io_type', sa.String(length=10), nullable=False),
        sa.Column('io_number', sa.Integer(), nullable=False),
        sa.Column('linked_machine_id', sa.Integer(), nullable=True),
        sa.Column('activation_delay', sa.Integer(), nullable=True, default=0),
        sa.Column('deactivation_delay', sa.Integer(), nullable=True, default=0),
        sa.Column('active', sa.Boolean(), nullable=True, default=True),
        sa.ForeignKeyConstraint(['node_id'], ['node.id'], ),
        sa.ForeignKeyConstraint(['linked_machine_id'], ['machine.id'], ),
        sa.PrimaryKeyConstraint('id'),
        sa.UniqueConstraint('node_id', 'io_type', 'io_number')
    )
    
    # Create temporary node table for existing machines with IP addresses
    conn = op.get_bind()
    
    # Get existing machines with IP addresses
    machines = conn.execute(sa.text("SELECT id, name, ip_address FROM machine WHERE ip_address IS NOT NULL AND ip_address != ''")).fetchall()
    
    # Create nodes for each machine that has an IP address
    for machine in machines:
        # Insert node record
        node_name = f"Legacy Node for {machine[1]}"
        node_id = f"legacy-{machine[0]}"
        
        conn.execute(
            sa.text("INSERT INTO node (node_id, name, ip_address, node_type, is_esp32) VALUES (:node_id, :name, :ip, :type, :esp32)"),
            {"node_id": node_id, "name": node_name, "ip": machine[2], "type": "machine_monitor", "esp32": False}
        )
        
        # Get the newly created node ID
        node = conn.execute(sa.text("SELECT id FROM node WHERE node_id = :node_id"), {"node_id": node_id}).fetchone()
        if node:
            # Update the machine to link to this node
            conn.execute(
                sa.text("UPDATE machine SET node_id = :node_id, node_port = 0 WHERE id = :machine_id"),
                {"node_id": node[0], "machine_id": machine[0]}
            )


def downgrade() -> None:
    # Remove foreign key constraint from machine table
    op.drop_constraint('fk_machine_node', 'machine', type_='foreignkey')
    
    # Drop accessory_io table
    op.drop_table('accessory_io')
    
    # Remove node_id and node_port columns from machine table
    op.drop_column('machine', 'node_port')
    op.drop_column('machine', 'node_id')
    
    # Drop node table
    op.drop_table('node')