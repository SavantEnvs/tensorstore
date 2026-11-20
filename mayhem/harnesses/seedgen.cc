// Not a fuzz target: writes the serialization_fuzzer and ocdbt_format_fuzzer
// seeds (valid encodings made by tensorstore's own encoders) into argv[1]/argv[2].
// Run by hand when regenerating mayhem/<target>/testsuite; build.sh does not build it.
#include <fstream>
#include <string>

#include "absl/strings/cord.h"
#include "tensorstore/array.h"
#include "tensorstore/index_space/index_transform_builder.h"
#include "tensorstore/kvstore/ocdbt/format/manifest.h"
#include "tensorstore/kvstore/ocdbt/format/version_tree.h"
#include "tensorstore/kvstore/spec.h"
#include "tensorstore/serialization/batch.h"
#include "tensorstore/spec.h"

namespace ocdbt = tensorstore::internal_ocdbt;

static void Put(const std::string& path, char tag, const std::string& body) {
  std::ofstream f(path, std::ios::binary);
  f.put(tag);
  f << body;
}

template <typename T>
static void PutSer(const std::string& path, char tag, const T& v) {
  auto enc = tensorstore::serialization::EncodeBatch(v);
  if (enc.ok()) Put(path, tag, *enc);
}

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  std::string s = argv[1], o = argv[2];
  for (const char* j :
       {R"({"driver":"zarr","kvstore":"memory://a/","dtype":"uint16","schema":{"domain":{"shape":[10,20]},"chunk_layout":{"read_chunk":{"shape":[5,5]}}}})",
        R"({"driver":"zarr3","kvstore":"memory://","dtype":"int32","schema":{"domain":{"shape":[4,4]}},"transform":{"input_shape":[4,4],"output":[{"input_dimension":1},{"input_dimension":0}]}})",
        R"({"driver":"n5","kvstore":{"driver":"memory","path":"x/"},"metadata":{"dimensions":[3,4],"blockSize":[2,2],"dataType":"uint8","compression":{"type":"raw"}}})",
        R"({"driver":"neuroglancer_precomputed","kvstore":"memory://p/","scale_index":0})",
        R"({"driver":"array","array":[[1,2],[3,4]],"dtype":"float32"})"}) {
    static int i = 0;
    auto spec = tensorstore::Spec::FromJson(::nlohmann::json::parse(j));
    if (spec.ok()) PutSer(s + "/spec" + std::to_string(i++), 0, *spec);
  }
  if (auto k = tensorstore::kvstore::Spec::FromJson("memory://a/b"); k.ok())
    PutSer(s + "/kvspec_memory", 1, *k);
  if (auto k = tensorstore::kvstore::Spec::FromJson("memory://x.zip|zip:inner/");
      k.ok())
    PutSer(s + "/kvspec_zip", 1, *k);
  PutSer(s + "/array_int32", 2,
         tensorstore::SharedArray<const void>(
             tensorstore::MakeArray<int32_t>({{1, 2, 3}, {4, 5, 6}})));
  PutSer(s + "/array_string", 2,
         tensorstore::SharedArray<const void>(
             tensorstore::MakeArray<std::string>({"ab", "c"})));
  if (auto t = tensorstore::IndexTransformBuilder<>(2, 2)
                   .input_origin({1, 2})
                   .input_shape({3, 4})
                   .output_single_input_dimension(0, 5, 2, 1)
                   .output_constant(1, 7)
                   .Finalize();
      t.ok())
    PutSer(s + "/transform", 3, tensorstore::IndexTransform<>(*t));

  ocdbt::Config config;
  ocdbt::VersionTreeNode node;
  node.version_tree_arity_log2 = config.version_tree_arity_log2;
  node.height = 0;
  auto& entries = node.entries.emplace<ocdbt::VersionTreeNode::LeafNodeEntries>();
  auto& ref = entries.emplace_back();
  ref.root.location =
      ocdbt::IndirectDataReference{ocdbt::DataFileId{"", "f1"}, 0, 10};
  ref.root.statistics = {0, 10, 1};
  ref.generation_number = 1;
  ref.root_height = 0;
  ref.commit_time = ocdbt::CommitTime{20};
  if (auto e = ocdbt::EncodeVersionTreeNode(config, node); e.ok())
    Put(o + "/version_tree_leaf", 2, std::string(*e));
  ocdbt::Manifest m;
  m.config = config;
  m.versions = entries;
  if (auto e = ocdbt::EncodeManifest(m); e.ok())
    Put(o + "/manifest", 0, std::string(*e));
  return 0;
}
