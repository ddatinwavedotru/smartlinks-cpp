# Multi-stage Dockerfile для SmartLinks C++
# Этап 1: Base image с установленными зависимостями
# Этап 2: Builder - сборка проекта
# Этап 3: Runtime - минимальный образ для запуска

# ========== Stage 1: Base - установка всех зависимостей ==========
FROM ubuntu:22.04 AS base

# Установить часовой пояс (чтобы избежать интерактивных вопросов)
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Обновить пакеты и установить build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    pkg-config \
    ca-certificates \
    libssl-dev \
    zlib1g-dev \
    libjsoncpp-dev \
    uuid-dev \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Установить Drogon framework
RUN git clone https://github.com/drogonframework/drogon.git /tmp/drogon && \
    cd /tmp/drogon && \
    git submodule update --init && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd / && rm -rf /tmp/drogon

# Установить MongoDB C driver
RUN wget https://github.com/mongodb/mongo-c-driver/releases/download/1.24.4/mongo-c-driver-1.24.4.tar.gz -O /tmp/mongo-c-driver.tar.gz && \
    cd /tmp && tar xzf mongo-c-driver.tar.gz && \
    cd mongo-c-driver-1.24.4 && \
    mkdir cmake-build && cd cmake-build && \
    cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --parallel $(nproc) && \
    cmake --install . && \
    ldconfig && \
    cd / && rm -rf /tmp/mongo-c-driver*

# Установить MongoDB C++ driver
RUN git clone https://github.com/mongodb/mongo-cxx-driver.git \
    --branch releases/v3.8 --depth 1 /tmp/mongo-cxx-driver && \
    cd /tmp/mongo-cxx-driver/build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_VERSION=3.8.0 && \
    cmake --build . --parallel $(nproc) && \
    cmake --install . && \
    ldconfig && \
    cd / && rm -rf /tmp/mongo-cxx-driver

# Установить jwt-cpp (header-only library)
RUN git clone https://github.com/Thalhammer/jwt-cpp.git /tmp/jwt-cpp && \
    cd /tmp/jwt-cpp && \
    git checkout v0.7.2 && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DJWT_BUILD_EXAMPLES=OFF \
         -DJWT_BUILD_TESTS=OFF && \
    make install && \
    cd / && rm -rf /tmp/jwt-cpp

# ========== Stage 2: Builder - сборка SmartLinks ==========
FROM base AS builder

WORKDIR /app

# Скопировать исходники
COPY include ./include
COPY src ./src
COPY tests ./tests
COPY config ./config
COPY plugins ./plugins
COPY CMakeLists.txt .

# Собрать проект (без тестов для production)
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF && \
    make -j$(nproc)

# ========== Stage 3: Runtime - минимальный образ для запуска ==========
FROM ubuntu:22.04 AS runtime

# Установить только runtime зависимости (без build tools)
RUN apt-get update && apt-get install -y \
    libssl3 \
    libjsoncpp25 \
    libuuid1 \
    libcurl4 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Скопировать собранный бинарник из builder stage
COPY --from=builder /app/build/smartlinks /usr/local/bin/smartlinks

# Скопировать shared libraries из builder stage
# Drogon libraries
COPY --from=builder /usr/local/lib/libdrogon.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libtrantor.so* /usr/local/lib/
# MongoDB C++ driver libraries
COPY --from=builder /usr/local/lib/libmongocxx.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libbsoncxx.so* /usr/local/lib/
# MongoDB C driver libraries
COPY --from=builder /usr/local/lib/libmongoc-1.0.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libbson-1.0.so* /usr/local/lib/
# SmartLinks library
COPY --from=builder /app/build/libsmartlinks_lib.so /usr/local/lib/

# Скопировать конфигурацию
COPY --from=builder /app/config /app/config

# Обновить ld cache
RUN ldconfig

# Установить рабочую директорию
WORKDIR /app

# Создать директорию для DSL плагинов и скопировать их
RUN mkdir -p /app/plugins
COPY --from=builder /app/build/plugins/libdatetime_plugin.so /app/plugins/
COPY --from=builder /app/build/plugins/liblanguage_plugin.so /app/plugins/
COPY --from=builder /app/build/plugins/libauthorized_plugin.so /app/plugins/

# Экспонировать порт (по умолчанию 8080)
EXPOSE 8080

# Запустить приложение
CMD ["smartlinks"]
