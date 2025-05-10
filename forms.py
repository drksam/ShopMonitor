from flask_wtf import FlaskForm
from wtforms import StringField, TextAreaField, BooleanField, SelectField, SubmitField, HiddenField, IntegerField, DateTimeField
from wtforms.validators import DataRequired, Length, ValidationError, Optional
from models import Area, User, Machine, Zone, RFIDUser, MachineAuthorization

class AreaForm(FlaskForm):
    """Form for creating and editing areas"""
    name = StringField('Area Name', validators=[DataRequired(), Length(min=2, max=64)])
    description = TextAreaField('Description', validators=[Length(max=255)])
    code = StringField('Area Code', validators=[DataRequired(), Length(min=2, max=10)])
    active = BooleanField('Active')
    submit = SubmitField('Save')
    
    def validate_code(self, field):
        """Validate that area code is unique"""
        # Check if we're editing an existing area
        if hasattr(self, 'area_id') and self.area_id.data:
            area = Area.query.filter_by(code=field.data).first()
            if area and str(area.id) != self.area_id.data:
                raise ValidationError('This area code is already in use.')
        else:
            area = Area.query.filter_by(code=field.data).first()
            if area:
                raise ValidationError('This area code is already in use.')

class AreaPermissionForm(FlaskForm):
    """Form for managing area permissions"""
    user_id = SelectField('User', validators=[DataRequired()], coerce=int)
    permission_level = SelectField('Permission Level', validators=[DataRequired()],
                                 choices=[
                                     ('view', 'View Only'),
                                     ('operate', 'Operate'),
                                     ('manage', 'Manage'),
                                     ('admin', 'Administrator')
                                 ])
    submit = SubmitField('Add Permission')
    
    def __init__(self, *args, **kwargs):
        super(AreaPermissionForm, self).__init__(*args, **kwargs)
        # Populate user choices
        self.user_id.choices = [(u.id, f"{u.username} ({u.email})") 
                              for u in User.query.order_by(User.username).all()]

class ZoneForm(FlaskForm):
    """Form for creating and editing zones/locations"""
    name = StringField('Zone Name', validators=[DataRequired(), Length(min=2, max=64)])
    description = TextAreaField('Description', validators=[Length(max=255)])
    code = StringField('Zone Code', validators=[DataRequired(), Length(min=2, max=10)])
    area_id = SelectField('Area', validators=[DataRequired()], coerce=int)
    active = BooleanField('Active')
    submit = SubmitField('Save')
    
    def __init__(self, *args, **kwargs):
        super(ZoneForm, self).__init__(*args, **kwargs)
        # Populate area choices
        self.area_id.choices = [(a.id, f"{a.name} ({a.code})")
                              for a in Area.query.filter_by(active=True).order_by(Area.name).all()]
    
    def validate_code(self, field):
        """Validate that zone code is unique within an area"""
        if not self.area_id.data:
            return
            
        # Check if we're editing an existing zone
        if hasattr(self, 'zone_id') and self.zone_id.data:
            zone = Zone.query.filter_by(code=field.data, area_id=self.area_id.data).first()
            if zone and str(zone.id) != self.zone_id.data:
                raise ValidationError('This zone code is already in use in the selected area.')
        else:
            zone = Zone.query.filter_by(code=field.data, area_id=self.area_id.data).first()
            if zone:
                raise ValidationError('This zone code is already in use in the selected area.')

class MachineForm(FlaskForm):
    """Form for creating and editing machines"""
    name = StringField('Machine Name', validators=[DataRequired(), Length(min=2, max=64)])
    machine_id = StringField('Machine ID', validators=[DataRequired(), Length(min=1, max=10)])
    description = TextAreaField('Description', validators=[Length(max=255)])
    zone_id = SelectField('Location (Zone)', validators=[DataRequired()], coerce=int)
    node_id = SelectField('Controller Node', coerce=int, validators=[])
    node_port = SelectField('Node Port', choices=[(0, '1'), (1, '2'), (2, '3'), (3, '4')], coerce=int, validators=[])
    submit = SubmitField('Save')
    
    def __init__(self, *args, **kwargs):
        super(MachineForm, self).__init__(*args, **kwargs)
        # Allow empty selection for node (indicating no node)
        self.node_id.choices = [(0, 'No Controller')] + [
            (n.id, f"{n.name} ({n.node_id})")
            for n in Node.query.filter_by(node_type='machine_monitor').order_by(Node.name).all()
        ]
    
    def validate_machine_id(self, field):
        """Validate that machine ID is unique"""
        # Check if we're editing an existing machine
        if hasattr(self, 'machine_id') and self.machine_id.data:
            machine = Machine.query.filter_by(machine_id=field.data).first()
            if machine and str(machine.id) != self.machine_id.data:
                raise ValidationError('This machine ID is already in use.')
        else:
            machine = Machine.query.filter_by(machine_id=field.data).first()
            if machine:
                raise ValidationError('This machine ID is already in use.')

class LeadOperatorForm(FlaskForm):
    """Form for assigning a lead operator to a machine"""
    user_id = SelectField('Lead Operator', validators=[DataRequired()], coerce=int)
    submit = SubmitField('Assign as Lead')
    
    def __init__(self, machine_id=None, *args, **kwargs):
        super(LeadOperatorForm, self).__init__(*args, **kwargs)
        
        # Get users authorized on this machine who can be leads
        if machine_id:
            from models import MachineAuthorization, RFIDUser
            
            # Query for all users who can be lead on this machine
            users = RFIDUser.query.join(MachineAuthorization).filter(
                MachineAuthorization.machine_id == machine_id,
                MachineAuthorization.can_be_lead == True,
                RFIDUser.active == True,
                RFIDUser.can_be_lead == True
            ).all()
            
            self.user_id.choices = [(u.id, u.name) for u in users]
            
            # If no users are eligible, provide an empty list with a message
            if not users:
                self.user_id.choices = [(-1, 'No eligible lead operators')]

class MultiUserLoginForm(FlaskForm):
    """Form for logging multiple users into a machine"""
    rfid_tag = StringField('RFID Tag', validators=[DataRequired()])
    is_lead = BooleanField('Log in as Lead Operator')
    machine_id = HiddenField('Machine ID', validators=[DataRequired()])
    submit = SubmitField('Log In')
    
    def validate_is_lead(self, field):
        """Validate that user can be a lead if they're trying to log in as one"""
        if field.data:
            from models import RFIDUser, Machine, MachineAuthorization
            
            # Get the user by RFID tag
            user = RFIDUser.query.filter_by(rfid_tag=self.rfid_tag.data).first()
            if not user:
                return
                
            # Get the machine 
            machine = Machine.query.filter_by(id=self.machine_id.data).first()
            if not machine:
                return
                
            # Check if user can be a lead
            if not user.can_be_lead:
                raise ValidationError('This user cannot be assigned as a lead operator.')
                
            # Check if user is authorized to be a lead on this machine
            auth = MachineAuthorization.query.filter_by(
                user_id=user.id,
                machine_id=machine.id,
                can_be_lead=True
            ).first()
            
            if not auth:
                raise ValidationError('This user is not authorized to be a lead on this machine.')

class QualityReportForm(FlaskForm):
    """Form for reporting rework and scrap quantities"""
    rework_qty = StringField('Rework Quantity', validators=[DataRequired()], default='0')
    scrap_qty = StringField('Scrap Quantity', validators=[DataRequired()], default='0')
    notes = TextAreaField('Notes', validators=[Length(max=255)])
    submit = SubmitField('Submit Report')