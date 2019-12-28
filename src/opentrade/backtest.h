#ifndef OPENTRADE_BACKTEST_H_
#define OPENTRADE_BACKTEST_H_
#ifdef BACKTEST

#include <boost/date_time/gregorian/gregorian.hpp>
#include <cstdlib>
#include <fstream>
#include <set>

#include "python.h"
#include "security.h"

namespace opentrade {

class Simulator;

class Backtest : public Singleton<Backtest> {
 public:
  Backtest() : of_(PythonOr(std::getenv("TRADES_OUTFILE"), "trades.txt")) {}
  void Play(const boost::gregorian::date& date);
  void Start(const std::string& py, const std::string& default_tick_file,
             int start_date, int end_date);
  SubAccount* CreateSubAccount(const std::string& name,
                               const BrokerAccount* broker = nullptr);
  void OnConfirmation(const Confirmation& cm);
  void End();
  void Clear();
  void Skip() { skip_ = true; }
  void AddSimulator(const std::string& fn_tmpl, const std::string& name = "");
  auto latency() { return latency_; }
  auto start_date() const { return start_date_; }
  auto end_date() const { return end_date_; }

 private:
  bp::object obj_;
  bp::object on_start_;
  bp::object on_start_of_day_;
  bp::object on_confirmation_;
  bp::object on_end_of_day_;
  bp::object on_end_;
  double latency_ = 0;  // in seconds
  double trade_hit_ratio_ = 0.5;
  std::ofstream of_;
  bool skip_ = false;
  std::vector<std::pair<std::string, Simulator*>> simulators_;
  std::set<std::string> used_symbols_;
  bp::object start_date_;
  bp::object end_date_;  // exclusive
};

}  // namespace opentrade

#endif  // BACKTEST
#endif  // OPENTRADE_BACKTEST_H_
