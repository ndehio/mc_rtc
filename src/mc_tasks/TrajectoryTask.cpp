#include <mc_tasks/TrajectoryTask.h>
#include <mc_tasks/MetaTaskLoader.h>

#include <mc_rbdyn/Surface.h>
#include <mc_trajectory/spline_utils.h>

namespace mc_tasks
{
TrajectoryTask::TrajectoryTask(
    const mc_rbdyn::Robots& robots, unsigned int robotIndex,
    const std::string& surfaceName, const sva::PTransformd& X_0_t,
    double duration, double timeStep, double stiffness, double posWeight,
    double oriWeight, const Eigen::MatrixXd& waypoints, unsigned int nrWP)
    : robots(robots),
      rIndex(robotIndex),
      surfaceName(surfaceName),
      X_0_t(X_0_t),
      wp(waypoints),
      duration(duration),
      timeStep(timeStep),
      t(0.)
{
  const mc_rbdyn::Robot& robot = robots.robot(robotIndex);
  const auto& surface = robot.surface(surfaceName);
  type_ = "trajectory";
  name_ = "trajectory_" + robot.name() + "_" + surface.name();
  X_0_start = surface.X_0_s(robot);

  if(nrWP > 0)
  {
    Eigen::Vector3d start = X_0_start.translation();
    Eigen::Vector3d end = X_0_t.translation();
    wp = mc_trajectory::generateInterpolatedWaypoints(start, end, nrWP);
  }

  transTask.reset(new tasks::qp::TransformTask(robots.mbs(), static_cast<int>(robotIndex), surface.bodyName(), X_0_start, surface.X_b_s()));
  stiffness_ = stiffness;
  transTrajTask.reset(new tasks::qp::TrajectoryTask(robots.mbs(), static_cast<int>(robotIndex), transTask.get(), stiffness, 2*sqrt(stiffness), 1.0));
  for(unsigned int i = 0; i < 3; ++i)
  {
    dimWeight_(i) = oriWeight;
  }
  for(unsigned int i = 3; i < 6; ++i)
  {
    dimWeight_(i) = posWeight;
  }
  transTrajTask->dimWeight(dimWeight_);

  generateBS();
}

void TrajectoryTask::addToSolver(mc_solver::QPSolver & solver)
{
  if(!inSolver)
  {
    solver.addTask(transTrajTask.get());
    inSolver = true;
  }
}

void TrajectoryTask::removeFromSolver(mc_solver::QPSolver & solver)
{
  if(inSolver)
  {
    solver.removeTask(transTrajTask.get());
    inSolver = false;
  }
}

void TrajectoryTask::update()
{
  auto res = bspline->splev({t}, 2);
  Eigen::Vector3d & pos = res[0][0];
  Eigen::Vector3d & vel = res[0][1];
  Eigen::Vector3d & acc = res[0][2];
  sva::PTransformd interp = sva::interpolate(X_0_start, X_0_t, t / duration);
  sva::PTransformd target(interp.rotation(), pos);
  Eigen::VectorXd refVel(6);
  Eigen::VectorXd refAcc(6);
  for(unsigned int i = 0; i < 3; ++i)
  {
    refVel(i) = 0;
    refAcc(i) = 0;
    refVel(i+3) = vel(i);
    refAcc(i+3) = acc(i);
  }
  transTask->target(target);
  transTrajTask->refVel(refVel);
  transTrajTask->refAccel(refAcc);

  t = std::min(t + timeStep, duration);
}

bool TrajectoryTask::timeElapsed()
{
  return t >= duration;
}

Eigen::VectorXd TrajectoryTask::eval() const
{
  const auto& robot = robots.robot(rIndex);
  sva::PTransformd X_0_s = robot.surface(surfaceName).X_0_s(robot);
  return sva::transformError(X_0_s, X_0_t).vector();
}

Eigen::VectorXd TrajectoryTask::evalTracking() const
{
  return transTask->eval();
}

Eigen::VectorXd TrajectoryTask::speed() const
{
  return transTask->speed();
}

std::vector<Eigen::Vector3d> TrajectoryTask::controlPoints()
{
  std::vector<Eigen::Vector3d> res;
  res.reserve(static_cast<unsigned int>(wp.size()) + 2);
  res.push_back(X_0_start.translation());
  for(int i = 0; i < wp.cols(); ++i)
  {
    Eigen::Vector3d tmp;
    tmp(0) = wp(0, i);
    tmp(1) = wp(1, i);
    tmp(2) = wp(2, i);
    res.push_back(tmp);
  }
  res.push_back(X_0_t.translation());
  return res;
}

void TrajectoryTask::generateBS()
{
  bspline.reset(new mc_trajectory::BSplineTrajectory(controlPoints(), duration));
}

void TrajectoryTask::selectActiveJoints(mc_solver::QPSolver & solver,
                                        const std::vector<std::string> & activeJoints)
{
  bool putBack = inSolver;
  if(putBack)
  {
    removeFromSolver(solver);
  }
  selectorT = std::make_shared<tasks::qp::JointsSelector>(tasks::qp::JointsSelector::ActiveJoints(robots.mbs(), rIndex, transTask.get(), activeJoints));
  transTrajTask = std::make_shared<tasks::qp::TrajectoryTask>(robots.mbs(), rIndex, selectorT.get(), stiffness_, 2*sqrt(stiffness_), 1.0);
  transTrajTask->dimWeight(dimWeight_);
  if(putBack)
  {
    addToSolver(solver);
  }
}

void TrajectoryTask::selectUnactiveJoints(mc_solver::QPSolver & solver,
                                          const std::vector<std::string> & unactiveJoints)
{
  bool putBack = inSolver;
  if(putBack)
  {
    removeFromSolver(solver);
  }
  selectorT = std::make_shared<tasks::qp::JointsSelector>(tasks::qp::JointsSelector::UnactiveJoints(robots.mbs(), rIndex, transTask.get(), unactiveJoints));
  transTrajTask = std::make_shared<tasks::qp::TrajectoryTask>(robots.mbs(), rIndex, selectorT.get(), stiffness_, 2*sqrt(stiffness_), 1.0);
  transTrajTask->dimWeight(dimWeight_);
  if(putBack)
  {
    addToSolver(solver);
  }
}

void TrajectoryTask::resetJointsSelector(mc_solver::QPSolver & solver)
{
  bool putBack = inSolver;
  if(putBack)
  {
    removeFromSolver(solver);
  }
  selectorT = nullptr;
  transTrajTask = std::make_shared<tasks::qp::TrajectoryTask>(robots.mbs(), rIndex, transTask.get(), stiffness_, 2*sqrt(stiffness_), 1.0);
  transTrajTask->dimWeight(dimWeight_);
  if(putBack)
  {
    addToSolver(solver);
  }
}

void TrajectoryTask::dimWeight(const Eigen::VectorXd & dimW)
{
  assert(dimW.size() == 6);
  transTrajTask->dimWeight(dimW);
}


sva::PTransformd TrajectoryTask::target() const
{
  return X_0_t;
}

void TrajectoryTask::addToLogger(mc_rtc::Logger & logger)
{
  logger.addLogEntry(name_ + "_surface_pose",
                     [this]()
                     {
                       const auto & robot = robots.robot(rIndex);
                       return robot.surface(surfaceName).X_0_s(robot);
                     });
  logger.addLogEntry(name_ + "_target_trajectory_pose",
                     [this]()
                     {
                       return this->transTask->target();
                     });
  logger.addLogEntry(name_ + "_target_pose",
                     [this]()
                     {
                       return this->X_0_t;
                     });
}

void TrajectoryTask::removeFromLogger(mc_rtc::Logger & logger)
{
  logger.removeLogEntry(name_ + "_surface_pose");
  logger.removeLogEntry(name_ + "_target_trajectory_pose");
  logger.removeLogEntry(name_ + "_target_pose");
}

void TrajectoryTask::addToGUI(mc_rtc::gui::StateBuilder & gui)
{
  MetaTask::addToGUI(gui);
  gui.addElement(
    {"Tasks", name_},
    mc_rtc::gui::Transform("pos_target",
                           [this]() { return this->target(); }),
    mc_rtc::gui::Transform("traj_target",
                           [this]() { return this->transTask->target(); }),
    mc_rtc::gui::Transform("pos",
                           [this]()
                           {
                             return robots.robot(rIndex).surface(surfaceName).X_0_s(robots.robot(rIndex));
                           })
  );
}

}

namespace
{
static bool registered = mc_tasks::MetaTaskLoader::register_load_function(
    "trajectoryTask",
    [](mc_solver::QPSolver& solver, const mc_rtc::Configuration& config)
    {

      sva::PTransformd X_0_t;
      unsigned int nrWP = 0;
      Eigen::MatrixXd waypoints;
      const auto robotIndex = config("robotIndex");

      if(config.has("nrWP"))
      {
        nrWP = config("nrWP");
      }

      if (config.has("targetSurface"))
      { // Target defined from a target surface, with an offset defined
        // in the surface coordinates
        const auto& c = config("targetSurface");
        const auto& surfaceName = c("surface");
        const auto& robot = solver.robot(c("robotIndex"));

        sva::PTransformd offset = sva::PTransformd::Identity();
        if (c.has("offset"))
        {
          const auto& o = c("offset");
          Eigen::Vector3d trans = o("translation");
          Eigen::Vector3d rpy = o("rotation");
          using namespace Eigen;
          Eigen::Matrix3d m;
          m = AngleAxisd(rpy.x() * M_PI / 180., Vector3d::UnitX()) *
              AngleAxisd(rpy.y() * M_PI / 180., Vector3d::UnitY()) *
              AngleAxisd(rpy.z() * M_PI / 180., Vector3d::UnitZ());
          offset = sva::PTransformd(m.inverse(), trans);
        }

        sva::PTransformd targetSurface = robot.surface(surfaceName).X_0_s(robot);
        X_0_t = offset * targetSurface;

        if (c.has("waypoints"))
        {
          const auto& cw = c("waypoints");
          // Control points offsets defined wrt to the target surface frame
          const auto& controlPoints = cw("controlPoints");

          nrWP = 0; // No automatic waypoint, provide control points
          waypoints.resize(3, controlPoints.size());
          for (unsigned int i = 0; i < controlPoints.size(); ++i)
          {
            const Eigen::Vector3d wp = controlPoints[i];
            sva::PTransformd X_offset(wp);
            waypoints.block(0, i, 3, 1) = (X_offset * targetSurface).translation();
          }
        }
      }
      else if(config.has("target"))
      { // Absolute target pose
        X_0_t = config("target");

        const auto& c = config("waypoints");
        // Control points defined in world coordinates
        const auto& controlPoints = c("controlPoints");
        nrWP = 0; // No automatic waypoint, provide control points
        waypoints.resize(3, controlPoints.size());
        if(config.has("waypoints"))
        {
          waypoints.resize(3, controlPoints.size());
          for (unsigned int i = 0; i < controlPoints.size(); ++i)
          {
            const Eigen::Vector3d wp = controlPoints[i];
            waypoints.block<3, 1>(0, i) = wp;
          }
        }
      }


      auto t = std::make_shared<mc_tasks::TrajectoryTask>(
          solver.robots(),
          robotIndex,
          config("surface"),
          X_0_t,
          config("duration"),
          config("timeStep"),
          config("stiffness"),
          config("posWeight"),
          config("oriWeight"),
          waypoints,
          nrWP
          );
      t->load(solver, config);
      return t;
    });
}
