//
// Created by qiayuan on 2022/7/18.
//
#include "legged_controllers/target_trajectories_goal_publisher.h"

#include <ocs2_core/Types.h>
#include <ocs2_core/misc/LoadData.h>

using namespace legged;

namespace
{
scalar_t TARGET_DISPLACEMENT_VELOCITY;
scalar_t TARGET_ROTATION_VELOCITY;
scalar_t COM_HEIGHT;
vector_t DEFAULT_JOINT_STATE(12);
}  // namespace

scalar_t estimateTimeToTarget(const vector_t& desired_base_displacement)
{
  const scalar_t& dx = desired_base_displacement(0);
  const scalar_t& dy = desired_base_displacement(1);
  const scalar_t& dyaw = desired_base_displacement(3);
  const scalar_t rotation_time = std::abs(dyaw) / TARGET_ROTATION_VELOCITY;
  const scalar_t displacement = std::sqrt(dx * dx + dy * dy);
  const scalar_t displacement_time = displacement / TARGET_DISPLACEMENT_VELOCITY;
  return std::max(rotation_time, displacement_time);
}

TargetTrajectories commandLineToTargetTrajectories(const vector_t& goal, const SystemObservation& observation)
{
  const vector_t target_pose = [&]() {
    vector_t target(6);
    target(0) = goal(0);
    target(1) = goal(1);
    target(2) = COM_HEIGHT;
    target(3) = goal(3);
    target(4) = 0;
    target(5) = 0;
    return target;
  }();

  const vector_t current_pose = observation.state.segment<6>(6);
  // target reaching duration
  const scalar_t target_reaching_time = observation.time + estimateTimeToTarget(target_pose - current_pose);

  // desired time trajectory
  const scalar_array_t time_trajectory{ observation.time, target_reaching_time };

  // desired state trajectory
  vector_array_t state_trajectory(2, vector_t::Zero(observation.state.size()));
  state_trajectory[0] << vector_t::Zero(6), current_pose, DEFAULT_JOINT_STATE;
  state_trajectory[1] << vector_t::Zero(6), target_pose, DEFAULT_JOINT_STATE;

  // desired input trajectory (just right dimensions, they are not used)
  const vector_array_t input_trajectory(2, vector_t::Zero(observation.input.size()));

  return { time_trajectory, state_trajectory, input_trajectory };
}

int main(int argc, char* argv[])
{
  const std::string robot_name = "legged_robot";

  // Initialize ros node
  ::ros::init(argc, argv, robot_name + "_target");
  ::ros::NodeHandle node_handle;
  // Get node parameters
  std::string reference_file;
  node_handle.getParam("/reference_file", reference_file);

  loadData::loadCppDataType(reference_file, "comHeight", COM_HEIGHT);
  loadData::loadEigenMatrix(reference_file, "defaultJointState", DEFAULT_JOINT_STATE);
  loadData::loadCppDataType(reference_file, "targetRotationVelocity", TARGET_ROTATION_VELOCITY);
  loadData::loadCppDataType(reference_file, "targetDisplacementVelocity", TARGET_DISPLACEMENT_VELOCITY);

  TargetTrajectoriesGoalPublisher target_pose_command(node_handle, robot_name, &commandLineToTargetTrajectories);

  ros::spin();
  // Successful exit
  return 0;
}