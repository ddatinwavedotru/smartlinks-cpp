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

// Test context for DSL MongoDB and HTTP operations
class DslTestContext {
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
            // Используем отдельную DSL тестовую БД
            collection_ = client_["LinksDSLTest"]["links"];
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

    void InsertSmartLinkWithDslRule(
        const std::string& slug,
        const std::string& rules_dsl
    ) {
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;

        collection_.delete_one(document{} << "slug" << slug << finalize);

        auto doc = document{}
            << "slug" << slug
            << "state" << "active"
            << "rules_dsl" << rules_dsl
            << finalize;

        collection_.insert_one(doc.view());
        std::cout << "[DSL TEST] Inserted document: " << slug << " with DSL: " << rules_dsl << std::endl;
    }

    void DeleteSmartLink(const std::string& slug) {
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;

        collection_.delete_one(document{} << "slug" << slug << finalize);
    }

    std::string LoginAndGetToken() {
        std::cout << "[DSL TEST] Logging in to JWT service to get access token" << std::endl;

        std::string cmd = "curl -s -X POST http://localhost:3000/auth/login "
                         "-H 'Content-Type: application/json' "
                         "-d '{\"username\":\"testuser\",\"password\":\"password123\"}'";

        std::cout << "[DSL TEST] Executing: " << cmd << std::endl;

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
            std::cerr << "[DSL TEST] Login failed with exit code: " << curl_exit << std::endl;
            throw std::runtime_error("Login request failed");
        }

        auto token_pos = result.find("\"access_token\":\"");
        if (token_pos == std::string::npos) {
            std::cerr << "[DSL TEST] Failed to find access_token in response: " << result << std::endl;
            throw std::runtime_error("No access_token in login response");
        }

        auto token_start = token_pos + 16;
        auto token_end = result.find("\"", token_start);
        std::string token = result.substr(token_start, token_end - token_start);

        std::cout << "[DSL TEST] Successfully obtained JWT token (length=" << token.length() << ")" << std::endl;
        return token;
    }

    void SendRequest(
        const std::string& path,
        drogon::HttpMethod method,
        const std::map<std::string, std::string>& headers = {}
    ) {
        std::cout << "[DSL TEST] Sending " << drogon::to_string(method) << " request to http://localhost:8080" << path << std::endl;

        std::string cmd = "curl -si -X " + std::string(drogon::to_string(method)) +
                         " http://localhost:8080" + path;

        for (const auto& [key, value] : headers) {
            cmd += " -H \"" + key + ": " + value + "\"";
        }

        std::cout << "[DSL TEST] Executing: " << cmd << std::endl;

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
            std::cerr << "[DSL TEST] curl failed with exit code: " << curl_exit << std::endl;
            std::cerr << "[DSL TEST] Output: " << result << std::endl;
        }

        ParseResponse(result);
        std::cout << "[DSL TEST] Response received, status=" << GetLastStatusCode() << std::endl;
    }

    void ParseResponse(const std::string& response) {
        last_response_ = drogon::HttpResponse::newHttpResponse();

        auto status_line_end = response.find("\r\n");
        if (status_line_end == std::string::npos) {
            last_response_->setStatusCode(static_cast<drogon::HttpStatusCode>(500));
            return;
        }

        std::string status_line = response.substr(0, status_line_end);
        auto code_start = status_line.find(' ');
        if (code_start != std::string::npos) {
            auto code_end = status_line.find(' ', code_start + 1);
            std::string code_str = status_line.substr(code_start + 1, code_end - code_start - 1);
            int status_code = std::stoi(code_str);
            last_response_->setStatusCode(static_cast<drogon::HttpStatusCode>(status_code));
        }

        auto headers_end = response.find("\r\n\r\n");
        if (headers_end != std::string::npos) {
            std::string headers_section = response.substr(status_line_end + 2, headers_end - status_line_end - 2);
            std::istringstream header_stream(headers_section);
            std::string line;
            while (std::getline(header_stream, line)) {
                if (line.back() == '\r') line.pop_back();
                auto colon = line.find(": ");
                if (colon != std::string::npos) {
                    std::string key = line.substr(0, colon);
                    std::string value = line.substr(colon + 2);
                    last_response_->addHeader(key, value);
                }
            }
        }
    }

    int GetLastStatusCode() const {
        return static_cast<int>(last_response_->statusCode());
    }

    std::string GetLastHeader(const std::string& key) const {
        return std::string(last_response_->getHeader(key));
    }
};

mongocxx::instance DslTestContext::instance_{};
mongocxx::client DslTestContext::client_{mongocxx::uri{}};
mongocxx::collection DslTestContext::collection_{};
bool DslTestContext::initialized_ = false;

// Test fixture
class SmartLinksDslTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        std::cout << "[DSL TEST SUITE SETUP] Initializing MongoDB connection" << std::endl;
        DslTestContext::Initialize();
    }

    void SetUp() override {
        std::cout << "[DSL TEST SETUP] Test initialized" << std::endl;
    }
};

// ==================== Method Not Allowed Tests ====================

TEST_F(SmartLinksDslTest, Returns405_WhenPostRequestSent) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule("/test-dsl-redirect", "DATETIME < 9999-12-31T23:59:59Z -> https://example.com");

    context.SendRequest("/test-dsl-redirect", drogon::Post);

    EXPECT_EQ(context.GetLastStatusCode(), 405);
}

TEST_F(SmartLinksDslTest, Returns405_WhenPutRequestSent) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule("/test-dsl-redirect", "DATETIME < 9999-12-31T23:59:59Z -> https://example.com");

    context.SendRequest("/test-dsl-redirect", drogon::Put);

    EXPECT_EQ(context.GetLastStatusCode(), 405);
}

TEST_F(SmartLinksDslTest, Returns405_WhenDeleteRequestSent) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule("/test-dsl-redirect", "DATETIME < 9999-12-31T23:59:59Z -> https://example.com");

    context.SendRequest("/test-dsl-redirect", drogon::Delete);

    EXPECT_EQ(context.GetLastStatusCode(), 405);
}

TEST_F(SmartLinksDslTest, DoesNotReturn405_WhenGetRequestSent) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule("/test-dsl-redirect", "DATETIME < 9999-12-31T23:59:59Z -> https://example.com");

    context.SendRequest("/test-dsl-redirect", drogon::Get);

    EXPECT_NE(context.GetLastStatusCode(), 405);
}

TEST_F(SmartLinksDslTest, DoesNotReturn405_WhenHeadRequestSent) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule("/test-dsl-redirect", "DATETIME < 9999-12-31T23:59:59Z -> https://example.com");

    context.SendRequest("/test-dsl-redirect", drogon::Head);

    EXPECT_NE(context.GetLastStatusCode(), 405);
}

// ==================== Not Found Tests ====================

TEST_F(SmartLinksDslTest, Returns404_WhenSlugDoesNotExist) {
    DslTestContext context;
    context.DeleteSmartLink("/nonexistent-dsl");

    context.SendRequest("/nonexistent-dsl", drogon::Get);

    EXPECT_EQ(context.GetLastStatusCode(), 404);
}

TEST_F(SmartLinksDslTest, Returns404_WhenSlugIsDeleted) {
    DslTestContext context;
    context.InsertSmartLink("/deleted-link-dsl", "deleted");

    context.SendRequest("/deleted-link-dsl", drogon::Get);

    EXPECT_EQ(context.GetLastStatusCode(), 404);
}

TEST_F(SmartLinksDslTest, DoesNotReturn404_WhenSlugExistsAndIsActive) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule("/active-link-dsl", "DATETIME < 9999-12-31T23:59:59Z -> https://example.com");

    context.SendRequest("/active-link-dsl", drogon::Get);

    EXPECT_NE(context.GetLastStatusCode(), 404);
}

// ==================== Unprocessable Content Tests ====================

TEST_F(SmartLinksDslTest, Returns422_WhenSlugIsFreezed) {
    DslTestContext context;
    context.InsertSmartLink("/freezed-link-dsl", "freezed");

    context.SendRequest("/freezed-link-dsl", drogon::Get);

    EXPECT_EQ(context.GetLastStatusCode(), 422);
}

TEST_F(SmartLinksDslTest, DoesNotReturn422_WhenSlugIsActive) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule("/active-link-dsl", "DATETIME < 9999-12-31T23:59:59Z -> https://example.com");

    context.SendRequest("/active-link-dsl", drogon::Get);

    EXPECT_NE(context.GetLastStatusCode(), 422);
}

// ==================== Temporary Redirect Tests (DSL) ====================

TEST_F(SmartLinksDslTest, Returns307_WithUnconditionalDslRule) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule(
        "/unconditional-dsl",
        "DATETIME < 9999-12-31T23:59:59Z -> https://example.com"
    );

    context.SendRequest("/unconditional-dsl", drogon::Get);

    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetLastHeader("Location"), "https://example.com");
}

TEST_F(SmartLinksDslTest, Returns307_WithLanguageDslRule) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule(
        "/language-redirect-dsl",
        "LANGUAGE = ru-RU -> https://example.ru"
    );

    std::map<std::string, std::string> headers = {
        {"Accept-Language", "ru-RU,ru;q=0.9"}
    };

    context.SendRequest("/language-redirect-dsl", drogon::Get, headers);

    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetLastHeader("Location"), "https://example.ru");
}

TEST_F(SmartLinksDslTest, Returns307_WithMultipleDslRules_FirstMatch) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule(
        "/multi-rule-dsl",
        "LANGUAGE = ru-RU -> https://example.ru; LANGUAGE = en-US -> https://example.com"
    );

    std::map<std::string, std::string> headers = {
        {"Accept-Language", "ru-RU,en-US;q=0.9"}
    };

    context.SendRequest("/multi-rule-dsl", drogon::Get, headers);

    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetLastHeader("Location"), "https://example.ru");
}

TEST_F(SmartLinksDslTest, Returns429_WithDslRule_NoMatch) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule(
        "/no-match-dsl",
        "LANGUAGE = fr-FR -> https://example.fr"
    );

    std::map<std::string, std::string> headers = {
        {"Accept-Language", "en-US"}
    };

    context.SendRequest("/no-match-dsl", drogon::Get, headers);

    EXPECT_EQ(context.GetLastStatusCode(), 429);
}

TEST_F(SmartLinksDslTest, Returns307_WithDatetimeBetween) {
    DslTestContext context;
    // Правило, которое всегда активно
    context.InsertSmartLinkWithDslRule(
        "/datetime-between-dsl",
        "DATETIME IN[2020-01-01T00:00:00Z, 2030-12-31T23:59:59Z] -> https://example-valid.com"
    );

    context.SendRequest("/datetime-between-dsl", drogon::Get);

    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetLastHeader("Location"), "https://example-valid.com");
}

TEST_F(SmartLinksDslTest, Returns307_WithAuthorizedRule) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule(
        "/authorized-dsl",
        "AUTHORIZED -> https://example-private.com; DATETIME < 9999-12-31T23:59:59Z -> https://example-public.com"
    );

    try {
        std::string token = context.LoginAndGetToken();
        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + token}
        };

        context.SendRequest("/authorized-dsl", drogon::Get, headers);

        EXPECT_EQ(context.GetLastStatusCode(), 307);
        EXPECT_EQ(context.GetLastHeader("Location"), "https://example-private.com");
    } catch (const std::exception& e) {
        std::cerr << "[DSL TEST] JWT test skipped: " << e.what() << std::endl;
        GTEST_SKIP() << "JWT service unavailable";
    }
}

TEST_F(SmartLinksDslTest, Returns307_WithComplexDslRule) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule(
        "/complex-dsl",
        "LANGUAGE = ru-RU AND DATETIME IN[2020-01-01T00:00:00Z, 2030-12-31T23:59:59Z] -> https://example-ru-valid.com"
    );

    std::map<std::string, std::string> headers = {
        {"Accept-Language", "ru-RU"}
    };

    context.SendRequest("/complex-dsl", drogon::Get, headers);

    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetLastHeader("Location"), "https://example-ru-valid.com");
}

TEST_F(SmartLinksDslTest, Returns307_WithOrOperator) {
    DslTestContext context;
    context.InsertSmartLinkWithDslRule(
        "/or-operator-dsl",
        "LANGUAGE = fr-FR OR LANGUAGE = de-DE -> https://example-europe.com"
    );

    std::map<std::string, std::string> headers = {
        {"Accept-Language", "de-DE"}
    };

    context.SendRequest("/or-operator-dsl", drogon::Get, headers);

    EXPECT_EQ(context.GetLastStatusCode(), 307);
    EXPECT_EQ(context.GetLastHeader("Location"), "https://example-europe.com");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    std::cout << "[DSL TEST SUITE] Starting SmartLinks DSL integration tests" << std::endl;
    return RUN_ALL_TESTS();
}
