#ifndef ALGOS_VWAP_VOLUME_PROFILE_H_
#define ALGOS_VWAP_VOLUME_PROFILE_H_

#include "opentrade/logger.h"
#include "opentrade/security.h"

namespace opentrade {

struct VolumeProfile {
  explicit VolumeProfile(const char* file) {
    PipeStream str(file);
    if (!str.good()) {
      LOG_ERROR("Failed to read volume profile file: " << file);
      return;
    }
    static const int kLineLength = 128;
    char line[kLineLength];
    while (str.stream().getline(line, sizeof(line))) {
      uint32_t id;
      int hour;
      int minute;
      float volume;
      if (sscanf(line, "%u %d:%d %f", &id, &hour, &minute, &volume) != 4) {
        LOG_ERROR("Invalid volume profile file: " << file);
        profiles_.clear();
        break;
      }
      profiles_[id].emplace_back(hour * 60 + minute, volume);
    }
    for (auto& pair : profiles_)
      std::sort(pair.second.begin(), pair.second.end());
  }

  struct Volume {
    Volume(int m, float v) : minute_time(m), value(v) {}
    bool operator<(int time) const { return minute_time < time; }
    bool operator<(const Volume& b) const {
      return minute_time < b.minute_time;
    }
    int minute_time;  // minutes since midnight
    float value;
  };
  typedef std::vector<float> Profile;

  // [start, end]
  Profile Get(Security::IdType id, int start, int end) const {
    if (start >= end) return {};
    auto it = profiles_.find(id);
    if (it == profiles_.end()) return {};
    auto& p = it->second;
    auto it_begin = std::lower_bound(p.begin(), p.end(), start);  // first >=
    auto it_end = std::lower_bound(p.begin(), p.end(), end);
    if (it_end != p.end() && it_end->minute_time == end) it_end++;
    if (it_begin == it_end) return {};
    std::vector<float> res((it_end - 1)->minute_time - start + 1);
    auto sum = 0.;
    for (auto it = it_begin; it != it_end; ++it) sum += it->value;
    auto tmp = 0.;
    for (auto it = it_begin; it != it_end; ++it) {
      tmp += it->value;
      res[it->minute_time - start] = tmp / sum;
    }
    for (auto i = 1u; i < res.size(); ++i) {
      auto& b = res[i];
      auto a = res[i - 1];
      if (b < a) b = a;
    }
    return res;
  }

 private:
  std::unordered_map<Security::IdType, std::vector<Volume>> profiles_;
};

}  // namespace opentrade

#endif  // ALGOS_VWAP_VOLUME_PROFILE_H_
