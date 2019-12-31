#include "3rd/catch.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "opentrade/utility.h"

static inline std::string FacetNow() {
  static std::locale loc(std::cout.getloc(), new boost::posix_time::time_facet(
                                                 "%Y-%m-%d %H:%M:%S.%f"));

  std::stringstream ss;
  ss.imbue(loc);
  ss << boost::posix_time::microsec_clock::universal_time();
  return ss.str();
}

namespace opentrade {

TEST_CASE("Utility", "[Utility]") {
  SECTION("GetNowStr") {
    std::cout << "time_facet " << FacetNow() << " vs " << FacetNow()
              << std::endl;
    std::cout << "GetNowStr  " << GetNowStr<false>() << " vs "
              << GetNowStr<false>() << std::endl;
  }
  SECTION("RollSum") {
    auto tm = GetTime();
    RollSum<double> rs(120);
    rs.Update(0, tm);
    rs.Update(2, tm + 2);
    REQUIRE((rs.GetValue() == 2));
    rs.Update(3, tm + 2);
    REQUIRE((rs.GetValue() == 5));
    rs.Update(3, tm + 122);
    REQUIRE((rs.GetValue() == 8));
    rs.Update(3, tm + 130);
    REQUIRE((rs.GetValue() == 6));
    rs.Update(4, tm + 140);
    REQUIRE((rs.GetValue() == 10));
    rs.Clear();
    REQUIRE((rs.GetValue() == 0));
  }

  SECTION("RollDelta") {
    auto tm = GetTime();
    RollDelta<double> rd(120, 100);
    rd.Update(100, tm);
    REQUIRE((rd.GetValue() == 0));
    rd.Update(102, tm + 2);  // 2
    REQUIRE((rd.GetValue() == 2));
    rd.Update(103, tm + 2);  // 1
    REQUIRE((rd.GetValue() == 3));
    rd.Update(102, tm + 2);  // -1
    REQUIRE((rd.GetValue() == 3));
    rd.Update(103, tm + 10);  // 0
    REQUIRE((rd.GetValue() == 3));
    rd.Update(103, tm + 122);  // 0
    REQUIRE((rd.GetValue() == 3));
    rd.Update(103, tm + 130);  // 0
    REQUIRE((rd.GetValue() == 0));
    rd.Update(104, tm + 140);  // 1
    REQUIRE((rd.GetValue() == 1));
    rd.Clear();
    REQUIRE((rd.GetValue() == 0));
  }
}

}  // namespace opentrade
