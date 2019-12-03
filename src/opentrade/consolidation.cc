#include "consolidation.h"

namespace opentrade {

static PriceLevel::Quotes kEmptyQuotes;

ConsolidationBook::ConsolidationBook(int num_src) {
  ask_quotes.resize(num_src, kEmptyQuotes.end());
  bid_quotes.resize(num_src, kEmptyQuotes.end());
}

void ConsolidationBook::Reset() {
  Lock lock(m);
  for (auto& p : ask_quotes) p = kEmptyQuotes.end();
  for (auto& p : bid_quotes) p = kEmptyQuotes.end();
  asks.clear();
  bids.clear();
}

template <bool reset, typename A>
inline void ConsolidationBook::Erase(PriceLevel::Quotes::const_iterator it,
                                     A* a) {
  if constexpr (reset) {
    if constexpr (A::IsAsk())
      ask_quotes[it->inst->src_idx()] = kEmptyQuotes.end();
    else
      bid_quotes[it->inst->src_idx()] = kEmptyQuotes.end();
  }
  auto level = it->parent;
  level->size -= it->size;
  level->quotes.erase(it);
  if (level->quotes.empty()) a->erase(level->self);
}

template <typename A, typename B>
inline void ConsolidationBook::Insert(double price, MarketData::Qty size,
                                      const Instrument* inst, A* a, B* b) {
  auto p = a->emplace(price);
  auto& level = const_cast<PriceLevel&>(*p.first);
  if (p.second) level.self = p.first;
  if constexpr (A::IsAsk())
    ask_quotes[inst->src_idx()] = level.Insert(inst, size);
  else
    bid_quotes[inst->src_idx()] = level.Insert(inst, size);
  // remove crossed levels of b, lock allowed
  for (auto it = b->begin(); it != b->end() && A::kPriceCmp(price, it->price);
       ++it) {
    while (!it->quotes.empty()) Erase<true>(it->quotes.begin(), b);
  }
}

template <typename A, typename B>
inline void ConsolidationBook::Update(double price, MarketData::Qty size,
                                      const Instrument* inst, A* a, B* b) {
  PriceLevel::Quotes::iterator it;
  if constexpr (A::IsAsk())
    it = ask_quotes[inst->src_idx()];
  else
    it = bid_quotes[inst->src_idx()];
  Lock lock(m);
  if (it == kEmptyQuotes.end()) {
    if (price > 0) Insert(price, size, inst, a, b);
  } else {
    if (price > 0) {
      if (price != it->parent->price) {
        Erase<false>(it, a);
        Insert(price, size, inst, a, b);
      }
    } else {
      Erase<true>(it, a);
    }
  }
}

void ConsolidationHandler::Start() noexcept {
  MarketDataManager::Instance().AddAdapter(
      new DummyFeed(kConsolidationSrc.str()));
}

void ConsolidationHandler::Subscribe(Instrument* inst, bool listen) noexcept {
  assert(listen == false);  // listen not implmented yet
  assert(kConsolidationSrc == inst->src());
  Async([=]() {
    auto book = const_cast<Ind*>(inst->Get<Ind>());
    if (!book) {
      book = new Ind{};
      book->ask_quotes.resize(MarketDataManager::Instance().adapters().size(),
                              kEmptyQuotes.end());
      book->bid_quotes.resize(MarketDataManager::Instance().adapters().size(),
                              kEmptyQuotes.end());
      const_cast<MarketData&>(inst->md()).Set(book);
      for (auto& p : MarketDataManager::Instance().adapters()) {
        if (kConsolidationSrc == p.second->src()) continue;
        Algo::Subscribe(inst->sec(), DataSrc(p.second->src()), true, inst);
      }
    }
    if (listen) book->AddListener(inst);
  });
}

void ConsolidationHandler::OnMarketQuote(const Instrument& inst,
                                         const MarketData& md,
                                         const MarketData& md0) noexcept {
  assert(inst.src_idx() < MarketDataManager::Instance().adapters().size());
  assert(inst.parent());
  auto book = const_cast<Ind*>(inst.parent()->Get<Ind>());
  auto& q0 = md0.quote();
  auto& q = md.quote();
  if (q.ask_price != q0.ask_price)
    book->Update(q.ask_price, q.ask_size, &inst, &book->asks, &book->bids);
  if (q.bid_price != q0.bid_price)
    book->Update(q.bid_price, q.bid_size, &inst, &book->bids, &book->asks);
}

}  // namespace opentrade
