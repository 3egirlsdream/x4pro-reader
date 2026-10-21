#pragma once
struct FakeFrontlight {
  bool present() const { return true; }
};
inline FakeFrontlight Frontlight;
