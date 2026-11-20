// Shared helpers for the tensorstore fuzz harnesses (mayhem/ layer).
#ifndef MAYHEM_FUZZ_COMMON_H_
#define MAYHEM_FUZZ_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/cord.h"
#include <nlohmann/json.hpp>
#include "tensorstore/context.h"
#include "tensorstore/index.h"
#include "tensorstore/index_space/dim_expression.h"
#include "tensorstore/kvstore/kvstore.h"
#include "tensorstore/kvstore/operations.h"
#include "tensorstore/open.h"
#include "tensorstore/tensorstore.h"

namespace mayhem_fuzz {

// Input container: repeated [u8 name_len][name][u32le data_len][data]; a
// truncated length is clamped to the bytes left.
inline std::vector<std::pair<std::string, std::string>> ParseContainer(
    const uint8_t* data, size_t size) {
  std::vector<std::pair<std::string, std::string>> out;
  size_t p = 0;
  while (p < size && out.size() < 64) {
    size_t nlen = data[p++];
    if (nlen > size - p) nlen = size - p;
    std::string name(reinterpret_cast<const char*>(data + p), nlen);
    p += nlen;
    uint32_t dlen = 0;
    for (int i = 0; i < 4 && p < size; ++i) dlen |= uint32_t(data[p++]) << (8 * i);
    if (dlen > size - p) dlen = size - p;
    std::string value(reinterpret_cast<const char*>(data + p), dlen);
    p += dlen;
    out.emplace_back(std::move(name), std::move(value));
  }
  return out;
}

// Reads at most kMaxElements elements from the start of the store's domain.
// The cap only bounds the harness's own output allocation (metadata may
// declare an arbitrarily large domain).
template <typename Store>
void ReadBounded(const Store& store) {
  constexpr tensorstore::Index kMaxElements = 1 << 18;
  auto domain = store.domain();
  const auto rank = domain.rank();
  std::vector<tensorstore::Index> start(rank), stop(rank);
  tensorstore::Index budget = kMaxElements;
  for (tensorstore::DimensionIndex i = 0; i < rank; ++i) {
    auto iv = domain[i].interval();
    if (!tensorstore::IsFinite(iv)) return;
    tensorstore::Index n = iv.size();
    if (n > budget) n = budget;
    if (n < 1) n = (iv.size() == 0 ? 0 : 1);
    start[i] = iv.inclusive_min();
    stop[i] = iv.inclusive_min() + n;
    if (n > 0) budget /= n;
    if (budget < 1) budget = 1;
  }
  auto view = store | tensorstore::AllDims().HalfOpenInterval(start, stop);
  if (!view.ok()) return;
  auto arr = tensorstore::Read(*view).result();
  (void)arr;
}

}  // namespace mayhem_fuzz

#endif  // MAYHEM_FUZZ_COMMON_H_
