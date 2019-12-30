#include "../twap/twap.h"

namespace opentrade {

struct Pov : public TWAP {
  std::string OnStart(const ParamMap& params) noexcept override {
    auto err = TWAP::OnStart(params);
    if (!err.empty()) return err;
    if (max_pov_ <= 0) return "MaxPov required";
    return {};
  }

  double GetLeaves() noexcept override {
    auto pov = (inst_->md().trade.volume - initial_volume_) * max_pov_;
    auto n = min_size_;
    if (n <= 0) n = inst_->sec().lot_size;
    return pov + n - inst_->total_exposure();
  }
};

}  // namespace opentrade

extern "C" {
opentrade::Adapter* create() { return new opentrade::Pov{}; }
}
