// Fuzzes a tensorstore image driver (png, tiff, bmp, webp): the whole input is
// the image file, stored in a fresh in-memory kvstore and opened with
// tensorstore::Open {driver: <image>} + Read.
#include "fuzz_common.h"

#ifndef MAYHEM_DRIVER
#error "define MAYHEM_DRIVER"
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto context = tensorstore::Context::Default();
  auto mem = tensorstore::kvstore::Open({{"driver", "memory"}}, context).result();
  if (!mem.ok()) return 0;
  (void)tensorstore::kvstore::Write(
      *mem, "f", absl::Cord(std::string_view(reinterpret_cast<const char*>(data), size)))
      .result();
  auto store = tensorstore::Open(
                   {{"driver", MAYHEM_DRIVER},
                    {"kvstore", {{"driver", "memory"}, {"path", "f"}}}},
                   context, tensorstore::OpenMode::open,
                   tensorstore::ReadWriteMode::read)
                   .result();
  if (!store.ok()) return 0;
  mayhem_fuzz::ReadBounded(*store);
  return 0;
}
