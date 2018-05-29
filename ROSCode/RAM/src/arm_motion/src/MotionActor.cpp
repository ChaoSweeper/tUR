//
// Created by korisd on 5/25/18.
//

#include "arm_motion/MotionActor.h"

MotionActor::MotionActor( std::string name, DynamixelController &controller ) :
        action_server( node_handle, name, false ),
        action_name(name),
        _controller(controller)
{
    action_server.registerGoalCallback( boost::bind( &MotionActor::goalCallBack, this ) );
    action_server.registerPreemptCallback( boost::bind( &MotionActor::preemptCallBack, this ) );
    action_timer = node_handle.createTimer( ros::Duration(0.1), boost::bind( &MotionActor::motionMonitor, this, _1 ), false, false );
}

void MotionActor::goalCallBack()
{
    /*setup the new motion*/
    for( int i = 0; i < MAX_SERVO; i++ )
    {
        joint_goals[i] = action_server.acceptNewGoal()->joints[i];
    }
    goal_step = 0;
    goal_max = (uint8_t)joint_goals[0].position.size();
    performMotionStep(); //the first, as soon as it receives the goal
    action_timer.start();
}

void MotionActor::preemptCallBack()
{
    result.success = false;
    action_server.setPreempted( result );
}

void MotionActor::motionMonitor( const ros::TimerEvent &event )
{
    if( action_server.isActive() )
    {
        if( checkMotionStep() ); // check for arrival
        {
            goal_step++;
            if( goal_step != goal_max )
            {
                /* if we haven't completed motion */
                if( !performMotionStep() )
                {
                    result.success = false;
                    action_server.setAborted( result );
                }
                else
                {
                    feedback.on_step = goal_step;
                    action_server.publishFeedback( feedback );
                }
            }
            else
            {
                /* motion is completed */
                result.success = true;
                action_server.setSucceeded( result );
            }
        }
    }
    else
    {
        /* we got preempted */
        action_timer.stop();
    }
}

bool MotionActor::checkMotionStep()
{
    bool arrived = true;
    std::vector<int32_t> servo_position = _controller.getServoPositions();

    for( int i = 0; i < MAX_SERVO; i++ )
    {
        if( abs( servo_position[i] - (int32_t)joint_goals[i].position[goal_step] )
            > (int32_t)joint_goals[i].effort[goal_step] )
        {
            arrived = false;
            break;
        }
    }
    return arrived;
}

bool MotionActor::performMotionStep()
{
    bool success = true;
    if( goal_step != goal_max )
    {
        for( uint8_t i = 0; i < MAX_SERVO; i++ )
        {
            uint8_t id = i + (uint8_t)1;
            uint32_t position = (uint32_t)joint_goals[i].position[goal_step];
            uint32_t velocity = (uint32_t)joint_goals[i].velocity[goal_step];

            bool status = _controller.changePosition( id, position );
            if( !status )
            {
                ROS_ERROR( "%s: failed to write goal position[%d] to servo[%d]", __FUNCTION__, position, id );
                success = false;
            }

            status = _controller.changeVelocity( id, velocity );
            if( !status )
            {
                ROS_ERROR( "%s: failed to write profile velocity[%d] to servo[%d}", __FUNCTION__, velocity, id );
                success = false;
            }
        }
    }
    else
    {
        ROS_ERROR( "%s: attempting to perform a goal[%d}] outside the bounds of the goal vectors.", __FUNCTION__, goal_step );
        success = false;
    }
    return success;
}