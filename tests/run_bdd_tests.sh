#!/bin/bash
set -e

echo "=== SmartLinks BDD Tests (Cucumber) ==="
echo ""

# Проверка зависимостей
if ! command -v gem &> /dev/null; then
    echo "ERROR: Ruby gem not found. Please install Ruby."
    exit 1
fi

if ! gem list cucumber -i &> /dev/null; then
    echo "Installing Cucumber..."
    gem install cucumber
fi

# Путь к wireserver
WIRESERVER="${1:-../build/tests/smartlinks_bdd_tests}"

if [ ! -f "$WIRESERVER" ]; then
    echo "ERROR: Wireserver not found at: $WIRESERVER"
    echo "Please build the project first:"
    echo "  cd build && cmake --build ."
    exit 1
fi

# Проверка SmartLinks приложения
echo "Checking if SmartLinks application is running..."
if ! curl -s http://localhost:8080/test-redirect > /dev/null 2>&1; then
    echo "WARNING: SmartLinks application is not responding at http://localhost:8080"
    echo "Please start the application in another terminal:"
    echo "  cd build && ./smartlinks"
    echo ""
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Запуск wireserver в фоне
echo "Starting cucumber-cpp wireserver..."
$WIRESERVER > wireserver.log 2>&1 &
WIRESERVER_PID=$!

# Ждём, пока wireserver запустится
sleep 2

# Проверка, что wireserver запустился
if ! kill -0 $WIRESERVER_PID 2>/dev/null; then
    echo "ERROR: Wireserver failed to start. Check wireserver.log"
    cat wireserver.log
    exit 1
fi

echo "Wireserver started (PID: $WIRESERVER_PID)"
echo ""

# Функция очистки при выходе
cleanup() {
    echo ""
    echo "Stopping wireserver..."
    kill $WIRESERVER_PID 2>/dev/null || true
    wait $WIRESERVER_PID 2>/dev/null || true
}

trap cleanup EXIT

# Запуск Cucumber
echo "Running Cucumber tests..."
cd "$(dirname "$0")"
cucumber "$@"

CUCUMBER_EXIT=$?

echo ""
if [ $CUCUMBER_EXIT -eq 0 ]; then
    echo "✅ All tests passed!"
else
    echo "❌ Some tests failed. Exit code: $CUCUMBER_EXIT"
fi

exit $CUCUMBER_EXIT
