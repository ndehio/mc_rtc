#ifndef _H_FORCECONTACTSENSOR_H_
#define _H_FORCECONTACTSENSOR_H_

#include <mc_control/ContactSensor.h>
#include <boost/circular_buffer.hpp>

namespace mc_control
{

struct Threshold
{
public:
  Threshold();
  Threshold(double t, unsigned int i);

  double thresh;
  unsigned int iter;
};

struct ForceSensor
{
public:
  enum SensorConfig
  {
    Unactivated = 0,
    Activated = 1,
    Forward = 2,
    Backward = 3,
    WindowSize = 100
  };
public:
  ForceSensor(const mc_rbdyn::Robot & robot, const std::string & sensorName, const Threshold & thresh);

  void computeOffset();

  void update(const Eigen::Vector3d & force);
public:
  SensorConfig activated;
  Threshold thresh;
  unsigned int activatedIter;
  SensorConfig direction;
  boost::circular_buffer<double> lastValues;
  double offset;
  std::vector<std::string> surfacesName;
};

struct ForceContactSensor : public ContactSensor
{
public:
  ForceContactSensor(const mc_rbdyn::Robot & robot);

  virtual void resetOffset() override;

  virtual std::vector<std::string> update(MCController & ctl) override;
public:
  std::vector<ForceSensor> sensors;
};

}

#endif