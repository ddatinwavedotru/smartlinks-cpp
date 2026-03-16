# SmartLinks Integration Tests (GTest)

## Fuck Gherkin - Simple GTest Tests

No Cucumber. No Gauge. No Ruby. No wire protocol bullshit.

Just **clean C++ GTest tests** like in C# xUnit/NUnit.

## Structure

```cpp
TEST_F(SmartLinksTest, Returns404_WhenSlugDoesNotExist) {
    // Arrange
    context.DeleteSmartLink("/nonexistent");

    // Act
    context.SendRequest("/nonexistent", drogon::Get);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 404);
}
```

Simple. Readable. Works.

## Test Coverage

### Method Not Allowed (405) - 5 tests
- ✅ Returns 405 for POST
- ✅ Returns 405 for PUT
- ✅ Returns 405 for DELETE
- ✅ Does not return 405 for GET
- ✅ Does not return 405 for HEAD

### Not Found (404) - 3 tests
- ✅ Returns 404 when slug does not exist
- ✅ Returns 404 when slug is deleted
- ✅ Does not return 404 when slug is active

### Unprocessable Content (422) - 2 tests
- ✅ Returns 422 when slug is freezed
- ✅ Does not return 422 when slug is active

### Temporary Redirect (307) - 7 tests
- ✅ Returns 307 with unconditional redirect
- ✅ Returns 307 when Accept-Language matches
- ✅ Returns 307 with wildcard language
- ✅ Returns 307 when time is within window
- ✅ Does not return 307 when time expired
- ✅ Returns 307 with first matching rule (multiple rules)
- ✅ Returns 429 when no rule matches

**Total: 17 integration tests**

## Build

```bash
cd build
cmake ..
make -j$(nproc)
```

## Run

```bash
# Start MongoDB
docker-compose up -d mongodb

# Start SmartLinks application
cd build
./smartlinks &

# Run tests (automated - recommended)
../run_integration_tests.sh
```

This script handles server lifecycle automatically.

**Or manually:**

Terminal 1 - Start test server:
```bash
MONGODB_DATABASE=LinksTest ./smartlinks
```

Terminal 2 - Run tests:
```bash
./tests/smartlinks_tests
# Or via CTest:
ctest -R SmartLinks_Integration_Tests --output-on-failure
```

## TestContext

The `TestContext` class provides:

- **MongoDB operations**: `InsertSmartLink()`, `DeleteSmartLink()`, `CleanupAll()`
- **HTTP client**: `SendRequest()` with headers support
- **Assertions**: `GetLastStatusCode()`, `GetResponseHeader()`

All tests use fixture `SmartLinksTest` which:
- Initializes MongoDB connection once
- Cleans database before/after each test
- Provides `context` member for operations

### Test Database Isolation

**IMPORTANT**: Tests use a separate database `LinksTest` to avoid polluting production data:

- **Production**: `mongodb://localhost:27017/Links/links` (used by application)
- **Tests**: `mongodb://localhost:27017/LinksTest/links` (used by tests)

This ensures:
- ✅ Tests don't modify production data
- ✅ Development environment remains stable
- ✅ Tests can run in parallel with application
- ✅ No need to restore DB state after tests

## Why GTest instead of Gherkin?

| Feature | GTest | Gherkin (Cucumber/Gauge) |
|---------|-------|--------------------------|
| **Setup** | ✅ CMake + GTest | ❌ Ruby/Go + plugins |
| **Syntax** | ✅ C++ (type-safe) | ❌ Text parsing |
| **Debugging** | ✅ Native C++ debugger | ❌ Wire protocol mess |
| **Speed** | ✅ Fast | ❌ Slow (IPC overhead) |
| **CI/CD** | ✅ Works everywhere | ❌ Ruby/Go dependencies |
| **Readability** | ✅ Clear AAA pattern | ❌ Step definitions hell |

## Example Output

```
[==========] Running 17 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 17 tests from SmartLinksTest
[ RUN      ] SmartLinksTest.Returns405_WhenPostRequestSent
[       OK ] SmartLinksTest.Returns405_WhenPostRequestSent (25 ms)
[ RUN      ] SmartLinksTest.Returns404_WhenSlugDoesNotExist
[       OK ] SmartLinksTest.Returns404_WhenSlugDoesNotExist (18 ms)
[ RUN      ] SmartLinksTest.Returns307_WithUnconditionalRedirectRule
[       OK ] SmartLinksTest.Returns307_WithUnconditionalRedirectRule (22 ms)
...
[----------] 17 tests from SmartLinksTest (312 ms total)

[----------] Global test environment tear-down
[==========] 17 tests from 1 test suite ran. (313 ms total)
[  PASSED  ] 17 tests.
```

Clean. Simple. Works.
