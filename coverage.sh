#!/bin/bash
set -e

echo "=== Measuring Test Coverage ==="

# Проверка gcovr
if ! command -v gcovr &> /dev/null; then
    echo "Error: gcovr not found. Install: sudo apt-get install gcovr"
    exit 1
fi

echo ""
echo "Prerequisites for full test coverage:"
echo "  1. MongoDB running on localhost:27017"
echo "  2. JWT service running on localhost:3000"
echo "  Tip: Run 'docker-compose up mongodb jwt-service' in another terminal"
echo ""

# Очистка build
echo "[1/6] Cleaning build directory..."
rm -rf build && mkdir -p build && cd build

# Конфигурация с coverage
echo "[2/6] Configuring CMake with ENABLE_COVERAGE=ON..."
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..

# Сборка
echo "[3/6] Building project..."
make -j$(nproc)

# Запуск тестов
echo "[4/6] Running tests..."
export MONGODB_URI="mongodb://root:password@localhost:27017"
export JWT_SERVICE_URL="http://localhost:3000"
ctest --output-on-failure --verbose || true

# Генерация отчетов gcovr
echo "[5/6] Generating coverage reports with gcovr..."
gcovr -r .. \
      --filter ../src/ \
      --filter ../include/ \
      --filter ../plugins/ \
      --exclude ../tests/ \
      --exclude ../jwt-service/ \
      --exclude ../extern/ \
      --exclude ../_deps/ \
      --html --html-details -o coverage_report.html \
      --print-summary

# XML для Codecov
gcovr -r .. \
      --filter ../src/ \
      --filter ../include/ \
      --filter ../plugins/ \
      --exclude ../tests/ \
      --exclude ../jwt-service/ \
      --exclude ../extern/ \
      --exclude ../_deps/ \
      --xml > coverage.xml

echo "[6/6] Done!"
echo "========================================"
echo "✓ Coverage report generated!"
echo "  HTML: build/coverage_report.html"
echo "  XML:  build/coverage.xml"
echo "========================================"
