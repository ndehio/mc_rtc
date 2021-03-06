/*
 * Copyright 2020 CNRS-UM LIRMM, CNRS-AIST JRL
 */

#include "mc_admittance_sample_controller.h"

AdmittanceSampleController::AdmittanceSampleController(mc_rbdyn::RobotModulePtr rm,
                                                       double dt,
                                                       const mc_rtc::Configuration & config)
: mc_control::fsm::Controller(rm, dt, config)
{
}

void AdmittanceSampleController::reset(const mc_control::ControllerResetData & reset_data)
{
  Controller::reset(reset_data);
  // Update anchor frame for the KinematicInertial observer
  anchorFrame(sva::interpolate(robot().surfacePose("RightFoot"), robot().surfacePose("LeftFoot"), 0.5));
  anchorFrameReal(sva::interpolate(realRobot().surfacePose("RightFoot"), realRobot().surfacePose("LeftFoot"), 0.5));

  auto handForceConfig = mc_rtc::gui::ForceConfig(mc_rtc::gui::Color(0., 1., 0.));
  handForceConfig.force_scale *= 10;
  gui()->addElement(
      {"Forces"},
      mc_rtc::gui::Force("RightGripper", handForceConfig,
                         [this]() { return robot().forceSensor("RightHandForceSensor").worldWrench(robot()); },
                         [this]() { return robot().surface("RightGripper").X_0_s(robot()); }));

  using Color = mc_rtc::gui::Color;
  gui()->addPlot(
      "Wrenches without gravity (t)", mc_rtc::gui::plot::X({"t", {t_ + 0, t_ + 10}}, [this]() { return t_; }),
      mc_rtc::gui::plot::Y("Wrenches(z)", [this]() { return robot().surfaceWrench("RightGripper").force().z(); },
                           Color::Blue));
}

bool AdmittanceSampleController::run()
{
  t_ += timeStep;
  // Update anchor frame for the KinematicInertial observer
  anchorFrame(sva::interpolate(robot().surfacePose("RightFoot"), robot().surfacePose("LeftFoot"), 0.5));
  anchorFrameReal(sva::interpolate(realRobot().surfacePose("RightFoot"), realRobot().surfacePose("LeftFoot"), 0.5));
  return Controller::run();
}
