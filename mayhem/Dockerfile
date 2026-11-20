# syntax=docker/dockerfile:1.7
#
# tensorstore commit image: the org base + a pinned Bazel whose download cache and output
# base live under /opt/toolchains/bazel, so mayhem/build.sh re-runs offline.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# Bazel 8.5.1 (the version upstream pins in .bazelversion), by sha256.
USER root
ARG BAZEL_VERSION=8.5.1
ARG BAZEL_SHA256=61d89402f0368e64b6c827be5de79d8e65382e8124c3cbb97325611a1851392e
RUN mkdir -p /opt/toolchains/bazel/bin /opt/toolchains/bazel/root /opt/toolchains/bazel/repocache \
 && curl -fsSL -o /opt/toolchains/bazel/bin/bazel \
      "https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-linux-x86_64" \
 && echo "${BAZEL_SHA256}  /opt/toolchains/bazel/bin/bazel" | sha256sum -c - \
 && chmod 0755 /opt/toolchains/bazel/bin/bazel \
 && chown -R 2000:2000 /opt/toolchains/bazel/root /opt/toolchains/bazel/repocache

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

# Bazel keeps its own action cache in /opt/toolchains/bazel/root (outside the tree), which is
# what keeps the graded rebuild incremental; ccache would not see Bazel's absolute-path compiler.
RUN bash mayhem/build.sh

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
