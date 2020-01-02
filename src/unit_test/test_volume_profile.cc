#include "3rd/catch.hpp"

#include "algos/vwap/volume_profile.h"

namespace opentrade {

TEST_CASE("VolumeProfile", "[VolumeProfile]") {
  static const char* kVpFile = "test_vp.txt";
  std::ofstream of(kVpFile);
  of << "1 12:00 100\n";
  of << "1 12:01 200\n";
  of << "1 12:02 100\n";
  of << "1 12:03 200\n";
  of << "1 12:10 100\n";
  of << "1 12:06 100\n";
  of.close();
  VolumeProfile vp(kVpFile);

  auto p = vp.Get(1, 11 * 60, 12 * 60 - 1);
  REQUIRE(p.empty());
  p = std::move(vp.Get(1, 11 * 60, 12 * 60 + 5));
  REQUIRE(p.size() == 64);
  REQUIRE(p[0] == 0);
  REQUIRE(p[60] == 1.f / 6);
  p = std::move(vp.Get(1, 12 * 60, 12 * 60 + 6));
  REQUIRE(p.size() == 7);
  REQUIRE(p[2] == 4.f / 7);
  REQUIRE(p[3] == 6.f / 7);
  REQUIRE(p[4] == 6.f / 7);
  REQUIRE(p[5] == 6.f / 7);
  REQUIRE(p[6] == 1);
  p = std::move(vp.Get(1, 12 * 60 + 10, 12 * 60 + 6));
  REQUIRE(p.empty());
  p = std::move(vp.Get(1, 12 * 60 + 10, 12 * 60 + 12));
  REQUIRE(p.size() == 1);
  REQUIRE(p[0] == 1);
  p = std::move(vp.Get(2, 12 * 60 + 10, 12 * 60 + 12));
  REQUIRE(p.empty());
  p = std::move(vp.Get(1, 12 * 60 + 4, 12 * 60 + 12));
  REQUIRE(p.size() == 7);
  REQUIRE(p[0] == 0);
  REQUIRE(p[6] == 1);
}

}  // namespace opentrade
