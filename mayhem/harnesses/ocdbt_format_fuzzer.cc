// Fuzzes the OCDBT on-disk format decoders (tensorstore/kvstore/ocdbt/format):
// the manifest, b+tree node and version tree node parsers that the ocdbt
// kvstore runs on files read from storage. The first byte picks the decoder,
// the rest is the encoded file.
#include <stddef.h>
#include <stdint.h>

#include "absl/strings/cord.h"
#include "tensorstore/kvstore/ocdbt/format/btree.h"
#include "tensorstore/kvstore/ocdbt/format/manifest.h"
#include "tensorstore/kvstore/ocdbt/format/version_tree.h"

namespace ocdbt = tensorstore::internal_ocdbt;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return -1;
  absl::Cord encoded(
      std::string_view(reinterpret_cast<const char*>(data + 1), size - 1));
  switch (data[0] % 3) {
    case 0: {
      auto m = ocdbt::DecodeManifest(encoded);
      (void)m;
      break;
    }
    case 1: {
      auto n = ocdbt::DecodeBtreeNode(encoded, /*base_path=*/{});
      (void)n;
      break;
    }
    case 2: {
      auto n = ocdbt::DecodeVersionTreeNode(encoded, /*base_path=*/{});
      (void)n;
      break;
    }
  }
  return 0;
}
