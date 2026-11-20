// Fuzzes a tensorstore array driver (zarr, zarr3, n5, neuroglancer_precomputed)
// end to end through the public API: the input is a container of
// (key, value) entries written into a fresh in-memory kvstore (metadata file,
// chunk/shard files), then tensorstore::Open + Read decodes them.
#include "fuzz_common.h"

#ifndef MAYHEM_DRIVER
#error "define MAYHEM_DRIVER"
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto entries = mayhem_fuzz::ParseContainer(data, size);
  if (entries.empty()) return -1;
  // A fresh context per input: the memory kvstore and the cache pool are
  // context resources, so no state carries over between inputs.
  auto context = tensorstore::Context::Default();
  auto mem = tensorstore::kvstore::Open({{"driver", "memory"}}, context).result();
  if (!mem.ok()) return 0;
  for (auto& [key, value] : entries) {
    if (key.empty()) continue;
    (void)tensorstore::kvstore::Write(*mem, key, absl::Cord(value)).result();
  }
  auto store = tensorstore::Open(
                   {{"driver", MAYHEM_DRIVER},
                    {"kvstore", {{"driver", "memory"}}}},
                   context, tensorstore::OpenMode::open,
                   tensorstore::ReadWriteMode::read)
                   .result();
  if (!store.ok()) return 0;
  mayhem_fuzz::ReadBounded(*store);
  return 0;
}
