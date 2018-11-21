#pragma once

#include <mc_control/fsm/State.h>

namespace mc_control
{

namespace fsm
{

/** Implements parallel states
 *
 * This states plays multiple states at once. Strictly speaking those states
 * are not played in parallel but rather sequentially.
 *
 * If this state plays {state_1, ..., state_N}. The state is completed when all
 * state_i::run() function returns true and its output is state_N output.
 *
 * Configuration entries:
 *
 * - states: list of states run by this state
 * - delays: delay the start of the given state(s) by the given time (in seconds)
 * - configs: for each state in states, configs(state) is used to further
 *   configure state
 *
 * Example:
 *
 * {
 *   "states": [ "StateA", "StateB", "StateC" ], // Play StateA, StateB and StateC
 *   "delays": { "StateA": 1.0 }, // StateA will start one second after StateB and StateC
 *   "configs: { "StateB": {} }
 * }
 *
 */

struct MC_CONTROL_FSM_STATE_DLLAPI ParallelState : State
{
  void configure(const mc_rtc::Configuration & config) override;

  void start(Controller &) override;

  bool run(Controller &) override;

  void stop(Controller &) override;

  void teardown(Controller &) override;

  bool read_msg(std::string & msg) override;

  bool read_write_msg(std::string & msg, std::string & out) override;

protected:
  mc_rtc::Configuration config_;
  double time_ = 0;
  struct DelayedState
  {
    DelayedState(Controller & ctl, const std::string & name, double delay, mc_rtc::Configuration config);
    bool run(Controller & ctl, double time);
    StatePtr & state();

  private:
    StatePtr state_;
    std::string name_;
    mc_rtc::Configuration config_;
    double delay_;
    void createState(Controller & ctl);
  };
  std::vector<DelayedState> states_;
};

} // namespace fsm

} // namespace mc_control