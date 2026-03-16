#include "test_context.hpp"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/types.hpp>
#include <iostream>

using namespace bsoncxx::builder::stream;

namespace smartlinks::test {

TestContext::TestContext() {
    // Инициализация MongoDB клиента
    try {
        mongo_client_ = std::make_shared<mongocxx::client>(
            mongocxx::uri("mongodb://root:password@localhost:27017")
        );
        collection_ = mongo_client_->database("Links").collection("links");
    } catch (const std::exception& e) {
        std::cerr << "Failed to connect to MongoDB: " << e.what() << std::endl;
        throw;
    }

    // Инициализация HTTP клиента
    http_client_ = drogon::HttpClient::newHttpClient("http://localhost:8080");
}

TestContext::~TestContext() {
    // Очистка тестовых данных
    CleanupAllTestData();
}

void TestContext::InsertSmartLink(const std::string& slug, const std::string& state) {
    auto doc = document{}
        << "slug" << slug
        << "state" << state
        << "rules" << open_array << close_array
        << finalize;

    collection_.delete_one(document{} << "slug" << slug << finalize);
    collection_.insert_one(doc.view());
}

void TestContext::InsertSmartLinkWithRule(
    const std::string& slug,
    const std::string& language,
    const std::string& redirect,
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end
) {
    auto start_bson = bsoncxx::types::b_date(start);
    auto end_bson = bsoncxx::types::b_date(end);

    auto doc = document{}
        << "slug" << slug
        << "state" << "active"
        << "rules" << open_array
            << open_document
                << "language" << language
                << "redirect" << redirect
                << "start" << start_bson
                << "end" << end_bson
            << close_document
        << close_array
        << finalize;

    collection_.delete_one(document{} << "slug" << slug << finalize);
    collection_.insert_one(doc.view());
}

void TestContext::InsertSmartLinkWithRules(
    const std::string& slug,
    const std::vector<std::map<std::string, std::string>>& rules
) {
    auto doc_builder = document{};
    doc_builder << "slug" << slug << "state" << "active";

    auto rules_array = doc_builder << "rules" << open_array;

    for (const auto& rule : rules) {
        auto start_hours = std::stoi(rule.at("start_hours"));
        auto end_hours = std::stoi(rule.at("end_hours"));

        auto start = std::chrono::system_clock::now() + std::chrono::hours(start_hours);
        auto end = std::chrono::system_clock::now() + std::chrono::hours(end_hours);

        rules_array << open_document
            << "language" << rule.at("language")
            << "redirect" << rule.at("redirect")
            << "start" << bsoncxx::types::b_date(start)
            << "end" << bsoncxx::types::b_date(end)
            << close_document;
    }

    rules_array << close_array;
    auto doc = doc_builder << finalize;

    collection_.delete_one(document{} << "slug" << slug << finalize);
    collection_.insert_one(doc.view());
}

void TestContext::DeleteSmartLink(const std::string& slug) {
    collection_.delete_one(document{} << "slug" << slug << finalize);
}

void TestContext::CleanupAllTestData() {
    // Удаляем все документы, которые могли быть созданы тестами
    // (можно улучшить, помечая тестовые документы специальным полем)
    collection_.delete_many(document{} << finalize);
}

void TestContext::SendRequest(
    const std::string& path,
    drogon::HttpMethod method,
    const std::map<std::string, std::string>& headers
) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath(path);
    req->setMethod(method);

    for (const auto& [key, value] : headers) {
        req->addHeader(key, value);
    }

    // Синхронный запрос
    bool request_completed = false;
    http_client_->sendRequest(
        req,
        [this, &request_completed](drogon::ReqResult result, const drogon::HttpResponsePtr& resp) {
            if (result == drogon::ReqResult::Ok) {
                last_response_ = resp;
            } else {
                std::cerr << "HTTP request failed" << std::endl;
                last_response_ = nullptr;
            }
            request_completed = true;
        }
    );

    // Ждём завершения запроса
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!request_completed && std::chrono::steady_clock::now() < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!request_completed) {
        throw std::runtime_error("HTTP request timeout");
    }
}

int TestContext::GetLastStatusCode() const {
    if (!last_response_) {
        throw std::runtime_error("No response available");
    }
    return static_cast<int>(last_response_->statusCode());
}

std::string TestContext::GetResponseHeader(const std::string& header) const {
    if (!last_response_) {
        throw std::runtime_error("No response available");
    }
    return last_response_->getHeader(header);
}

} // namespace smartlinks::test
