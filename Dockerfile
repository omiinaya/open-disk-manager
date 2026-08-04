# Open Partition Manager — build image
#
# Build:
#   docker build -t opm .
# Run (device access required for real partition work):
#   docker run --rm -it --privileged -v /dev:/dev opm opm list
#
# The image builds the CLI + core library. The GUI needs Qt and is intended
# for desktop use, so it is not included here.

FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake g++ ninja-build libblkid-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -G Ninja \
    && cmake --build build -j"$(nproc)"

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libblkid1 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/src/cli/opm /usr/local/bin/opm
COPY --from=builder /src/man/opm.1 /usr/local/share/man/man1/opm.1

# External tools used for optional features (honest errors when absent).
RUN apt-get update && apt-get install -y --no-install-recommends \
    cryptsetup dislocker efibootmgr chntpw testdisk openssl \
    && rm -rf /var/lib/apt/lists/*

ENTRYPOINT ["opm"]
