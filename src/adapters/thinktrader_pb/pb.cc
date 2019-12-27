#include "pb.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <ctime>

#include "opentrade/logger.h"
#include "opentrade/order.h"

void PB::Start() noexcept {
  dir_ = config("dir");
  if (dir_.empty()) {
    LOG_FATAL(name() << ": dir not given");
  }
  channel_ = config("channel");
  if (channel_.empty()) {
    LOG_FATAL(name() << ": channel not given");
  }
  auto interval = config("interval");
  if (!interval.empty()) {
    auto n = 1000 * atof(interval.c_str());
    if (n > 0) interval_ = n;
  }
  LOG_INFO(name() << ": interval=" << interval_ << "ms");
  tp_.AddTask([this]() { Loop(); });
}

inline void PB::LoopAction() {
  std::string order_file = dir_ + "/orderstatus_.csv";
  std::string trade_file = dir_ + "/trade.csv";
  struct stat attr;
  auto now = time(NULL);
  stat(order_file.c_str(), &attr);
  if (attr.st_mtime + 60 < now) {
    connected_ = 0;
    return;
  }
  stat(trade_file.c_str(), &attr);
  if (attr.st_mtime + 60 < now) {
    connected_ = 0;
    return;
  }
  connected_ = 1;
  std::ifstream ifs(order_file.c_str());
  if (!ifs.good()) {
    connected_ = 0;
    return;
  }
  std::string line;
  std::vector<std::string> toks;
  std::getline(ifs, line);
  boost::split(toks, line, boost::is_any_of(",\r"));
  int i_tag = -1;
  int i_cmd_id = -1;
  int istatus_ = -1;
  int i_order_id = -1;
  int i_reason = -1;
  for (auto i = 0u; i < toks.size(); ++i) {
    if (toks[i] == "\xcd\xb6\xd7\xca\xb1\xb8\xd7\xa2") {  // tou zi bei zhu
      i_tag = i;
    } else if (toks[i] ==
               "\xd6\xb8\xc1\xee\xb1\xe0\xba\xc5") {  // zhi ling bian hao
      i_cmd_id = i;
    } else if (toks[i] ==
               "\xce\xaf\xcd\xd0\xd7\xb4\xcc\xac") {  // wei tuo zhuang tai
      istatus_ = i;
    } else if (toks[i] ==
               "\xba\xcf\xcd\xac\xb1\xe0\xba\xc5") {  // he tong bian hao
      i_order_id = i;
    } else if (toks[i] ==
               "\xb7\xcf\xb5\xa5\xd4\xad\xd2\xf2") {  // he tong bian hao
      i_reason = i;
    }
  }
  if (i_tag < 0 || i_cmd_id < 0 || istatus_ < 0 || i_order_id < 0 ||
      i_reason < 0) {
    LOG_ERROR(name() << ": invalid order status file");
    return;
  }

  // to-do: time

  static thread_local std::unordered_map<int64_t, int64_t> kTag2Cmd;
  while (std::getline(ifs, line)) {
    boost::split(toks, line, boost::is_any_of(",\r"));
    if (static_cast<int>(toks.size()) < i_tag + 1) continue;
    if (static_cast<int>(toks.size()) < i_cmd_id + 1) continue;
    if (static_cast<int>(toks.size()) < istatus_ + 1) continue;
    if (static_cast<int>(toks.size()) < i_order_id + 1) continue;
    if (static_cast<int>(toks.size()) < i_reason + 1) continue;
    if (toks[i_tag].empty()) continue;
    std::string reason;
    auto tag = atoll(toks[i_tag].c_str());
    if (tag <= 0) continue;
    kTag2Cmd[tag] = atoll(toks[i_cmd_id].c_str());
    auto status = toks[istatus_];
    auto& status0 = status_[tag];
    if (status0 != status) {
      status0 = status;
      if (status == "\xd2\xd1\xb3\xb7") {  // yi che
        HandleCanceled(tag);
        pending_cancels_.erase(tag);
      } else if (status == "\xd2\xd1\xb1\xa8") {  // yi bao
        HandleNew(tag, toks[i_order_id]);
      } else if (status == "\xb7\xcf\xb5\xa5") {  // fei dan
        HandleNewRejected(tag, toks[i_reason]);
        pending_cancels_.erase(tag);
      } else if (status == "\xd2\xd1\xb3\xc9") {  // yi cheng
        pending_cancels_.erase(tag);
      } else if (status ==
                 "\xd2\xd1\xb1\xa8\xb4\xfd\xb3\xb7") {  // yi bao dai che
        HandlePendingCancel(tag, tag);
        pending_cancels_.erase(tag);
      } else if (status == "\xce\xb4\xb1\xa8") {  // wei bao
        HandlePendingNew(tag);
      }
    }
  }
  ifs.close();
  ifs.open(trade_file.c_str());
  if (!ifs.good()) {
    connected_ = 0;
    return;
  }
  std::getline(ifs, line);
  boost::split(toks, line, boost::is_any_of(",\r"));
  i_tag = -1;
  int i_px = -1;
  int i_qty = -1;
  int i_trade_id = -1;
  i_order_id = -1;
  for (auto i = 0; i < static_cast<int>(toks.size()); ++i) {
    if (toks[i] == "\xcd\xb6\xd7\xca\xb1\xb8\xd7\xa2") {  // tou zi bei zhu
      i_tag = i;
    } else if (toks[i] ==
               "\xb3\xc9\xbd\xbb\xbc\xdb\xb8\xf1") {  // cheng jiao jia ge
      i_px = i;
    } else if (toks[i] ==
               "\xb3\xc9\xbd\xbb\xca\xfd\xc1\xbf") {  // cheng jiao shu liang
      i_qty = i;
    } else if (toks[i] ==
               "\xb3\xc9\xbd\xbb\xb1\xe0\xba\xc5") {  // cheng jiao bian hao
      i_trade_id = i;
    } else if (toks[i] ==
               "\xba\xcf\xcd\xac\xb1\xe0\xba\xc5") {  // he tong bian hao
      i_order_id = i;
    }
  }
  if (i_tag < 0 || i_px < 0 || i_qty < 0 || i_trade_id < 0 || i_order_id < 0) {
    LOG_ERROR(name() << ": invalid trade file");
    return;
  }
  while (std::getline(ifs, line)) {
    boost::split(toks, line, boost::is_any_of(",\r"));
    if (static_cast<int>(toks.size()) < i_tag + 1) continue;
    if (static_cast<int>(toks.size()) < i_px + 1) continue;
    if (static_cast<int>(toks.size()) < i_qty + 1) continue;
    if (static_cast<int>(toks.size()) < i_trade_id + 1) continue;
    if (static_cast<int>(toks.size()) < i_order_id + 1) continue;
    auto trade_id = toks[i_trade_id];
    auto tag = atoll(toks[i_tag].c_str());
    if (tag <= 0) continue;
    if (!trades_readed_.insert(trade_id).second) continue;
    HandleFill(tag, atof(toks[i_qty].c_str()), atof(toks[i_px].c_str()),
               trade_id);
  }
  static const std::string kCmdFile = dir_ + "/command.csv";
  if (std::ifstream(kCmdFile.c_str()).good()) {
    LOG_ERROR(name() << ": command.csv exists");
    return;
  }
  if (pending_cancels_.empty() && orders_.empty()) return;
  static const std::string kStore =
      (opentrade::kStorePath / "command.h").string();
  std::ofstream of(kStore.c_str());
  for (auto& pair : orders_) {
    of << pair.second.mode << ',' << channel_ << ',' << pair.second.px << ','
       << pair.second.symbol << ',' << pair.second.qty << "\r\n";
    of << "orderparam=<tag>note=" << pair.first << "</tag>\r\n";
  }
  for (auto it = pending_cancels_.begin(); it != pending_cancels_.end();) {
    auto it2 = kTag2Cmd.find(*it);
    if (it2 == kTag2Cmd.end()) {
      ++it;
      continue;
    }
    it = pending_cancels_.erase(it);
    of << "cancel " << it2->second << "\r\n";
  }
  of.close();
  if (system(("cp " + kStore + " " + kCmdFile).c_str()) == 0) {
    // do nothing
  }
  orders_.clear();  // for safety, always clear even above failed
}

void PB::Loop() {
  LoopAction();
  tp_.AddTask([this]() { Loop(); }, boost::posix_time::milliseconds(interval_));
}

std::string PB::Cancel(const opentrade::Order& ord) noexcept {
  auto id = ord.id;
  auto orig_id = ord.orig_id;
  tp_.AddTask([this, id, orig_id]() {
    auto it = orders_.find(orig_id);
    if (it != orders_.end()) {
      HandleCanceled(id, orig_id);
      orders_.erase(it);
      return;
    }
    pending_cancels_.insert(orig_id);
  });
  return {};
}

std::string PB::Place(const opentrade::Order& ord) noexcept {
  auto id = ord.id;
  Order exch_order;
  exch_order.qty = ord.qty;
  exch_order.px = ord.price;
  if (exch_order.px <= 0) return "Market order not support";
  if (ord.IsBuy())
    exch_order.mode = "23";
  else
    exch_order.mode = "24";
  exch_order.symbol = ord.sec->symbol;
  tp_.AddTask([this, id, exch_order]() { orders_[id] = exch_order; });
  return {};
}

extern "C" {
opentrade::Adapter* create() { return new PB{}; }
}
