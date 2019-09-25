/*
 * Copyright 2015-2019 CNRS-UM LIRMM, CNRS-AIST JRL
 */

#pragma once

#include <mc_control/Configuration.h>
#include <mc_control/generic_gripper.h>
#include <mc_observers/ObserverLoader.h>
#include <mc_rbdyn/Robots.h>
#include <mc_rtc/GUIState.h>
#include <mc_rtc/log/Logger.h>
#include <mc_solver/CollisionsConstraint.h>
#include <mc_solver/ContactConstraint.h>
#include <mc_solver/DynamicsConstraint.h>
#include <mc_solver/KinematicsConstraint.h>
#include <mc_solver/QPSolver.h>
#include <mc_solver/msg/QPResult.h>
#include <mc_tasks/PostureTask.h>

namespace mc_rbdyn
{
struct Contact;
}

#include <mc_control/api.h>

namespace mc_control
{

/** \class ControllerResetData
 * \brief Contains information allowing the controller to start smoothly from
 * the current state of the robot
 * \note
 * For now, this only contains the state of the robot (free flyer and joints state)
 */
struct MC_CONTROL_DLLAPI ControllerResetData
{
  /** Contains free flyer + joints state information */
  const std::vector<std::vector<double>> & q;
};

struct MCGlobalController;

/** \class MCController
 * \brief MCController is the base class to implement all controllers. It
 * assumes that at least two robots are provided. The first is considered as the
 * "main" robot. Some common constraints and a posture task are defined (but not
 * added to the solver) for this robot
 */
struct MC_CONTROL_DLLAPI MCController
{
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  friend struct MCGlobalController;

public:
  virtual ~MCController();
  /** This function is called at each time step of the process driving the robot
   * (i.e. simulation or robot's controller). This function is the most likely
   * to be overriden for complex controller behaviours.
   * \return True if the solver succeeded, false otherwise
   * \note
   * This is meant to run in real-time hence some precaution should apply (e.g.
   * no i/o blocking calls, no thread instantiation and such)
   *
   * \note
   * The default implementation does the bare minimum (i.e. call run on QPSolver)
   * It is recommended to use it in your override.
   */
  virtual bool run();

  /** This function is called before the run() funciton at each time step of the process
   * driving the robot (i.e. simulation or robot's controller). The default
   * behaviour is to call the run() function of each loaded observer and update
   * the realRobot instance from the desired list of estimators.
   *
   * This is meant to run in real-time hence some precaution should apply (e.g.
   * no i/o blocking calls, no thread instantiation and such)
   *
   * \note Some estimators are likely to require extra information (such as
   * contact location for the KinematicInertial-observers) etc. To provide it,
   * override this method in your controller, call your observer-specific code,
   * then, if the default behaviour suits you, call the default MCController::runObservers().
   *
   * Example
   * \code{.cpp}
   * MyController::runObservers()
   * {
   *   // The kinematic inertial observer requires an anchor frame on the floor,
   *   somewhere in-between the robot feet. For a walking controller, it is
   *   advised to smoothly change the anchor frame in-between the feet, to avoid
   *   state jumps when switching to the next single-support phase.
   *   auto observer = static_pointer_cast<KinematicInertialObserver>(observers["KinematicInertial"]));
   *   observer->leftFootRatio(percentageOfDoubleSupportTime);
   *   MCController::runObservers();
   * }
   * \endcode
   *
   * @returns true if all observers ran as expected, false otherwise
   */
  virtual bool runObservers();

  /**
   * WARNING EXPERIMENTAL
   * Runs the QP on real_robot state
   * ONLY SUPPORTS ONE ROBOT FOR NOW
   */
  virtual bool runClosedLoop();

  /** Can be called in derived class instead of run to use a feedback strategy
   * different from the default one
   *
   * \param fType Type of feedback used in the solver
   *
   */
  bool run(mc_solver::FeedbackType fType);

  /** Gives access to the result of the QP execution
   * \param t Unused at the moment
   */
  virtual const mc_solver::QPResultMsg & send(const double & t);

  /** Reset the controller with data provided by ControllerResetData. This is
   * called at two possible points during a simulation/live execution:
   *   1. Actual start
   *   2. Switch from a previous (MCController-like) controller
   * In the first case, the data comes from the simulation/controller. In the
   * second case, the data comes from the previous MCController instance.
   * \param reset_data Contains information allowing to reset the controller
   * properly
   * \note
   * The default implementation reset the main robot's state to that provided by
   * reset_data (with a null speed/acceleration). It maintains the contacts as
   * they were set by the controller previously.
   */
  virtual void reset(const ControllerResetData & reset_data);

  /** Return the main robot (first robot provided in the constructor
   * \anchor mc_controller_robot_const_doc
   */
  virtual const mc_rbdyn::Robot & robot() const;

  /** Return the env "robot"
   * \note
   * In multi-robot scenarios, the env robot is either:
   *   1. The first robot with zero dof
   *   2. The last robot provided at construction
   * \anchor mc_controller_env_const_doc
   */
  virtual const mc_rbdyn::Robot & env() const;

  /** Return the mc_rbdyn::Robots controlled by this controller
   * \anchor mc_controller_robots_const_doc
   */
  virtual const mc_rbdyn::Robots & robots() const;

  /** Non-const variant of \ref mc_controller_robots_const_doc "robots()" */
  virtual mc_rbdyn::Robots & robots();

  /** Non-const variant of \ref mc_controller_robot_const_doc "robot()" */
  virtual mc_rbdyn::Robot & robot();

  /** Non-const variant of \ref mc_controller_env_const_doc "env()" */
  virtual mc_rbdyn::Robot & env();

  /** Return the mc_solver::QPSolver instance attached to this controller
   * \anchor mc_controller_qpsolver_const_doc
   */
  const mc_solver::QPSolver & solver() const;

  /** Non-const variant of \ref mc_controller_qpsolver_const_doc "solver()" */
  mc_solver::QPSolver & solver();

  /** Set a joint position to the desired value
   * \param jname Name of the joint to control
   * \param pos Desired position (radians)
   * \return True if jname is valid, false otherwise
   * \note
   * No control is made on the value of pos to ensure it is within the joints'
   * limits of jname. It is assumed that this is done via the controller own
   * constraint set
   *
   * \note
   * The default implementation only works on the main robot.
   */
  virtual bool set_joint_pos(const std::string & jname, const double & pos);

  /** Get a joint position
   * \param jname Name of the desired joint
   * \param position position of jname joint (radians)
   * \return True if jname is valid, false otherwise
   * \note
   * Due to the overhead of checks on joint validity, it is not recommended
   * to use this function to repeatedly access a specific joint value.
   *
   * \note
   * The default implementation only works on the main robot.
   */
  bool get_joint_pos(const std::string & jname, double & pos);

  /** Change the currently controlled end-effector
   * \param name End of the name effector
   * \return False if the controller does not implement this kind of control or
   * if name is not a end-effector, true otherwise
   */
  virtual bool change_ef(const std::string & name);

  /** Move the currently controlled end-effector
   * \param t Translation amount
   * \param m Rotation applied to current orientation
   * \return False if the controller does not implement this control, true
   * otherwise
   */
  virtual bool move_ef(const Eigen::Vector3d & t, const Eigen::Matrix3d & m);

  /** Move the CoM
   * \param t CoM translation
   * \return False if the controller does not implement this control, true
   */
  virtual bool move_com(const Eigen::Vector3d & t);

  /** Trigger next step in a FSM controller
   * \return False if the controller does not implement this or if the switch
   * cannot happen, true otherwise
   */
  virtual bool play_next_stance();

  /** Driving service
   * \param wheel Wheel rotation angle
   * \param ankle Ankle angle (related to acceleration)
   * \param pan Head pan angle
   * \param tilt Head tilt angle
   * \return False if the controller does not implement this, true otherwise
   */
  virtual bool driving_service(double wheel, double ankle, double pan, double tilt);

  /** Generic message passing interface, write-only version
   * \param msg A message passed to the controller
   * \return True if the controller was able to do something out of msg, false
   * otherwise
   * \note
   * The default implementation does nothing and always return false.
   */
  virtual bool read_msg(std::string & msg);

  /** Generic message passing interface, read/write version
   * \param msg A message passed to the controller
   * \param out A message passed back to the caller
   * \return True if the controller was able to do something out of msg, false
   * otherwise
   * \note
   * The default implementation does nothing and always return false.
   */
  virtual bool read_write_msg(std::string & msg, std::string & out);

  /** Returns mc_rtc::Logger instance */
  mc_rtc::Logger & logger();

  /** Returns mc_rtc::gui::StateBuilder ptr */
  std::shared_ptr<mc_rtc::gui::StateBuilder> gui()
  {
    return gui_;
  }

  /** Access real robots data */
  const mc_rbdyn::Robots & realRobots() const;

  /** Returns a list of robots supported by the controller.
   * \return Vector of supported robots designed by name (as returned by
   * RobotModule::name())
   * \note
   * Default implementation returns an empty list which indicates that all
   * robots are supported.
   */
  virtual std::vector<std::string> supported_robots() const;

protected:
  /** Builds a controller base with an empty environment
   * \param robot Pointer to the main RobotModule
   * \param dt Controller timestep
   * your controller
   */
  MCController(std::shared_ptr<mc_rbdyn::RobotModule> robot, double dt);

  /** Builds a multi-robot controller base
   * \param robots Collection of robot modules used by the controller
   * \param dt Timestep of the controller
   */
  MCController(const std::vector<std::shared_ptr<mc_rbdyn::RobotModule>> & robot_modules, double dt);

protected:
  /** Load an additional robot into the controller
   *
   * \param rm RobotModule used to load the robot
   *
   * \param name Name of the robot
   *
   * \returns The loaded robot
   */
  mc_rbdyn::Robot & loadRobot(mc_rbdyn::RobotModulePtr rm, const std::string & name);

protected:
  /** QP solver */
  std::shared_ptr<mc_solver::QPSolver> qpsolver;
  /** Real robots provided by MCGlobalController, nullptr until ::reset */
  std::shared_ptr<mc_rbdyn::Robots> real_robots;

  /** Observers provided by MCGlobalController */
  std::map<std::string, std::shared_ptr<mc_observers::Observer>> observers;
  /** Observers order provided by MCGlobalController
   * Observers will be run and update real robot in that order
   **/
  std::vector<std::string> observersOrder;
  /** Observers that will be updating the realRobot provided by
   * MCGlobalController */
  std::vector<std::string> updateObservers;
  /** Logger provided by MCGlobalController */
  std::shared_ptr<mc_rtc::Logger> logger_;
  /** GUI state builder */
  std::shared_ptr<mc_rtc::gui::StateBuilder> gui_;

public:
  /** Controller timestep */
  const double timeStep;
  /** Grippers */
  std::map<std::string, std::shared_ptr<mc_control::Gripper>> grippers;
  /** Contact constraint for the main robot */
  mc_solver::ContactConstraint contactConstraint;
  /** Dynamics constraints for the main robot */
  mc_solver::DynamicsConstraint dynamicsConstraint;
  /** Kinematics constraints for the main robot */
  mc_solver::KinematicsConstraint kinematicsConstraint;
  /** Self collisions constraint for the main robot */
  mc_solver::CollisionsConstraint selfCollisionConstraint;
  /** Posture task for the main robot */
  std::shared_ptr<mc_tasks::PostureTask> postureTask;
};

} // namespace mc_control

#ifdef WIN32
#  define CONTROLLER_MODULE_API __declspec(dllexport)
#else
#  if __GNUC__ >= 4
#    define CONTROLLER_MODULE_API __attribute__((visibility("default")))
#  else
#    define CONTROLLER_MODULE_API
#  endif
#endif

/** Provides a handle to construct the controller with Json config */
#define CONTROLLER_CONSTRUCTOR(NAME, TYPE)                                                                        \
  extern "C"                                                                                                      \
  {                                                                                                               \
    CONTROLLER_MODULE_API void MC_RTC_CONTROLLER(std::vector<std::string> & names)                                \
    {                                                                                                             \
      names = {NAME};                                                                                             \
    }                                                                                                             \
    CONTROLLER_MODULE_API void destroy(mc_control::MCController * ptr)                                            \
    {                                                                                                             \
      delete ptr;                                                                                                 \
    }                                                                                                             \
    CONTROLLER_MODULE_API mc_control::MCController * create(const std::string &,                                  \
                                                            const std::shared_ptr<mc_rbdyn::RobotModule> & robot, \
                                                            const double & dt,                                    \
                                                            const mc_control::Configuration & conf)               \
    {                                                                                                             \
      return new TYPE(robot, dt, conf);                                                                           \
    }                                                                                                             \
  }

/** Provides a handle to construct a generic controller */
#define SIMPLE_CONTROLLER_CONSTRUCTOR(NAME, TYPE)                                                                 \
  extern "C"                                                                                                      \
  {                                                                                                               \
    CONTROLLER_MODULE_API void MC_RTC_CONTROLLER(std::vector<std::string> & names)                                \
    {                                                                                                             \
      names = {NAME};                                                                                             \
    }                                                                                                             \
    CONTROLLER_MODULE_API void destroy(mc_control::MCController * ptr)                                            \
    {                                                                                                             \
      delete ptr;                                                                                                 \
    }                                                                                                             \
    CONTROLLER_MODULE_API mc_control::MCController * create(const std::string &,                                  \
                                                            const std::shared_ptr<mc_rbdyn::RobotModule> & robot, \
                                                            const double & dt,                                    \
                                                            const mc_control::Configuration &)                    \
    {                                                                                                             \
      return new TYPE(robot, dt);                                                                                 \
    }                                                                                                             \
  }
