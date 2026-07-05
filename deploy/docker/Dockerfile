# Stage 1: Build
FROM ubuntu:24.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libsodium-dev \
    libavcodec-dev \
    libavutil-dev \
    libswscale-dev \
    libavformat-dev \
    libswresample-dev \
    libxkbcommon-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source code
COPY . .

# Build the core engine (excluding NexusDash UI for server-side)
RUN cmake -B build -S . -DBUILD_NEXUSDASH=OFF
RUN cmake --build build --config Release -j$(nproc)

# Stage 2: Runtime
FROM ubuntu:24.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libsodium23 \
    libavcodec60 \
    libavutil58 \
    libswscale7 \
    libavformat60 \
    libswresample4 \
    libxkbcommon0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy binaries from builder
COPY --from=builder /app/build/parsec-lite-core /usr/local/bin/parsec-lite-core

# Default entrypoint for Relay or Host mode
ENTRYPOINT ["parsec-lite-core"]
CMD ["--help"]
