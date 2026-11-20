"""Builds a libFuzzer binary and its standalone reproducer from one harness."""

def tensorstore_fuzzer(name, srcs, deps, defines = []):
    native.cc_library(
        name = name + "_lib",
        srcs = srcs,
        local_defines = defines,
        deps = deps,
        alwayslink = True,
    )
    native.cc_binary(
        name = name,
        deps = [":" + name + "_lib", ":lsan_off"],
        linkopts = ["-fsanitize=fuzzer"],
    )
    native.cc_binary(
        name = name + "-standalone",
        deps = [":" + name + "_lib", ":lsan_off", ":standalone_main"],
    )
