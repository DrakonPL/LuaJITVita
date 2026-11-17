# The vitasdk image is Alpine: no gcc -m32. LuaJIT needs 32-bit host tools for a 32-bit target.
FROM debian:bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      gcc gcc-multilib libc6-dev make git curl ca-certificates \
      zip python3 cmake xz-utils bzip2 file \
 && rm -rf /var/lib/apt/lists/*

ENV VITASDK=/usr/local/vitasdk
ENV PATH=${VITASDK}/bin:${PATH}

ARG VITASDK_CHANNEL=2026.08
RUN git clone --depth 1 https://github.com/vitasdk/vdpm /tmp/vdpm \
 && cd /tmp/vdpm \
 && VITASDK_CHANNEL=${VITASDK_CHANNEL} ./bootstrap-vitasdk.sh \
 && rm -rf /tmp/vdpm

# The 2026.08 channel has no taihen and no vita-rss-libdl. LuaJIT for PSP2 needs dlfcn.h and libdl.
# Same sources and hashes as the vitasdk/packages recipes.
ARG TAIHEN_SHA256=6a7d4592b983292e8b8c53c624fa800e33ff69fe06c9d267cafc33d0ded4d2a9
RUN mkdir /tmp/taihen && cd /tmp/taihen \
 && curl -sSLo taihen.tar.gz https://github.com/yifanlu/taiHEN/releases/download/v0.11/taihen.tar.gz \
 && echo "${TAIHEN_SHA256}  taihen.tar.gz" | sha256sum -c - \
 && tar -xzf taihen.tar.gz \
 && cp -r include lib "${VITASDK}/arm-vita-eabi/" \
 && rm -rf /tmp/taihen

ARG LIBDL_SHA256=5cdc75fe45eb285e48a69f12268488a4de77c8117e54eb327b013d568353fa9c
RUN mkdir /tmp/libdl && cd /tmp/libdl \
 && curl -sSLo libdl.tar.gz https://github.com/isage/vita-rss-libdl/archive/refs/tags/0.3.2.tar.gz \
 && echo "${LIBDL_SHA256}  libdl.tar.gz" | sha256sum -c - \
 && tar -xzf libdl.tar.gz \
 && cmake -S vita-rss-libdl-0.3.2 -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="${VITASDK}/share/vita.toolchain.cmake" \
 && cmake --build build -j"$(nproc)" \
 && cmake --install build \
 && rm -rf /tmp/libdl

WORKDIR /bench
