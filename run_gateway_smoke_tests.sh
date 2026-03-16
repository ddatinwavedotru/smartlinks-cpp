#!/bin/bash
# Smoke tests for SmartLinks system via API gateway only
# Brings up full docker-compose stack and validates core gateway routes

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
FAILED_TESTS=()
TEST_START_TIME=$(date +%s)
BASE_URL="http://localhost"

print_header() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

cleanup_all() {
    print_header "Cleaning up all resources"
    echo "Stopping Docker containers..."
    docker-compose -f "$PROJECT_ROOT/docker-compose.yml" down 2>/dev/null || true
    sleep 2
    print_success "Full cleanup completed"
    echo ""
}

wait_for_health() {
    local container_name="$1"
    local display_name="$2"
    local max_retries=30

    echo "Waiting for $display_name to be healthy..."
    for i in $(seq 1 $max_retries); do
        if docker inspect "$container_name" --format='{{.State.Health.Status}}' 2>/dev/null | grep -q "healthy"; then
            print_success "$display_name is healthy"
            return 0
        fi
        sleep 2
        echo -n "."
    done
    echo ""
    print_error "$display_name did not become healthy in $((max_retries*2)) seconds"
    docker logs "$container_name" | tail -20
    FAILED_TESTS+=("$display_name Health Check")
    return 1
}

seed_db() {
    echo "Seeding MongoDB with required smoke-test data..."
    # Ensure hash-password.js exists in JWT container
    if ! docker exec smartlinks-jwt test -f /app/hash-password.js 2>/dev/null; then
        if ! docker cp "$PROJECT_ROOT/jwt-service/hash-password.js" smartlinks-jwt:/app/ >/dev/null 2>&1; then
            print_error "Failed to copy hash-password.js to JWT container"
            FAILED_TESTS+=("Seed DB")
            return 1
        fi
    fi

    # Generate bcrypt hash via JWT container
    PASSWORD_HASH_OUTPUT=$(docker exec smartlinks-jwt node /app/hash-password.js password123 2>&1)
    PASSWORD_HASH=$(echo "$PASSWORD_HASH_OUTPUT" | grep "Hash:" | awk '{print $2}')

    if [ -z "$PASSWORD_HASH" ]; then
        print_error "Failed to generate password hash for test user"
        echo "Output: $PASSWORD_HASH_OUTPUT"
        FAILED_TESTS+=("Password Hash Generation")
        return 1
    fi

    ESCAPED_HASH="${PASSWORD_HASH}"

    # Seed links data
    LINKS_RESULT=$(
        cat <<'MONGO' | docker exec -i smartlinks-mongodb mongosh -u root -p password --authenticationDatabase admin --quiet Links 2>&1 || true
db.links.updateOne(
  { slug: "/test-redirect" },
  { $set: { slug: "/test-redirect", state: "active", rules: [ { language: "any", redirect: "https://example.com", start: new Date(Date.now() - 24*60*60*1000), end: new Date(Date.now() + 24*60*60*1000) } ] } },
  { upsert: true }
);
db.links.updateOne(
  { slug: "/deleted-link" },
  { $set: { slug: "/deleted-link", state: "deleted", rules: [] } },
  { upsert: true }
);
db.links.updateOne(
  { slug: "/freezed-link" },
  { $set: { slug: "/freezed-link", state: "freezed", rules: [] } },
  { upsert: true }
);
MONGO
    )

    if ! echo "$LINKS_RESULT" | grep -E -q "acknowledged|modifiedCount|upsertedCount"; then
        print_error "Failed to seed Links.links data"
        echo "Result: $LINKS_RESULT"
        FAILED_TESTS+=("Seed DB")
        return 1
    fi

    # Seed users data
    USERS_RESULT=$(
        cat <<MONGO | docker exec -i smartlinks-mongodb mongosh -u root -p password --authenticationDatabase admin --quiet Links 2>&1 || true
db.users.deleteOne({ username: "testuser" });
db.users.insertOne({ username: "testuser", password_hash: "${ESCAPED_HASH}", email: "testuser@example.com", created_at: new Date(), active: true });
db.users.deleteOne({ username: "admin" });
db.users.insertOne({ username: "admin", password_hash: "${ESCAPED_HASH}", email: "admin@example.com", created_at: new Date(), active: true, roles: ["admin"] });
MONGO
    )

    if ! echo "$USERS_RESULT" | grep -E -q "insertedId|acknowledged"; then
        print_error "Failed to seed Links.users data"
        echo "Result: $USERS_RESULT"
        FAILED_TESTS+=("Seed DB")
        return 1
    fi

    print_success "MongoDB seeded"
}

expect_status() {
    local name="$1"
    local expected="$2"
    shift 2

    local body
    body=$(mktemp)
    local status
    status=$(curl -s -o "$body" -w "%{http_code}" "$@" || true)

    if [ "$status" != "$expected" ]; then
        print_error "$name (expected $expected, got $status)"
        echo "Response body:"
        cat "$body"
        FAILED_TESTS+=("$name")
        rm -f "$body"
        return 1
    fi

    print_success "$name"
    rm -f "$body"
    return 0
}

expect_header_contains() {
    local name="$1"
    local expected_header="$2"
    shift 2

    local headers
    headers=$(mktemp)
    local body
    body=$(mktemp)

    local status
    status=$(curl -s -D "$headers" -o "$body" -w "%{http_code}" "$@" || true)

    if [ "$status" != "307" ]; then
        print_error "$name (expected 307, got $status)"
        echo "Headers:"; cat "$headers"
        echo "Body:"; cat "$body"
        FAILED_TESTS+=("$name")
        rm -f "$headers" "$body"
        return 1
    fi

    if ! grep -qi "^${expected_header}" "$headers"; then
        print_error "$name (missing header: $expected_header)"
        echo "Headers:"; cat "$headers"
        FAILED_TESTS+=("$name")
        rm -f "$headers" "$body"
        return 1
    fi

    print_success "$name"
    rm -f "$headers" "$body"
    return 0
}

print_header "SmartLinks - Gateway Smoke Tests"
echo "Project: $PROJECT_ROOT"

echo "Starting full docker-compose stack..."
docker-compose -f "$PROJECT_ROOT/docker-compose.yml" up -d

trap cleanup_all EXIT INT TERM

# Health checks
wait_for_health "smartlinks-mongodb" "MongoDB" || true
wait_for_health "smartlinks-jwt" "JWT Service" || true
wait_for_health "smartlinks-gateway" "API Gateway" || true

# Seed DB with known data for smoke tests
seed_db

print_header "Gateway Smoke Checks"

expect_status "Gateway health" "200" "$BASE_URL/gateway/health"
expect_status "JWT health via gateway" "200" "$BASE_URL/jwt/health"

expect_status "JWKS via gateway" "200" "$BASE_URL/.well-known/jwks.json"

# Login via gateway
login_body=$(mktemp)
login_status=$(curl -s -o "$login_body" -w "%{http_code}" \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"password123"}' \
  "$BASE_URL/jwt/auth/login" || true)

if [ "$login_status" != "200" ]; then
    print_error "JWT login via gateway (expected 200, got $login_status)"
    echo "Response body:"; cat "$login_body"
    FAILED_TESTS+=("JWT login via gateway")
else
    if ! grep -q "access_token" "$login_body"; then
        print_error "JWT login via gateway (missing access_token)"
        echo "Response body:"; cat "$login_body"
        FAILED_TESTS+=("JWT login via gateway")
    else
        print_success "JWT login via gateway"
    fi
fi
rm -f "$login_body"

# SmartLinks smoke checks via gateway
expect_header_contains "SmartLinks redirect /test-redirect" "Location: https://example.com" "$BASE_URL/test-redirect"
expect_status "SmartLinks /deleted-link" "404" "$BASE_URL/deleted-link"
expect_status "SmartLinks /freezed-link" "422" "$BASE_URL/freezed-link"

print_header "Smoke Tests Summary"

if [ ${#FAILED_TESTS[@]} -eq 0 ]; then
    print_success "All gateway smoke tests passed"
else
    print_error "Some smoke tests failed"
    echo "Failed tests:"
    for t in "${FAILED_TESTS[@]}"; do
        echo "  - $t"
    done
fi

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - TEST_START_TIME))

echo "Total execution time: ${ELAPSED}s"

if [ ${#FAILED_TESTS[@]} -ne 0 ]; then
    exit 1
fi
