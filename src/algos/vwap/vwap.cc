#include "../twap/twap.h"
#include "volume_profile.h"

namespace opentrade {

struct VWAP : public TWAP {
  std::string OnStart(const ParamMap& params) noexcept override {
    auto err = TWAP::OnStart(params);
    if (!err.empty()) return err;
    auto sec = st_.sec;
    profile_ = std::move(kProfiles_.Get(
        sec->id, sec->exchange->GetSeconds(start_time_) / 60,
        std::round(sec->exchange->GetSeconds(end_time_) / 60.) - 1));
    return {};
  }

  double GetLeaves() noexcept override {
    if (profile_.empty()) return TWAP::GetLeaves();
    auto i = std::min(
        static_cast<int64_t>(profile_.size() - 1),
        std::max(0l, static_cast<int64_t>((GetTime() - start_time_) / 60)));
    return st_.qty * profile_[i] - inst_->total_exposure();
  }

  void OnStop() noexcept override {
    TWAP::OnStop();
    decltype(profile_)().swap(profile_);
  }

 private:
  VolumeProfile::Profile profile_;
  static inline VolumeProfile kProfiles_{
      PythonOr(std::getenv("VOLUME_PROFILE_FILE"), "volume_profile.txt")};
};

}  // namespace opentrade

extern "C" {
opentrade::Adapter* create() { return new opentrade::VWAP{}; }
}
