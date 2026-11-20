// Fuzzes tensorstore's binary serialization decoder (serialization::DecodeBatch),
// the format used for pickled / remotely shipped tensorstore objects. The first
// byte picks the decoded type, the rest is the encoded batch:
//   0: tensorstore::Spec   1: tensorstore::kvstore::Spec
//   2: SharedArray<const void>   3: IndexTransform<>
// A successful decode is re-encoded (round trip through the same serializers).
#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "tensorstore/array.h"
#include "tensorstore/index_space/index_transform.h"
#include "tensorstore/kvstore/spec.h"
#include "tensorstore/serialization/batch.h"
#include "tensorstore/serialization/serialization.h"
#include "tensorstore/spec.h"

namespace {
template <typename T>
void DecodeAndReencode(std::string_view encoded) {
  T value;
  if (!tensorstore::serialization::DecodeBatch(encoded, value).ok()) return;
  (void)tensorstore::serialization::EncodeBatch(value);
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return -1;
  std::string_view encoded(reinterpret_cast<const char*>(data + 1), size - 1);
  switch (data[0] % 4) {
    case 0:
      DecodeAndReencode<tensorstore::Spec>(encoded);
      break;
    case 1:
      DecodeAndReencode<tensorstore::kvstore::Spec>(encoded);
      break;
    case 2:
      DecodeAndReencode<tensorstore::SharedArray<const void>>(encoded);
      break;
    case 3:
      DecodeAndReencode<tensorstore::IndexTransform<>>(encoded);
      break;
  }
  return 0;
}
