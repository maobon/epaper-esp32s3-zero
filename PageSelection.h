#pragma once

#include <stddef.h>
#include <stdint.h>

// SIZE_MAX means no ready page. A non-wrapping search is used by boot preview.
template <typename IsReady>
size_t findReadyPage(size_t start, size_t count, bool wrap, IsReady isReady) {
  if (count == 0 || (!wrap && start >= count)) {
    return SIZE_MAX;
  }
  size_t index = start % count;
  for (size_t visited = 0; visited < count; ++visited) {
    if (isReady(index)) {
      return index;
    }
    if (++index == count) {
      if (!wrap) break;
      index = 0;
    }
  }
  return SIZE_MAX;
}
