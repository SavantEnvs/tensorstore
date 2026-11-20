// Fuzzes the zip kvstore (EOCD / zip64 / central directory / local headers and
// the deflate/bzip2/zstd/xz readers): the input is a zip file in a fresh
// in-memory kvstore; kvstore::Open {driver: zip} + List + Read of every key.
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto context = tensorstore::Context::Default();
  auto mem = tensorstore::kvstore::Open({{"driver", "memory"}}, context).result();
  if (!mem.ok()) return 0;
  (void)tensorstore::kvstore::Write(
      *mem, "f", absl::Cord(std::string_view(reinterpret_cast<const char*>(data), size)))
      .result();
  auto kv = tensorstore::kvstore::Open(
                {{"driver", "zip"}, {"base", {{"driver", "memory"}, {"path", "f"}}}},
                context)
                .result();
  if (!kv.ok()) return 0;
  auto list = tensorstore::kvstore::ListFuture(*kv).result();
  if (!list.ok()) return 0;
  int n = 0;
  for (auto& e : *list) {
    if (++n > 64) break;
    (void)tensorstore::kvstore::Read(*kv, e.key).result();
    tensorstore::kvstore::ReadOptions opts;
    opts.byte_range = tensorstore::OptionalByteRangeRequest::Range(1, 9);
    (void)tensorstore::kvstore::Read(*kv, e.key, opts).result();
  }
  return 0;
}
