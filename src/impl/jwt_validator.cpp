#include "impl/jwt_validator.hpp"
#include <drogon/HttpClient.h>
#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>
#include <stdexcept>
#include <future>

namespace smartlinks::impl {

JwtValidator::JwtValidator(
    std::string jwks_uri,
    std::string expected_issuer,
    std::string expected_audience
) : jwks_uri_(std::move(jwks_uri)),
    expected_issuer_(std::move(expected_issuer)),
    expected_audience_(std::move(expected_audience)),
    verifier_(jwt::default_clock{}),
    verifier_initialized_(false),
    verifier_kid_("") {}

std::string JwtValidator::FetchJwks() const {
    // Создать HTTP клиент для загрузки JWKS от JWT-микросервиса
    auto client = drogon::HttpClient::newHttpClient(jwks_uri_);

    std::promise<std::string> promise;
    auto future = promise.get_future();

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/.well-known/jwks.json");

    client->sendRequest(req, [&promise](drogon::ReqResult result, const drogon::HttpResponsePtr& response) {
        if (result == drogon::ReqResult::Ok && response->getStatusCode() == drogon::k200OK) {
            promise.set_value(std::string(response->getBody()));
        } else {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Failed to fetch JWKS from JWT service")
            ));
        }
    });

    return future.get();
}

void JwtValidator::InitializeVerifier(const std::string& token_kid) const {
    if (verifier_initialized_ && (token_kid.empty() || token_kid == verifier_kid_)) {
        return;
    }

    auto jwks_json = FetchJwks();
    auto jwks = jwt::jwks<jwt::traits::kazuho_picojson>(jwks_json);

    if (jwks.begin() == jwks.end()) {
        throw std::runtime_error("JWKS does not contain any keys");
    }

    auto jwk = (!token_kid.empty() && jwks.has_jwk(token_kid)) ? jwks.get_jwk(token_kid) : *jwks.begin();

    const std::string n = jwk.get_jwk_claim("n").as_string();
    const std::string e = jwk.get_jwk_claim("e").as_string();
    const std::string public_key_pem = jwt::helper::create_public_key_from_rsa_components(n, e);
    verifier_kid_ = jwk.has_key_id() ? jwk.get_key_id() : "";

    verifier_ = jwt::verify<jwt::default_clock, jwt::traits::kazuho_picojson>(jwt::default_clock{})
        .allow_algorithm(jwt::algorithm::rs256(public_key_pem, "", "", ""))
        .with_issuer(expected_issuer_)
        .with_audience(expected_audience_);

    verifier_initialized_ = true;
}

bool JwtValidator::Validate(const std::string& token) const {
    try {
        // Декодировать JWT (явно указываем json_traits)
        auto decoded = jwt::decoded_jwt<jwt::traits::kazuho_picojson>(token);

        // Инициализировать verifier при первом вызове с учётом kid
        InitializeVerifier(decoded.get_key_id());

        // Проверить подпись, expiration, issuer, audience
        verifier_.verify(decoded);

        return true;
    } catch (const std::exception& e) {
        // Токен невалиден (подпись неверна, истёк, и т.д.)
        LOG_DEBUG << "JWT validation failed: " << e.what();
        return false;
    }
}

} // namespace smartlinks::impl
