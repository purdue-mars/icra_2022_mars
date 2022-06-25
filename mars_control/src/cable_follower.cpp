// Copyright (c) 2017 Franka Emika GmbH
// Use of this source code is governed by the Apache-2.0 license, see LICENSE
#include <mars_control/cable_follower.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <Eigen/Dense>
#include <controller_interface/controller_base.h>
#include <franka_hw/franka_velocity_command_interface.h>
#include <hardware_interface/hardware_interface.h>
#include <pluginlib/class_list_macros.h>
#include <ros/ros.h>
#include <mars_msgs/CableFollowingDebug.h>
#include <geometry_msgs/Pose.h>

namespace mars_control
{

  bool CableFollower::init(hardware_interface::RobotHW *robot_hardware,
                                     ros::NodeHandle &node_handle)
  {
    cartesian_velocity_interface_ = robot_hardware->get<franka_hw::FrankaVelocityCartesianInterface>();
    if (cartesian_velocity_interface_ == nullptr)
    {
      ROS_ERROR(
          "CableFollower: Could not get Cartesian Velocity "
          "interface from hardware");
      return false;
    }

    if (!node_handle.getParam("is_debug", is_debug_))
    {
      ROS_ERROR("CableFollower: Could not get parameter is_debug");
      return false;
    }

    if (is_debug_) {
      debug_pub_ = n.advertise<mars_msgs::CableFollowingDebug>("cable_following_debug", 10);
    }

    if (!node_handle.getParam("fixed_pose/x", fixed_pos_(0)) ||
        !node_handle.getParam("fixed_pose/y", fixed_pos_(1)) ||
        !node_handle.getParam("fixed_pose/z", fixed_pos_(2)) ||
        !node_handle.getParam("fixed_pose/qx", fixed_quat_(0)) ||
        !node_handle.getParam("fixed_pose/qy", fixed_quat_(1)) ||
        !node_handle.getParam("fixed_pose/qz", fixed_quat_(2)) ||
        !node_handle.getParam("fixed_pose/qw", fixed_quat_(3))) {
      ROS_ERROR("CableFollower: Could not get parameter fixed_pose");
      return false;
    }

    std::string arm_id;
    if (!node_handle.getParam("arm_id", arm_id))
    {
      ROS_ERROR("CableFollower: Could not get parameter arm_id");
      return false;
    }

    try
    {
      cartesian_velocity_handle_ = std::make_unique<franka_hw::FrankaCartesianVelocityHandle>(
          cartesian_velocity_interface_->getHandle(arm_id + "_robot"));
    }
    catch (const hardware_interface::HardwareInterfaceException &e)
    {
      ROS_ERROR_STREAM(
          "CableFollower: Exception getting Cartesian handle: " << e.what());
      return false;
    }

    auto state_interface = robot_hardware->get<franka_hw::FrankaStateInterface>();
    if (state_interface == nullptr)
    {
      ROS_ERROR("CableFollower: Could not get state interface from hardware");
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
              "CableFollower: Robot is not in the expected starting position for "
              "running this example. Run `roslaunch franka_example_controllers move_to_start.launch "
              "robot_ip:=<robot-ip> load_gripper:=<has-attached-gripper>` first.");
          return false;
        }
      }
    }
    catch (const hardware_interface::HardwareInterfaceException &e)
    {
      ROS_ERROR_STREAM(
          "CableFollower: Exception getting state handle: " << e.what());
      return false;
    }

    node_handle.subscribe("/contact", 1, &CableFollower::gelsightCallback, this);

    return true;
  }

  void CableFollower::starting(const ros::Time & /* time */)
  {
      elapsed_time_ = ros::Duration(0.0);
  }

  void CableFollower::update(const ros::Time & /* time */,
                             const ros::Duration &period)
  {
    elapsed_time_ += period;

    // Get current EE pose
    std::array<double, 16> m = cartesian_pose_handle_->getRobotState().O_T_EE_d;
    Eigen::Vector3d pos(m[12], m[13], m[14]);
    Eigen::Quaterniond quat(0.0, 0.0, 0.0, 1.0);
    quat(0) = sqrt(1.0 + m[0] + m[5] + m[10]) / 2.0;
    quat(1) = (m[6] - m[9]) / (quat(0) * 4.0);
    quat(2) = (m[8] - m[2]) / (quat(0) * 4.0);
    quat(3) = (m[1] - m[4]) / (quat(0) * 4.0);

    // Find pose wrt fixed frame
    pos -= fixed_pos_;
    quat *= fixed_quat.inverse();

    // Get cable pose from GelSight
    double y = cable_pos_(1) +  pos(1);
    double theta = 0.0;

    // Calculate model state (y, theta, alpha)
    double alpha = 0.0; // Use x, y and y_global
    Eigen::Vector3d x(y, theta, alpha);

    // Calculate phi from K
    Eigen::Vector3d K(-900.28427003, -9.54405588, 13.36354662);
    double phi = -K.dot(x);

    // Calculate velocity command from phi
    double target_dir = phi + alpha;
    target_dir = max(-3.14159265 / 3.0, min(target_dir, 3.14159265 / 3.0));
    double vnorm = 0.025;
    std::array<double, 6>
        cmd = {{vnorm * cos(target_dir), vnrom * sin(target_dir), 0.0, 0.0, 0.0, 0.0}};

    // Publish velocity to Franka
    velocity_cartesian_handle_->setCommand(cmd);

    if (is_debug_) {
      // Create pose msg
      geometry_msgs::Pose pose_msg;
      pose_msg.position.x = pos(0);
      pose_msg.position.y = pos(1);
      pose_msg.position.z = pos(2);

      pose_msg.orientation.w = quat(0);
      pose_msg.orientation.x = quat(1);
      pose_msg.orientation.y = quat(2);
      pose_msg.orientation.z = quat(3);

      // Publish debug message
      mars_msgs::CableFollowingDebug msg;
      msg.ee_pose = pose_msg;
      msg.cable_y = y;
      msg.cable_theta = theta;
      msg.cable_alpha = alpha;
      msg.output_phi = phi;
      msg.output_v = vnorm;
      debug_pub_.publish(msg);
    }
  }

  void CableFollower::stopping(const ros::Time&) {
    // DO NOT PUBLISH ZERO VELOCITIES
  }

  void CableFollower::gelsightCallback(const geometry_msgs::PoseStamped::ConstPtr &msg)
  {
    cable_pos_(0) = msg.pose.position.x;
    cable_pos_(1) = msg.pose.position.y;
    cable_pos_(2) = msg.pose.position.z;

    cable_quat_(0) = msg.pose.orientation.x;
    cable_quat_(1) = msg.pose.orientation.y;
    cable_quat_(2) = msg.pose.orientation.z;
    cable_quat_(3) = msg.pose.orientation.w;
  }
}

PLUGINLIB_EXPORT_CLASS(mars_control::CableFollower, controller_interface::ControllerBase)