#include <mc_tasks/LookAtTFTask.h>
#include <mc_tasks/MetaTaskLoader.h>

namespace mc_tasks
{
LookAtTFTask::LookAtTFTask(const std::string& bodyName,
                           const Eigen::Vector3d& bodyVector,
                           const std::string& sourceFrame,
                           const std::string& targetFrame,
                           const mc_rbdyn::Robots& robots,
                           unsigned int robotIndex, double stiffness,
                           double weight)
    : LookAtTask(bodyName, bodyVector, bodyVector, robots, robotIndex,
                 stiffness, weight),
      tfListener(tfBuffer),
      sourceFrame(sourceFrame),
      targetFrame(targetFrame)
{
  const mc_rbdyn::Robot& robot = robots.robot(rIndex);
  bIndex = robot.bodyIndexByName(bodyName);

  finalize(robots.mbs(), static_cast<int>(rIndex), bodyName, bodyVector,
           bodyVector);
  type_ = "lookAtTF";
  name_ = "look_at_TF_" + robot.name() + "_" + bodyName + "_" + targetFrame;
}

void LookAtTFTask::update()
{
  geometry_msgs::TransformStamped transformStamped;
  try
  {
    transformStamped =
        tfBuffer.lookupTransform(sourceFrame, targetFrame, ros::Time(0));
  }
  catch (tf2::TransformException& ex)
  {
    LOG_ERROR(ex.what());
    return;
  }
  Eigen::Vector3d target;
  target << transformStamped.transform.translation.x,
      transformStamped.transform.translation.y,
      transformStamped.transform.translation.z;

  LookAtTask::target(target);
}

} /* namespace mc_tasks */

namespace
{
static bool registered = mc_tasks::MetaTaskLoader::register_load_function(
    "lookAtTF",
    [](mc_solver::QPSolver& solver, const mc_rtc::Configuration& config) {
      auto t = std::make_shared<mc_tasks::LookAtTFTask>(
          config("body"), config("bodyVector"), config("sourceFrame"),
          config("targetFrame"), solver.robots(), config("robotIndex"));
      if (config.has("weight"))
      {
        t->weight(config("weight"));
      }
      if (config.has("stiffness"))
      {
        t->stiffness(config("stiffness"));
      }
      t->load(solver, config);
      return t;
    });
}
