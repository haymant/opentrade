#include "../twap/twap.h"

namespace opentrade {

struct Peg : public TWAP {
  const ParamDefs& GetParamDefs() noexcept override { return kCommonParamDefs; }

  std::string OnStart(const ParamMap& params) noexcept override {
    auto err = TWAP::OnStart(params);
    if (!err.empty()) return err;
    if (max_floor_ <= 0) return "MaxFloor required";
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
