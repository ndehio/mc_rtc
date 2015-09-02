#include <mc_control/mc_driving_controller.h>
#include <mc_robots/polaris_ranger.h>
#include <mc_rbdyn/robot.h>

#include <Tasks/QPContactConstr.h>

#include <RBDyn/FK.h>
#include <RBDyn/FV.h>

#include <mc_rbdyn/Surface.h>

namespace mc_control
{

MCDrivingController::MCDrivingController(const std::vector<std::shared_ptr<mc_rbdyn::RobotModule> >& env_modules)
  : MCMRQPController(env_modules),
    graspOffset(sva::RotX(-M_PI/2), Eigen::Vector3d(0., 0., 0.)),
    ef_task("RARM_LINK6", robots(), 0),
    polarisKinematicsConstraint(robots(), 1, timeStep, false,
        {0.1, 0.01, 0.01}, 0.5),
    drivingContacts(),
    collsConstraint(robots(), 0, 1, timeStep)
{
  mrqpsolver->addConstraintSet(hrp2contactConstraint);
  mrqpsolver->addConstraintSet(hrp2kinematicsConstraint);
  mrqpsolver->addConstraintSet(polarisKinematicsConstraint);
  mrqpsolver->addConstraintSet(hrp2selfCollisionConstraint);
  mrqpsolver->addConstraintSet(collsConstraint);

  mrqpsolver->solver.addTask(hrp2postureTask.get());
  robots().envIndex = 2;

  mc_rbdyn::Robot& polaris = robots().robots[1];

  //robot().mbc->q[0] = {0.8018680589369662, 0.09936561148509283, -0.06541812773434774, 0.5855378381237102, -0.3421374123035909, -0.0002850914593993392, 0.8847053544605464};
  robot().mbc->q[0] = {1, 0, 0, 0, 0, 0, 0.76};

  rbd::forwardKinematics(*(robot().mb), *(robot().mbc));
  rbd::forwardVelocity(*(robot().mb), *(robot().mbc));

  drivingContacts.emplace_back(robots(), robots().robotIndex, 1,
                           "Butthock", "left_seat");
  drivingContacts.emplace_back(robots(), robots().robotIndex, 1,
                           "LFullSole", "exit_platform");
  drivingContacts.emplace_back(robots(), robots().robotIndex, 1,
                           "LeftThight", "left_seat");
  drivingContacts.emplace_back(robots(), robots().robotIndex, 1,
                           "RightThight", "left_seat");
  drivingContacts.emplace_back(robots(), robots().robotIndex, 1,
                           "RightGripper", "bar_wheel");
  //drivingContacts.emplace_back(robots().robotIndex, 1,
  //                         robot().surfaces.at("LowerBack"),
  //                         polaris.surfaces.at("left_back"));
  mrqpsolver->setContacts(drivingContacts);
  //collsConstraint.addCollision(robots(),
  //  mc_solver::Collision("CHEST_LINK1", "seat_back", 0.4, 0.25, 0.0)
  //);

  ef_task.addToSolver(mrqpsolver->solver);
  ef_task.removeFromSolver(mrqpsolver->solver);

  polarisPostureTask = std::shared_ptr<tasks::qp::PostureTask>(new tasks::qp::PostureTask(mrqpsolver->robots.mbs, 1, mrqpsolver->robots.robots[1].mbc->q, 1, 100));

  std::cout << "MCDrivingController init done" << std::endl;
}

bool MCDrivingController::run()
{
  bool success = MCMRQPController::run();
  //std::cout << robots().robots[1].mbc->q[11][0] << std::endl;
  return success;
}

void MCDrivingController::reset(const ControllerResetData & reset_data)
{
  MCMRQPController::reset(reset_data);
  std::cout << "Enter reset" << std::endl;
  mc_rbdyn::Robot& polaris = robots().robots[1];
  robot().mbc->zero(*(robot().mb));
  robot().mbc->q = reset_data.q;
  //robot().mbc->q[0] = {0.8018680589369662, 0.09936561148509283, -0.06541812773434774, 0.5855378381237102, -0.3421374123035909, -0.0002850914593993392, 0.8847053544605464};
  robot().mbc->q[0] = {1, 0, 0, 0, 0, 0, 0.76};
  rbd::forwardKinematics(*(robot().mb), *(robot().mbc));
  rbd::forwardVelocity(*(robot().mb), *(robot().mbc));
  hrp2postureTask->posture(robot().mbc->q);

  resetWheelTransform();
  //resetBasePose();

  /*std::cout << sva::transformError(gripperSurface->X_0_s(robot(), *(robot().mbc)),
                                   wheelSurface->X_0_s(polaris, *(polaris.mbc))) << std::endl;
  */

  mrqpsolver->solver.addTask(polarisPostureTask.get());

  mrqpsolver->setContacts(drivingContacts);

  std::cout << "End reset" << std::endl;
}

void MCDrivingController::resetBasePose()
{
  mc_rbdyn::Robot& polaris = robots().robots[1];
  //Reset freeflyer, compute its position frow wheel and re-set it
  robot().mbc->q[0] = {1., 0., 0., 0., 0., 0., 0.};
  rbd::forwardKinematics(*(robot().mb), *(robot().mbc));
  rbd::forwardVelocity(*(robot().mb), *(robot().mbc));

  int steer_i = polaris.bodyIndexByName("steering_wheel");
  sva::PTransformd X_0_w = polaris.mbc->bodyPosW[steer_i];
  auto gripperSurface = robot().surfaces.at("RightGripper");
  sva::PTransformd X_0_s = gripperSurface->X_0_s(robot(), *(robot().mbc));
  //sva::PTransformd X_0_base = X_0_s.inv()*(graspOffset*X_0_w);
  sva::PTransformd X_0_base = X_0_s.inv()*X_0_w;

  const auto quat = Eigen::Quaterniond(X_0_base.rotation()).inverse();
  const Eigen::Vector3d trans(X_0_base.translation());
  std::vector<double> baseQ = {quat.w(), quat.x(), quat.y(), quat.z(),
                               trans.x(), trans.y(), trans.z()};

  robot().mbc->q[0] = baseQ;
  rbd::forwardKinematics(*(robot().mb), *(robot().mbc));
  rbd::forwardVelocity(*(robot().mb), *(robot().mbc));
}

void MCDrivingController::resetWheelTransform()
{
  mc_rbdyn::Robot& polaris = robots().robots[1];
  //Change wheel position
  int chassis_index = polaris.bodyIndexByName("chassis");
  //Do not take into account potential rotation of steering wheel
  int joint_index = polaris.jointIndexByName("adjust_steering_wheel");

  auto gripperSurface = robot().surfaces.at("RightGripper");
  auto wheelSurface = polaris.surfaces.at("bar_wheel");

  sva::PTransformd X_wheel_s = graspOffset*wheelSurface->X_b_s();

  sva::PTransformd X_0_s = gripperSurface->X_0_s(robot(), *(robot().mbc));

  sva::PTransformd X_0_chassis = polaris.mbc->bodyPosW[chassis_index];

  sva::PTransformd X_chassis_wheel = (X_wheel_s).inv()*X_0_s*(X_0_chassis).inv();

  polaris.mb->transform(joint_index, X_chassis_wheel);
  polaris.mbc->zero(*(polaris.mb));

  rbd::forwardKinematics(*(polaris.mb), *(polaris.mbc));
  rbd::forwardVelocity(*(polaris.mb), *(polaris.mbc));
}

bool MCDrivingController::changeWheelAngle(double theta)
{
  int wheel_i = robots().robots[1].jointIndexByName("steering_joint");
  auto p = polarisPostureTask->posture();
  double old = p[wheel_i][0];
  p[wheel_i][0] = theta;
  polarisPostureTask->posture(p);
  return true;
}

bool MCDrivingController::changeGaze(double pan, double tilt)
{
  int pan_i = robot().jointIndexByName("HEAD_JOINT0");
  int tilt_i = robot().jointIndexByName("HEAD_JOINT1");
  auto p = hrp2postureTask->posture();
  p[pan_i][0] = pan;
  p[tilt_i][0] = tilt;
  hrp2postureTask->posture(p);
  return true;
}

bool MCDrivingController::changeAnkleAngle(double theta)
{
  double tMax = 0.5;
  double tMin = -0.5;
  theta = (tMax-tMin)*theta + tMax;
  int ankle_i = robot().jointIndexByName("RLEG_JOINT4");
  auto p = hrp2postureTask->posture();
  p[ankle_i][0] = theta;
  hrp2postureTask->posture(p);
  return true;
}

bool MCDrivingController::changeWristAngle(double yaw)
{
  int wrist_i = robot().jointIndexByName("RARM_JOINT6");
  auto p = hrp2postureTask->posture();
  p[wrist_i][0] = yaw;
  hrp2postureTask->posture(p);
  return true;
}

bool MCDrivingController::driving_service(double wheel, double ankle, double pan, double tilt)
{
  bool r = true;
  r = r && changeWheelAngle(wheel);
  r = r && changeAnkleAngle(ankle);
  r = r && changeGaze(pan, tilt);
  return r;
}

}