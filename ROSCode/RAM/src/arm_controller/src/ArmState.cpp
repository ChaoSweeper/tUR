//
// Created by korisd on 5/1/18.
//

#include "arm_controller/ArmState.h"

ArmState:: ArmState( InputsTable *i, std::string name ) : State( name ), inputs(i) {}

std::string ArmState::transition()
{
    std::string transition_to = this->getIdentifier();

    switch( inputs->desired_mode )
    {
        default:
            break;
        case OFF_MODE:
            transition_to = OFF_STATE;
            break;
        case PAUSE_MODE:
            transition_to = PAUSE_STATE;
            break;
        case GO_MODE:
            transition_to = GO_STATE;
            break;
        case GO_SYNCHRONIZED_MODE:
            transition_to = GO_SYNCHRONIZED_STATE;
            break;

    }

    return transition_to;
}

std::vector<ServoCommand> ArmState::getOutputs() { return outputs; }

void ArmState::torqueOn()
{
    for( int i = ROTATION_SERVO; i < MAX_SERVO; i++ )
    {
        ServoCommand com;
        com.id = i + 1;
        com.command = "Torque_Enable";
        com.value = 1;

        outputs.push_back( com );
    }
}

void ArmState::torqueOff()
{
    for( int i = ROTATION_SERVO; i < MAX_SERVO; i++ )
    {
        ServoCommand com;
        com.id = i + 1;
        com.command = "Torque_Enable";
        com.value = 0;

        outputs.push_back( com );
    }
}

void ArmState::holdPosition()
{
    for( int i = ROTATION_SERVO; i < MAX_SERVO; i++ )
    {
        int id = i + 1;
        ServoCommand com;
        com.id = id;
        com.value = inputs->servos[i].Present_Position;
        com.command = "Goal_Position";

        outputs.push_back( com );
    }
}

void ArmState::resetCommands()
{
    outputs.clear();
}

uint32_t ArmState::radianToValue(double radian, int32_t max_position, int32_t min_position, float max_radian,
                                 float min_radian)
{
    int32_t value = 0;
    int32_t zero_position = (max_position + min_position)/2;

    if (radian > 0)
    {
        value = (radian * (max_position - zero_position) / max_radian) + zero_position;
    }
    else if (radian < 0)
    {
        value = (radian * (min_position - zero_position) / min_radian) + zero_position;
    }
    else
    {
        value = zero_position;
    }

    return value;
}

ServoCommand ArmState::generatePositionCommand(int id, double position)
{
    ServoCommand com;

    com.id = id;
    com.command = "Goal_Position";
    com.value = radianToValue( position, 4095, 0 );

    return com;
}

ServoCommand ArmState::generateVelocityCommand(int id, int velocity)
{
    ServoCommand com;

    com.id = id;
    com.command = "Profile_Velocity";
    com.value = velocity;

    return com;
}

kinematics::Coordinates ArmState::poseToCoordinates( geometry_msgs::Pose pose )
{
    kinematics::Coordinates coords;

    coords.x = pose.position.x;
    coords.y = pose.position.y;
    coords.z = pose.position.z;

    return coords;
}

double ArmState::poseMagnitude(geometry_msgs::Pose pose)
{
    /*
     * pythag in 3 dimensions
     */
    double x = pose.position.x;
    double y = pose.position.y;
    double z = pose.position.z;

    return sqrt( x*x + y*y + z*z );
}

bool ArmState::checkTolerances( geometry_msgs::Pose pose )
{
    bool tolerances_good = true;

    double pose_magnitude = poseMagnitude( pose );
 /*   if( pose_magnitude > MAX_MAG || pose_magnitude < MIN_MAG )
    {
        messaging::errorMsg( __FUNCTION__, "Goal Pose violates min/max conditions" );
        tolerances_good = false;
    }
  */
    if( tolerances_good == true )
    {
        kinematics::Coordinates coords = poseToCoordinates( pose );
        kinematics::Joints joints = kinematics::inverseKinematics( coords, pose.orientation.w );

        for( int i = ROTATION_SERVO; i < MAX_SERVO; i++ )
        {
            uint16_t servo_min = servo_min_max[i][0];
            uint16_t servo_max = servo_min_max[i][1];
            uint16_t servo_position = radianToValue( joints[i], 4095, 0 );
            if( servo_position < servo_min || servo_position > servo_max )
            {
                std::cout << "Joint[" << i << "] is breaking at " << servo_position << " position" << std::endl;
                messaging::errorMsg( __FUNCTION__, "Joint value outside of min/max range for servo" );
                tolerances_good = false;
                break;
            }
        }

    }

    return tolerances_good;
}