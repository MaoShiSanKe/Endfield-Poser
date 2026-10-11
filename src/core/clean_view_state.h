#pragma once
#include <atomic>

namespace poser_clean_view {
// Session-only. Restarting the game must never start with an invisible UI.
enum Mode { Visible=0, GameOnly=1 };
static std::atomic<int> requested{Visible};
inline void Request(int mode) {
  requested.store(mode==GameOnly?mode:Visible);
}
inline void Toggle() {
  int value=requested.load();
  while(!requested.compare_exchange_weak(value,value==Visible?GameOnly:Visible)){}
}
}
