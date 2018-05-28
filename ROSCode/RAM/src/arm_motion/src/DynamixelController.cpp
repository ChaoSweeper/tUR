//
// Created by korisd on 5/25/18.
//

#include "arm_motion/DynamixelController.h"

/*
 * Public
 */
DynamixelController::DynamixelController( std::string bus )
{
    _valid = _bench.begin( bus.c_str());

    if( _valid )
    {
        ROS_INFO( "%s: Successful startup of Dynamixel Bus %s", __FUNCTION__, bus.c_str());
        if( verifyServos( MAX_SERVO ))
        {
            ROS_INFO( "%s: Servos verified and online.", __FUNCTION__ );
            ROS_INFO( "%s: Servo defaults being send.", __FUNCTION__ );
            initializeServos();
            setupPublishers();
            setupTimer();
        }
    }
    else
    {
        ROS_ERROR( "%s: Failed to startup Dynamixel Bus %s", __FUNCTION__, bus.c_str());
    }
}
bool DynamixelController::benchWrite( ServoCommand com )
{
    bool success = false;

    if( _valid )
    {
        if( validCommand( com ))
            success = _bench.itemWrite( com.id, com.command.c_str(), com.value );

        if( !success )
            ROS_ERROR( "%s: failed to write command { %d, %s, %d }", __FUNCTION__, com.id, com.command.c_str(),
                      com.value );
    }
    else
    {
        ROS_ERROR( "%s: attempt made when controller is invalid", __FUNCTION__ );
    }
    return success;
}

int32_t DynamixelController::benchRead( uint8_t id, std::string command )
{
    int32_t ret = -1;

    if( _valid )
    {
        ret = _bench.itemRead( id, command.c_str() );
    }
    else
    {
        ROS_ERROR( "%s: attempt made when controller is invalid", __FUNCTION__ );
    }
    return ret;
}

std::vector<int32_t> &DynamixelController::getServoPositions()
{
    static std::vector<int32_t> servo_positions;
    for( int i = 0; i < MAX_SERVO; i++ )
    {
        servo_positions.push_back( servo_info[i].Present_Position );
    }
    return servo_positions;
}

inline bool DynamixelController::torqueOn()
{
    changeTorqueEnable( 1 );
}

inline bool DynamixelController::torqueOff()
{
    changeTorqueEnable( 0 );
}
bool DynamixelController::holdPosition()
{
    ServoCommand com;
    bool servo_response[MAX_SERVO];

    com.command = "Goal_Position";

    for( int i = 0; i < MAX_SERVO; i++ )
    {
        com.id = i + 1;
        com.value = servo_info[i].Present_Position;
        servo_response[i] = benchWrite( com );
    }

    return analyzeServoResponse( __FUNCTION__, servo_response );
}

bool DynamixelController::changePosition( uint8_t id, uint32_t position)
{
    ServoCommand com;

    com.id = id;
    com.command = "Goal_Position";
    com.value = position;

    return benchWrite( com );
}

bool DynamixelController::changeVelocity( uint8_t id, uint32_t velocity )
{
    ServoCommand com;

    com.id = id;
    com.command = "Profile_Velocity";
    com.value = velocity;

    return benchWrite( com );
}

/*
 * Protected
 */
void DynamixelController::validateBus()
{
    ROS_INFO( "Bus has been validated." );
    _valid = true;
}

void DynamixelController::invalidateBus()
{
    ROS_ERROR( "Dynamixel bus has been invalidated." );
    _valid = false;
}

bool DynamixelController::verifyServos( int expected_number )
{
    bool success = true;

    if( _valid )
    {

        uint8_t num_servos;
        uint8_t servo_ids[4] = {0, 0, 0, 0};

        _bench.scan( servo_ids, &num_servos, expected_number );

        if( num_servos != expected_number )
        {
            ROS_INFO( "%s: invalid number of servos found on scan.", __FUNCTION__ );
            invalidateBus();
            success = false;
        }
        else
        {
            /*
             * make sure there are no zero ID servos
             * this is sort of a useless test... but humor me for now
             */
            for( int i = 0; i < 4; i++ )
            {
                if( servo_ids[i] == 0 )
                {
                    ROS_ERROR( "%s: found a servo with the ID of zero.", __FUNCTION__ );
                    success = false;
                }
            }
        }
    }
    else
    {
        ROS_ERROR( "%s: bus is invalid.", __FUNCTION__ );
        success = false;
    }
    return success;
}

void DynamixelController::initializeServos()
{
    ServoCommand max_vel;
    ServoCommand pid_i_gain;

    /* this part can be possible wrapped up with some Lua config files */
    max_vel.command = "Velocity_Limit";
    max_vel.value = MAX_VELOCITY;
    pid_i_gain.command = "Position_I_Gain";
    pid_i_gain.value = PID_I_GAIN;

    for( int i = 0; i < MAX_SERVO; i++ )
    {
        uint8_t id = i + 1;
        max_vel.id = id;
        pid_i_gain.id = id;

        benchWrite( max_vel );
        benchWrite( pid_i_gain );
    }
}

bool DynamixelController::validCommand( ServoCommand com )
{
    bool valid = false;

    /*
     * Assume the command is bad.
     * If it is in the valid ID range and command is not ""
     * Then, check the command field to make sure its valid
     * If the command shows up in the valid_commands table, it's good.
     */
    if( com.id < 0 || com.id > MAX_SERVO )
    {
        ROS_ERROR( "%s: ID[%d] is outside acceptable ranges.", __FUNCTION__, com.id );
    }
    else if( com.command == "" )
    {
        ROS_ERROR( "%s: Attempting to send a \"\" command ", __FUNCTION__ );
    }
    else
    {
        for( int i = 0; i < MAX_COMMAND; i++ )
        {
            if( com.command == valid_commands[i] )
            {
                valid = true;
                break;
            }
        }
        if( !valid )
            ROS_ERROR( "%s: Command \"%s\" is invalid for X Series.", __FUNCTION__, com.command.c_str());
    }

    return valid;
}

void DynamixelController::setupPublishers()
{
    for( int i = 0; i < MAX_SERVO; i++ )
        servo_info_publishers[i] = node_handle.advertise<dynamixel_workbench_msgs::XH>( servo_topic_names[i].c_str(), 10 );

}

void DynamixelController::setupTimer()
{
    servo_info_timer = node_handle.createTimer( ros::Duration( 0.1 ), boost::bind( &DynamixelController::updateAndPublishServoInfo, this, _1 ) );
}

void DynamixelController::updateAndPublishServoInfo( const ros::TimerEvent &event )
{
    updateServos();
    publishServoInfo();
}

inline void DynamixelController::updateServos()
{
    for( int i = 0; i < MAX_SERVO; i++ )
    {
        /*
         * This is going to be very explicit but slightly slower in the growth department.
         * However, since this doesn't really have to deal with success checking, it should be fine.
         */
        uint8_t id = i + 1;
        servo_info[i].ID = id;
        servo_info[i].Torque_Enable = (uint8_t)  _bench.itemRead(id, "Torque_Enable");
        servo_info[i].Goal_Position = (uint32_t) _bench.itemRead(id, "Goal_Position");
        servo_info[i].Present_Position = _bench.itemRead(id, "Present_Position");
        servo_info[i].Present_Velocity = _bench.itemRead(id, "Present_Velocity");
        servo_info[i].Profile_Velocity = (uint32_t) _bench.itemRead(id, "Profile_Velocity");
        servo_info[i].Present_Temperature = (uint8_t) _bench.itemRead(id, "Present_Temperature");
    }

}

inline void DynamixelController::publishServoInfo()
{
    for( int i = 0; i < MAX_SERVO; i++ )
    {
        servo_info_publishers[i].publish( servo_info[i] );
    }

}

bool DynamixelController::changeTorqueEnable( uint8_t value )
{
    ServoCommand com;
    bool servo_response[MAX_SERVO];

    com.command = "Torque_Enable";
    com.value = value;

    for( int i = 0; i < MAX_SERVO; i++ )
    {
        com.id = i + 1;
        servo_response[i] = benchWrite( com );
    }

    return analyzeServoResponse( __FUNCTION__, servo_response );
}

bool DynamixelController::analyzeServoResponse( std::string fun_name, bool *responses )
{
    bool result = true;

    for( int i = 0; i < MAX_SERVO; i++ )
    {
        if( !responses[i] )
        {
            ROS_ERROR( "%s: %s did not respond to command", fun_name.c_str(), servo_names[i].c_str() );
            result = false;
        }
    }

    return result;
}

