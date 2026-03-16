#include <gtest/gtest.h>
#include <drogon/drogon.h>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <chrono>
#include <thread>
#include <iostream>
#include <cstdio>

// Test context for MongoDB and HTTP operations
class TestContext {
private:
    static mongocxx::instance instance_;
    static mongocxx::client client_;
    static mongocxx::collection collection_;
    static bool initialized_;

    drogon::HttpResponsePtr last_response_;

public:
    static void Initialize() {
        if (!initialized_) {
            client_ = mongocxx::client{mongocxx::uri{"mongodb://root:password@localhost:27017"}};
            // Используем отдельную тестовую БД, чтобы не загрязнять production данные
            collection_ = client_["LinksTest"]["links"];
            initialized_ = true;
        }
    }

    void InsertSmartLink(const std::string& slug, const std::string& state) {
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;

        collection_.delete_one(document{} << "slug" << slug << finalize);

        auto doc = document{}
            << "slug" << slug
            << "state" << state
            << finalize;

        collection_.insert_one(doc.view());
    }

    void InsertSmartLinkWithRule(
        const std::string& slug,
        const std::string& language,
        const std::string& redirect,
        int start_hours_offset,
        int end_hours_offset,
        bool authorized = false
    ) {
        using bsoncxx::builder::basic::document;
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_array;
        using bsoncxx::builder::basic::make_document;

        collection_.delete_one(make_document(kvp("slug", slug)));

        auto now = std::chrono::system_clock::now();
        auto start_time = now + std::chrono::hours(start_hours_offset);
        auto end_time = now + std::chrono::hours(end_hours_offset);

        // Использовать basic builder вместо stream builder для избежания BSON assertion
        auto doc = make_document(
            kvp("slug", slug),
            kvp("state", "active"),
            kvp("rules", make_array(
                make_document(
                    kvp("language", language),
                    kvp("redirect", redirect),
                    kvp("start", bsoncxx::types::b_date{start_time}),
                    kvp("end", bsoncxx::types::b_date{end_time}),
                    kvp("authorized", authorized)
                )
            ))
        );

        collection_.insert_one(doc.view());
        std::cout << "[TEST] Inserted document: " << slug << " with rule for " << language << std::endl;
    }

    void InsertSmartLinkWithMultipleRules(
        const std::string& slug,
        const std::vector<std::tuple<std::string, std::string, int, int>>& rules
    ) {
        using bsoncxx::builder::basic::document;
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_array;
        using bsoncxx::builder::basic::make_document;

        collection_.delete_one(make_document(kvp("slug", slug)));

        auto now = std::chrono::system_clock::now();

        // Build rules array
        bsoncxx::builder::basic::array rules_array;
        for (const auto& [language, redirect, start_offset, end_offset] : rules) {
            auto start_time = now + std::chrono::hours(start_offset);
            auto end_time = now + std::chrono::hours(end_offset);

            rules_array.append(make_document(
                kvp("language", language),
                kvp("redirect", redirect),
                kvp("start", bsoncxx::types::b_date{start_time}),
                kvp("end", bsoncxx::types::b_date{end_time})
            ));
        }

        auto doc = make_document(
            kvp("slug", slug),
            kvp("state", "active"),
            kvp("rules", rules_array)
        );

        collection_.insert_one(doc.view());
        std::cout << "[TEST] Inserted document: " << slug << " with " << rules.size() << " rules" << std::endl;
    }

    void DeleteSmartLink(const std::string& slug) {
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;

        collection_.delete_one(document{} << "slug" << slug << finalize);
    }

    std::string LoginAndGetToken() {
        std::cout << "[TEST] Logging in to JWT service to get access token" << std::endl;

        // POST to JWT service login endpoint
        std::string cmd = "curl -s -X POST http://localhost:3000/auth/login "
                         "-H 'Content-Type: application/json' "
                         "-d '{\"username\":\"testuser\",\"password\":\"password123\"}'";

        std::cout << "[TEST] Executing: " << cmd << std::endl;

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            throw std::runtime_error("Failed to execute curl for login");
        }

        std::string result;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        int curl_exit = pclose(pipe);

        if (curl_exit != 0) {
            std::cerr << "[TEST] Login failed with exit code: " << curl_exit << std::endl;
            throw std::runtime_error("Login request failed");
        }

        // Parse JSON response to extract access_token
        // Expected format: {"access_token":"...","token_type":"Bearer","expires_in":3600}
        auto token_pos = result.find("\"access_token\":\"");
        if (token_pos == std::string::npos) {
            std::cerr << "[TEST] Failed to find access_token in response: " << result << std::endl;
            throw std::runtime_error("No access_token in login response");
        }

        auto token_start = token_pos + 16; // length of "\"access_token\":\""
        auto token_end = result.find("\"", token_start);
        std::string token = result.substr(token_start, token_end - token_start);

        std::cout << "[TEST] Successfully obtained JWT token (length=" << token.length() << ")" << std::endl;
        return token;
    }

    void SendRequest(
        const std::string& path,
        drogon::HttpMethod method,
        const std::map<std::string, std::string>& headers = {}
    ) {
        std::cout << "[TEST] Sending " << drogon::to_string(method) << " request to http://localhost:8080" << path << std::endl;

        // Использовать системный curl для надежности (Drogon HttpClient имеет проблемы с event loop в тестах)
        std::string cmd = "curl -si -X " + std::string(drogon::to_string(method)) +
                         " http://localhost:8080" + path;

        // Добавить заголовки
        for (const auto& [key, value] : headers) {
            cmd += " -H '" + key + ": " + value + "'";
        }

        std::cout << "[TEST] Executing: " << cmd << std::endl;

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            throw std::runtime_error("Failed to execute curl");
        }

        std::string result;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        int curl_exit = pclose(pipe);

        if (curl_exit != 0) {
            std::cerr << "[TEST] curl failed with exit code: " << curl_exit << std::endl;
            std::cerr << "[TEST] Output: " << result << std::endl;
        }

        // Парсинг HTTP ответа
        int status_code = 500;
        std::string location_header;

        // Найти status code в первой строке
        auto first_line_end = result.find("\r\n");
        if (first_line_end != std::string::npos) {
            std::string status_line = result.substr(0, first_line_end);
            auto code_pos = status_line.find(" ");
            if (code_pos != std::string::npos) {
                code_pos++;
                auto code_end = status_line.find(" ", code_pos);
                status_code = std::stoi(status_line.substr(code_pos, code_end - code_pos));
            }
        }

        // Найти Location header
        auto loc_pos = result.find("location:");
        if (loc_pos == std::string::npos) {
            loc_pos = result.find("Location:");
        }
        if (loc_pos != std::string::npos) {
            auto loc_start = result.find(":", loc_pos) + 1;
            auto loc_end = result.find("\r\n", loc_start);
            location_header = result.substr(loc_start, loc_end - loc_start);
            // Убрать пробелы
            location_header.erase(0, location_header.find_first_not_of(" \t"));
            location_header.erase(location_header.find_last_not_of(" \t\r\n") + 1);
        }

        // Создать Drogon response
        last_response_ = drogon::HttpResponse::newHttpResponse();
        last_response_->setStatusCode(static_cast<drogon::HttpStatusCode>(status_code));
        if (!location_header.empty()) {
            last_response_->addHeader("Location", location_header);
        }

        std::cout << "[TEST] Response received, status=" << status_code
                  << (location_header.empty() ? "" : ", location=" + location_header) << std::endl;
    }

    int GetLastStatusCode() const {
        return static_cast<int>(last_response_->statusCode());
    }

    std::string GetResponseHeader(const std::string& name) const {
        return last_response_->getHeader(name);
    }

    void CleanupAll() {
        collection_.delete_many({});
    }
};

mongocxx::instance TestContext::instance_{};
mongocxx::client TestContext::client_{};
mongocxx::collection TestContext::collection_{};
bool TestContext::initialized_ = false;

// Base test fixture
class SmartLinksTest : public ::testing::Test {
protected:
    TestContext context;

    static void SetUpTestSuite() {
        TestContext::Initialize();
        std::cout << "[TEST SETUP] Test suite initialized" << std::endl;
    }

    static void TearDownTestSuite() {
        std::cout << "[TEST TEARDOWN] Cleanup complete" << std::endl;
    }

    void SetUp() override {
        // Clean database before each test
        context.CleanupAll();
    }

    void TearDown() override {
        // Clean database after each test
        context.CleanupAll();
    }
};

// ============================================================================
// Method Not Allowed Tests (405)
// ============================================================================

TEST_F(SmartLinksTest, Returns405_WhenPostRequestSent) {
    // Arrange
    context.InsertSmartLink("/test-redirect", "active");

    // Act
    context.SendRequest("/test-redirect", drogon::Post);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 405);
}

TEST_F(SmartLinksTest, Returns405_WhenPutRequestSent) {
    // Arrange
    context.InsertSmartLink("/test-redirect", "active");

    // Act
    context.SendRequest("/test-redirect", drogon::Put);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 405);
}

TEST_F(SmartLinksTest, Returns405_WhenDeleteRequestSent) {
    // Arrange
    context.InsertSmartLink("/test-redirect", "active");

    // Act
    context.SendRequest("/test-redirect", drogon::Delete);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 405);
}

TEST_F(SmartLinksTest, DoesNotReturn405_WhenGetRequestSent) {
    // Arrange
    context.InsertSmartLink("/test-redirect", "active");

    // Act
    context.SendRequest("/test-redirect", drogon::Get);

    // Assert
    EXPECT_NE(context.GetLastStatusCode(), 405);
}

TEST_F(SmartLinksTest, DoesNotReturn405_WhenHeadRequestSent) {
    // Arrange
    context.InsertSmartLink("/test-redirect", "active");

    // Act
    context.SendRequest("/test-redirect", drogon::Head);

    // Assert
    EXPECT_NE(context.GetLastStatusCode(), 405);
}

// ============================================================================
// Not Found Tests (404)
// ============================================================================

TEST_F(SmartLinksTest, Returns404_WhenSlugDoesNotExist) {
    // Arrange
    context.DeleteSmartLink("/nonexistent");

    // Act
    context.SendRequest("/nonexistent", drogon::Get);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 404);
}

TEST_F(SmartLinksTest, Returns404_WhenSlugIsDeleted) {
    // Arrange
    context.InsertSmartLink("/deleted-link", "deleted");

    // Act
    context.SendRequest("/deleted-link", drogon::Get);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 404);
}

TEST_F(SmartLinksTest, DoesNotReturn404_WhenSlugExistsAndIsActive) {
    // Arrange
    context.InsertSmartLink("/active-link", "active");

    // Act
    context.SendRequest("/active-link", drogon::Get);

    // Assert
    EXPECT_NE(context.GetLastStatusCode(), 404);
}

// ============================================================================
// Unprocessable Content Tests (422)
// ============================================================================

TEST_F(SmartLinksTest, Returns422_WhenSlugIsFreezed) {
    // Arrange
    context.InsertSmartLink("/freezed-link", "freezed");

    // Act
    context.SendRequest("/freezed-link", drogon::Get);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 422);
}

TEST_F(SmartLinksTest, DoesNotReturn422_WhenSlugIsActive) {
    // Arrange
    context.InsertSmartLink("/active-link", "active");

    // Act
    context.SendRequest("/active-link", drogon::Get);

    // Assert
    EXPECT_NE(context.GetLastStatusCode(), 422);
}

// ============================================================================
// Temporary Redirect Tests (307)
// ============================================================================

TEST_F(SmartLinksTest, Returns307_WithUnconditionalRedirectRule) {
    // Arrange
    context.InsertSmartLinkWithRule("/unconditional", "any", "https://example.com", -1, +24);

    // Act
    context.SendRequest("/unconditional", drogon::Get);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetResponseHeader("Location"), "https://example.com");
}

TEST_F(SmartLinksTest, Returns307_WhenAcceptLanguageMatchesRuleLanguage) {
    // Arrange
    context.InsertSmartLinkWithRule("/language-redirect", "ru-RU", "https://example.com/ru", -1, +1);

    // Act
    std::map<std::string, std::string> headers = {
        {"Accept-Language", "en-US;q=0.9, ru-RU;q=0.8"}
    };
    context.SendRequest("/language-redirect", drogon::Get, headers);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetResponseHeader("Location"), "https://example.com/ru");
}

TEST_F(SmartLinksTest, Returns307_WhenWildcardLanguageIsUsed) {
    // Arrange
    context.InsertSmartLinkWithRule("/wildcard", "any", "https://example.com/all", -1, +1);

    // Act
    std::map<std::string, std::string> headers = {
        {"Accept-Language", "fr-FR"}
    };
    context.SendRequest("/wildcard", drogon::Get, headers);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetResponseHeader("Location"), "https://example.com/all");
}

TEST_F(SmartLinksTest, Returns307_WhenCurrentTimeIsWithinRuleTimeWindow) {
    // Arrange - создаём правило, которое активно сейчас
    context.InsertSmartLinkWithRule("/time-redirect", "any", "https://example.com/now", -1, +1);

    // Act
    context.SendRequest("/time-redirect", drogon::Get);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 307);
}

TEST_F(SmartLinksTest, DoesNotReturn307_WhenCurrentTimeIsOutsideRuleTimeWindow) {
    // Arrange - создаём правило, которое уже истекло
    context.InsertSmartLinkWithRule("/expired", "any", "https://example.com/expired", -10, -5);

    // Act
    context.SendRequest("/expired", drogon::Get);

    // Assert
    EXPECT_NE(context.GetLastStatusCode(), 307);
}

TEST_F(SmartLinksTest, Returns307_WithFirstMatchingRuleWhenMultipleRulesExist) {
    // Arrange
    std::vector<std::tuple<std::string, std::string, int, int>> rules = {
        {"en-US", "https://example.com/en", -1, +1},
        {"ru-RU", "https://example.com/ru", -1, +1}
    };
    context.InsertSmartLinkWithMultipleRules("/multi-redirect", rules);

    // Act
    std::map<std::string, std::string> headers = {
        {"Accept-Language", "en-US"}
    };
    context.SendRequest("/multi-redirect", drogon::Get, headers);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetResponseHeader("Location"), "https://example.com/en");
}

TEST_F(SmartLinksTest, Returns429_WhenNoRuleMatches) {
    // Arrange
    context.InsertSmartLink("/no-rules", "active");

    // Act
    context.SendRequest("/no-rules", drogon::Get);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 429);
}

// ============================================================================
// JWT Authorization Tests
// ============================================================================

TEST_F(SmartLinksTest, Returns307_WithValidJWT_WhenAuthorizationRequired) {
    // Arrange
    context.InsertSmartLinkWithRule("/jwt-required", "any", "https://example.com/secure", -1, +24, true);
    std::string token = context.LoginAndGetToken();

    // Act
    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + token}
    };
    context.SendRequest("/jwt-required", drogon::Get, headers);

    // Assert
    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetResponseHeader("Location"), "https://example.com/secure");
}

TEST_F(SmartLinksTest, Returns429_WithoutJWT_WhenAuthorizationRequired) {
    // Arrange
    context.InsertSmartLinkWithRule("/jwt-required-no-token", "any", "https://example.com/secure", -1, +24, true);

    // Act - no Authorization header
    context.SendRequest("/jwt-required-no-token", drogon::Get);

    // Assert - should NOT redirect
    EXPECT_EQ(context.GetLastStatusCode(), 429);
}

TEST_F(SmartLinksTest, Returns429_WithInvalidJWT_WhenAuthorizationRequired) {
    // Arrange
    context.InsertSmartLinkWithRule("/jwt-required-bad-token", "any", "https://example.com/secure", -1, +24, true);

    // Act - invalid token
    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer invalid.jwt.token"}
    };
    context.SendRequest("/jwt-required-bad-token", drogon::Get, headers);

    // Assert - should NOT redirect
    EXPECT_EQ(context.GetLastStatusCode(), 429);
}

TEST_F(SmartLinksTest, Returns307_WithoutJWT_WhenAuthorizationNotRequired) {
    // Arrange - authorized=false (backward compatibility)
    context.InsertSmartLinkWithRule("/no-auth", "any", "https://example.com/public", -1, +24, false);

    // Act - no Authorization header
    context.SendRequest("/no-auth", drogon::Get);

    // Assert - should redirect (backward compatible)
    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetResponseHeader("Location"), "https://example.com/public");
}

TEST_F(SmartLinksTest, Returns307_WhenAuthorizedFieldMissing) {
    // Arrange - правило без поля "authorized" (старый формат)
    using bsoncxx::builder::basic::document;
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_array;
    using bsoncxx::builder::basic::make_document;

    auto now = std::chrono::system_clock::now();
    auto start_time = now + std::chrono::hours(-1);
    auto end_time = now + std::chrono::hours(+24);

    auto doc = make_document(
        kvp("slug", "/legacy-format"),
        kvp("state", "active"),
        kvp("rules", make_array(
            make_document(
                kvp("language", "any"),
                kvp("redirect", "https://example.com/legacy"),
                kvp("start", bsoncxx::types::b_date{start_time}),
                kvp("end", bsoncxx::types::b_date{end_time})
                // НЕТ поля "authorized" - старый формат
            )
        ))
    );

    TestContext::Initialize();
    auto client = mongocxx::client{mongocxx::uri{"mongodb://root:password@localhost:27017"}};
    auto collection = client["LinksTest"]["links"];
    collection.delete_one(make_document(kvp("slug", "/legacy-format")));
    collection.insert_one(doc.view());

    // Act - no Authorization header
    context.SendRequest("/legacy-format", drogon::Get);

    // Assert - should redirect (backward compatible)
    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetResponseHeader("Location"), "https://example.com/legacy");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
