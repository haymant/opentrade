#include "../twap/twap.h"

namespace opentrade {

struct Peg : public TWAP {
  std::string OnStart(const ParamMap& params) noexcept override {
    auto err = TWAP::OnStart(params);
    if (!err.empty()) return err;
    if (max_pov_ <= 0 || max_floor_ <= 0) return "MaxPov or MaxFloor required";
    return {};
  }

  double GetLeaves() noexcept override {
    return st_.qty - inst_->total_exposure();
  }
};

}  // namespace opentrade

extern "C" {
opentrade::Adapter* create() { return new opentrade::Peg{}; }
}
