// Copyright (c) 2017 Franka Emika GmbH
// Use of this source code is governed by the Apache-2.0 license, see LICENSE
#include <mars_control/cartesian_pose_controller.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

#include <controller_interface/controller_base.h>
#include <franka_hw/franka_cartesian_command_interface.h>
#include <hardware_interface/hardware_interface.h>
#include <pluginlib/class_list_macros.h>
#include <ros/ros.h>
#include <mars_msgs/CableFollowingDebug.msg>

namespace mars_control
{

  bool CartesianPoseController::init(hardware_interface::RobotHW *robot_hardware,
                                     ros::NodeHandle &node_handle)
  {
    cartesian_pose_interface_ = robot_hardware->get<franka_hw::FrankaPoseCartesianInterface>();
    if (cartesian_pose_interface_ == nullptr)
    {
      ROS_ERROR(
          "CartesianPoseController: Could not get Cartesian Pose "
          "interface from hardware");
      return false;
    }

    if (!node_handle.getParam("is_debug", is_debug_))
    {
      ROS_ERROR("CartesianPoseController: Could not get parameter is_debug");
      return false;
    }  

    if (is_debug_) {
      debug_pub_ = n.advertise<mars_msgs::CableFollowingDebug>("cable_following_debug", 10);
    }

    std::string arm_id;
    if (!node_handle.getParam("arm_id", arm_id))
    {
      ROS_ERROR("CartesianPoseController: Could not get parameter arm_id");
      return false;
    }

    try
    {
      cartesian_pose_handle_ = std::make_unique<franka_hw::FrankaCartesianPoseHandle>(
          cartesian_pose_interface_->getHandle(arm_id + "_robot"));
    }
    catch (const hardware_interface::HardwareInterfaceException &e)
    {
      ROS_ERROR_STREAM(
          "CartesianPoseController: Exception getting Cartesian handle: " << e.what());
      return false;
    }

    auto state_interface = robot_hardware->get<franka_hw::FrankaStateInterface>();
    if (state_interface == nullptr)
    {
      ROS_ERROR("CartesianPoseController: Could not get state interface from hardware");
      return false;
    }

    try
    {
      auto state_handle = state_interface->getHandle(arm_id + "_robot");

      std::array<double, 7> q_start{{0, -M_PI_4, 0, -3 * M_PI_4, 0, M_PI_2, M_PI_4}};
      for (size_t i = 0; i < q_start.size(); i++)
      {
        if (std::abs(state_handle.getRobotState().q_d[i] - q_start[i]) > 0.1)
        {
          ROS_ERROR_STREAM(
              "CartesianPoseController: Robot is not in the expected starting position for "
              "running this example. Run `roslaunch franka_example_controllers move_to_start.launch "
              "robot_ip:=<robot-ip> load_gripper:=<has-attached-gripper>` first.");
          return false;
        }
      }
    }
    catch (const hardware_interface::HardwareInterfaceException &e)
    {
      ROS_ERROR_STREAM(
          "CartesianPoseController: Exception getting state handle: " << e.what());
      return false;
    }

    node_handle.subscribe("desired_pose", 1, &CartesianPoseController::desiredPoseCallback, this);

    return true;
  }

  void CartesianPoseController::starting(const ros::Time & /* time */)
  {
    initial_pose_ = cartesian_pose_handle_->getRobotState().O_T_EE_d;
    elapsed_time_ = ros::Duration(0.0);
  }

  void CartesianPoseController::update(const ros::Time & /* time */,
                                       const ros::Duration &period)
  {
    elapsed_time_ += period;

    // Testing subscriber
    ROS_INFO("I HEARD [%s]", test);

    // Replace with desired from topic above
    //  double delta_z = radius * (std::cos(angle) - 1);
    //  std::array<double, 16> new_pose = initial_pose_; // A 4x4 transformation matrix from origin to EE frame
    //  new_pose[12] -= delta_x;
    //  new_pose[14] -= delta_z;
    //  cartesian_pose_handle_->setCommand(new_pose);
  }

  void CartesianPoseController::desiredPoseCallback(const std_msgs::String::ConstPtr &msg)
  {
    // Replace with message to represent actual pose
    test = msg->data.c_str();
  }


}

PLUGINLIB_EXPORT_CLASS(mars_control::CartesianPoseController, controller_interface::ControllerBase)