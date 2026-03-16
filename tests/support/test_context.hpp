
#pragma once

#include <memory>
#include <string>
#include <chrono>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/json.hpp>
#include <drogon/drogon.h>
#include <drogon/HttpClient.h>

namespace smartlinks::test {

/**
 * TestContext - контекст для BDD тестов
 * Управляет MongoDB соединением, HTTP клиентом и тестовыми данными
 */
class TestContext {
private:
    // MongoDB
    std::shared_ptr<mongocxx::client> mongo_client_;
    mongocxx::collection collection_;

    // HTTP Client
    std::shared_ptr<drogon::HttpClient> http_client_;

    // Test state
    drogon::HttpResponsePtr last_response_;
    std::string current_slug_;

public:
    TestContext();
    ~TestContext();

    // MongoDB operations
    void InsertSmartLink(const std::string& slug, const std::string& state);
    void InsertSmartLinkWithRule(
        const std::string& slug,
        const std::string& language,
        const std::string& redirect,
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end
    );
    void InsertSmartLinkWithRules(
        const std::string& slug,
        const std::vector<std::map<std::string, std::string>>& rules
    );
    void DeleteSmartLink(const std::string& slug);
    void CleanupAllTestData();

    // HTTP operations
    void SendRequest(
        const std::string& path,
        drogon::HttpMethod method,
        const std::map<std::string, std::string>& headers = {}
    );

    // Getters
    drogon::HttpResponsePtr GetLastResponse() const { return last_response_; }
    int GetLastStatusCode() const;
    std::string GetResponseHeader(const std::string& header) const;

    // Setters
    void SetCurrentSlug(const std::string& slug) { current_slug_ = slug; }
    std::string GetCurrentSlug() const { return current_slug_; }
};

} // namespace smartlinks::test
