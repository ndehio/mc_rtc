#pragma once

#include <mc_tasks/TrajectoryTaskGeneric.h>

namespace mc_tasks
{

/*! \brief Control the Surface of a body

 * This task is thin wrapper around the appropriate tasks in Tasks.
 *
 */
struct MC_TASKS_DLLAPI SurfaceTransformTask : public TrajectoryTaskGeneric<tasks::qp::SurfaceTransformTask>
{
public:
  /*! \brief Constructor
   *
   * \param bodyName Name of the body to control
   *
   * \param robots Robots controlled by this task
   *
   * \param robotIndex Index of the robot controlled by this task
   *
   * \param stiffness Task stiffness
   *
   * \param weight Task weight
   *
   */
  SurfaceTransformTask(const std::string & bodyName, const mc_rbdyn::Robots
                  & robots, unsigned int robotIndex, double stiffness =
                  2.0, double weight = 500);

  /*! \brief Reset the task
   *
   * Set the task objective to the current body Surface
   */
  virtual void reset() override;

  /*! \brief Get the body Surface target */
  sva::PTransformd target();

  /*! \brief Set the body Surface target
   *
   * \param pos Body Surface in world frame
   *
   */
  void target(const sva::PTransformd & pos);
protected:
  std::string surfaceName;
  unsigned int bIndex;
};

}
