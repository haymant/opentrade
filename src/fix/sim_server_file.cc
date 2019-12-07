#include "sim_server.h"

#include "opentrade/market_data.h"

struct SimServerFile : public opentrade::MarketDataAdapter, public SimServer {
  void Start() noexcept override;
  void Stop() noexcept override {}
  void SubscribeSync(const Security& sec) noexcept override {}
};

void SimServerFile::Start() noexcept {
  auto ticks_file = config("ticks_file");
  if (ticks_file.empty()) {
    LOG_FATAL(name() << ": ticks_file not given");
  }

  opentrade::PipeStream ifs(ticks_file.c_str());
  if (!ifs.good()) {
    LOG_FATAL(name() << ": Can not open " << ticks_file);
  }
  bool binary;
  auto secs = opentrade::SecurityManager::Instance().GetSecurities(
      &ifs.stream(), ticks_file.c_str(), &binary);
  assert(!binary);

  StartFix(*this);
  connected_ = 1;

  std::thread thread([=, secs{std::move(secs)}]() {
    while (true) {
      struct tm tm;
      auto t = time(nullptr);
      gmtime_r(&t, &tm);
      auto n = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;
      auto t0 = t - n;
      std::string line;
      LOG_DEBUG(name() << ": Start to play back");
      auto skip = 0l;
      opentrade::PipeStream ifs(ticks_file.c_str());
      for (auto i = 0u; i < secs.size() + 2; ++i) {
        std::getline(ifs.stream(), line);
      }
      while (std::getline(ifs.stream(), line)) {
        if (skip-- > 0) continue;
        char hms_str[24];
        uint32_t i;
        char type;
        double px;
        double qty;
        if (sscanf(line.c_str(), "%s %u %c %lf %lf", hms_str, &i, &type, &px,
                   &qty) != 5)
          continue;
        auto hms = atol(hms_str);
        if (strlen(hms_str) > 6) hms /= 1000;
        if (i >= secs.size()) continue;
        t = t0 + hms / 10000 * 3600 + hms % 10000 / 100 * 60 + hms % 100;
        auto now = time(nullptr);
        if (t < now - 3) {
          skip = 1000;
          continue;
        }
        if (now < t) {
          LOG_DEBUG(name() << ": " << hms);
          std::this_thread::sleep_for(std::chrono::seconds(t - now));
        }
        auto sec = secs[i];
        if (!sec) continue;
        switch (type) {
          case 'T':
            Update(sec->id, px, qty);
            break;
          case 'A':
            if (*sec->exchange->name == 'U') qty *= 100;
            Update(sec->id, px, qty, false);
            if (!qty && sec->type == opentrade::kForexPair) qty = 1e9;
            break;
          case 'B':
            if (*sec->exchange->name == 'U') qty *= 100;
            Update(sec->id, px, qty, true);
            if (!qty && sec->type == opentrade::kForexPair) qty = 1e9;
            break;
          default:
            continue;
        }
        HandleTick(sec->id, type, px, qty);
      }
      for (auto& pair : md()) pair.second = opentrade::MarketData{};
    }
  });
  thread.detach();
}

extern "C" {
opentrade::Adapter* create() { return new SimServerFile{}; }
}
