// Fuzzes the JSON / URL front doors: tensorstore::Spec::FromJson,
// kvstore::Spec::FromJson, Schema/ChunkLayout JSON bindings, and
// Spec::FromUrl / kvstore::Spec::FromUrl, with ToJson / ToUrl round trips.
// The first byte selects the mode, the rest is the text.
#include "fuzz_common.h"
#include "tensorstore/chunk_layout.h"
#include "tensorstore/kvstore/spec.h"
#include "tensorstore/schema.h"
#include "tensorstore/spec.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return -1;
  const uint8_t mode = data[0] % 3;
  std::string_view text(reinterpret_cast<const char*>(data + 1), size - 1);
  if (mode == 2) {
    auto kvs = tensorstore::kvstore::Spec::FromUrl(text);
    if (kvs.ok()) { (void)kvs->ToUrl(); (void)kvs->ToJson(); }
    auto s = tensorstore::Spec::FromUrl(text);
    if (s.ok()) { (void)s->ToJson(); (void)s->ToUrl(); }
    return 0;
  }
  auto j = ::nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded()) return -1;
  if (mode == 0) {
    auto spec = tensorstore::Spec::FromJson(j);
    if (!spec.ok()) return 0;
    (void)spec->ToJson();
    (void)spec->ToUrl();
    (void)spec->schema();
    (void)spec->domain();
    (void)spec->chunk_layout();
    (void)spec->codec();
    (void)spec->fill_value();
  } else {
    auto kvs = tensorstore::kvstore::Spec::FromJson(j);
    if (kvs.ok()) { (void)kvs->ToJson(); (void)kvs->ToUrl(); }
    auto schema = tensorstore::Schema::FromJson(j);
    if (schema.ok()) (void)schema->ToJson();
    auto layout = tensorstore::ChunkLayout::FromJson(j);
    if (layout.ok()) (void)layout->ToJson();
  }
  return 0;
}
