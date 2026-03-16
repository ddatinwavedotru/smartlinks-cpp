#include <drogon/drogon.h>
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <fstream>
#include <cstdlib>
#include <mutex>
#include <json/json.h>

#include "ioc/ioc.hpp"
#include "ioc/init_command.hpp"
#include "ioc/command.hpp"
#include "middleware/drogon_middleware_adapter.hpp"
#include "middleware/method_not_allowed_middleware.hpp"
#include "middleware/not_found_middleware.hpp"
#include "middleware/unprocessable_content_middleware.hpp"
#include "middleware/temporary_redirect_middleware.hpp"
#include "impl/mongodb_repository.hpp"
#include "impl/statable_smart_link_repository.hpp"
#include "impl/smart_link_redirect_rules_repository.hpp"
#include "impl/smart_link_redirect_service.hpp"
#include "impl/freeze_smart_link_service.hpp"
#include "impl/jwt_validator.hpp"
// DSL components
#include "dsl/parser/parser_registry.hpp"
#include "dsl/parser/combined_parser.hpp"
#include "dsl/parser/register_parser_command.hpp"
#include "dsl/plugin/plugin_loader.hpp"
#include "impl/smart_link_redirect_rules_repository_dsl.hpp"
#include "impl/smart_link_redirect_service_dsl.hpp"

using namespace smartlinks;

/**
 * @brief Чтение конфигурации из JSON файла
 *
 * Аналог appsettings.Development.json из C# версии.
 */
Json::Value LoadConfig(const std::string& config_path) {
    std::ifstream config_file(config_path);
    Json::Value config;
    Json::CharReaderBuilder builder;
    std::string errors;

    if (!Json::parseFromStream(builder, config_file, &config, &errors)) {
        throw std::runtime_error("Failed to parse config: " + errors);
    }

    return config;
}

/**
 * @brief Регистрация всех зависимостей в IoC контейнере
 *
 * Использует паттерн IoC.Register для ВСЕХ регистраций (как в плане).
 * Порядок регистрации важен: от низкоуровневых к высокоуровневым.
 */
void RegisterDependencies(const Json::Value& config) {
    // ========== Инициализировать IoC ==========
    auto init_cmd = std::make_shared<ioc::InitCommand>();
    init_cmd->Execute();

    LOG_INFO << "IoC container initialized";

    // ========== Читаем MongoDB настройки из конфигурации ==========
    auto mongodb_config = config["mongodb"];
    std::string connection_uri = mongodb_config["connection_uri"].asString();

    // Имя БД можно переопределить через переменную окружения MONGODB_DATABASE
    // Это позволяет тестам использовать LinksTest, а production - Links
    const char* env_db = std::getenv("MONGODB_DATABASE");
    std::string database_name = env_db ? env_db : mongodb_config["database"].asString();

    std::string collection_name = mongodb_config["collection"].asString();

    LOG_INFO << "MongoDB config: " << connection_uri << " / "
             << database_name << " / " << collection_name
             << (env_db ? " (from env)" : " (from config)");

    // ========== Singleton: MongoDB клиент ==========
    auto mongo_client = std::make_shared<mongocxx::client>(
        mongocxx::uri(connection_uri)
    );

    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Singleton",
        ioc::Args{
            std::string("MongoClient"),
            ioc::DependencyFactory([mongo_client](const ioc::Args&) -> std::any {
                return mongo_client;
            })
        }
    )->Execute();

    LOG_INFO << "Registered: MongoClient (Singleton)";

    // ========== Singleton: MongoDB коллекция ==========
    auto collection = mongo_client->database(database_name)
                                   .collection(collection_name);

    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Singleton",
        ioc::Args{
            std::string("MongoCollection"),
            ioc::DependencyFactory([collection](const ioc::Args&) -> std::any {
                return collection;
            })
        }
    )->Execute();

    LOG_INFO << "Registered: MongoCollection (Singleton)";

    // ========== Singleton: IJwtValidator ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Singleton",
        ioc::Args{
            std::string("IJwtValidator"),
            ioc::DependencyFactory([config](const ioc::Args&) -> std::any {
                // Получить настройки JWT из config.json через DI
                const char* env_jwks_uri = std::getenv("JWT_JWKS_URI");
                const char* env_issuer = std::getenv("JWT_ISSUER");
                const char* env_audience = std::getenv("JWT_AUDIENCE");

                std::string jwks_uri = env_jwks_uri
                    ? env_jwks_uri
                    : config.get("JWT", Json::Value()).get("JwksUri", "http://jwt-service:3000").asString();
                std::string issuer = env_issuer
                    ? env_issuer
                    : config.get("JWT", Json::Value()).get("Issuer", "smartlinks-jwt-service").asString();
                std::string audience = env_audience
                    ? env_audience
                    : config.get("JWT", Json::Value()).get("Audience", "smartlinks-api").asString();

                LOG_INFO << "JWT config: " << jwks_uri << " / " << issuer << " / " << audience
                         << (env_jwks_uri || env_issuer || env_audience ? " (from env)" : " (from config)");

                return std::static_pointer_cast<domain::IJwtValidator>(
                    std::make_shared<impl::JwtValidator>(jwks_uri, issuer, audience)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: IJwtValidator (Singleton)";

    // ========== Scoped: IMongoDbRepository ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Scoped",
        ioc::Args{
            std::string("IMongoDbRepository"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                auto collection = ioc::IoC::Resolve<mongocxx::collection>("MongoCollection");
                auto smart_link = ioc::IoC::Resolve<std::shared_ptr<domain::ISmartLink>>(
                    "ISmartLink"
                );
                return std::static_pointer_cast<domain::IMongoDbRepository>(
                    std::make_shared<impl::MongoDbRepository>(collection, smart_link)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: IMongoDbRepository (Scoped)";

    // ========== Scoped: IStatableSmartLinkRepository ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Scoped",
        ioc::Args{
            std::string("IStatableSmartLinkRepository"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                auto repo = ioc::IoC::Resolve<std::shared_ptr<domain::IMongoDbRepository>>(
                    "IMongoDbRepository"
                );
                return std::static_pointer_cast<domain::IStatableSmartLinkRepository>(
                    std::make_shared<impl::StatableSmartLinkRepository>(repo)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: IStatableSmartLinkRepository (Scoped)";

    // ========== Scoped: ISmartLinkRedirectRulesRepository ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Scoped",
        ioc::Args{
            std::string("ISmartLinkRedirectRulesRepository"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                auto repo = ioc::IoC::Resolve<std::shared_ptr<domain::IMongoDbRepository>>(
                    "IMongoDbRepository"
                );
                return std::static_pointer_cast<domain::ISmartLinkRedirectRulesRepository>(
                    std::make_shared<impl::SmartLinkRedirectRulesRepository>(repo)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: ISmartLinkRedirectRulesRepository (Scoped)";

    // ========== Scoped: IContext (для DSL правил) ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Scoped",
        ioc::Args{
            std::string("IContext"),
            ioc::DependencyFactory([](const ioc::Args& factory_args) -> std::any {
                // Извлечь аргументы (request и jwt_validator)
                auto request = std::any_cast<drogon::HttpRequestPtr>(factory_args[0]);
                auto jwt_validator = std::any_cast<std::shared_ptr<domain::IJwtValidator>>(factory_args[1]);

                // Создать пустой контекст
                auto context = std::make_shared<dsl::Context>();

                // Заполнить данными из request и jwt_validator
                // (логика из Context::FromHttpRequest)
                context->set("request", request);
                context->set("jwt_validator", jwt_validator);

                return std::static_pointer_cast<dsl::IContext>(context);
            })
        }
    )->Execute();

    LOG_INFO << "Registered: IContext (Scoped)";

    // ========== Scoped: ISmartLinkRedirectService (с поддержкой DSL) ==========
    // Переключение между DSL и JSON через переменную окружения USE_DSL_RULES
    // По умолчанию используется DSL, для отключения установите USE_DSL_RULES=false
    const char* use_dsl_env = std::getenv("USE_DSL_RULES");
    bool use_dsl_rules = !use_dsl_env || std::string(use_dsl_env) != "false";

    LOG_INFO << "Redirect rules mode: " << (use_dsl_rules ? "DSL" : "JSON (legacy)");

    if (use_dsl_rules) {
        // Регистрация DSL реализации (по умолчанию)
        ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
            "IoC.Register.Scoped",
            ioc::Args{
                std::string("ISmartLinkRedirectService"),
                ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                    // Resolve request и jwt_validator для передачи в IContext factory
                    auto request = ioc::IoC::Resolve<drogon::HttpRequestPtr>("HttpRequest");
                    auto jwt_validator = ioc::IoC::Resolve<std::shared_ptr<domain::IJwtValidator>>(
                        "IJwtValidator"
                    );

                    // Resolve IContext с аргументами
                    ioc::Args context_args;
                    context_args.push_back(request);
                    context_args.push_back(jwt_validator);
                    auto context = ioc::IoC::Resolve<std::shared_ptr<dsl::IContext>>(
                        "IContext",
                        context_args
                    );

                    // Resolve DSL repository
                    auto dsl_repo = ioc::IoC::Resolve<std::shared_ptr<domain::ISmartLinkRedirectRulesRepositoryDsl>>(
                        "ISmartLinkRedirectRulesRepositoryDsl"
                    );

                    return std::static_pointer_cast<domain::ISmartLinkRedirectService>(
                        std::make_shared<impl::SmartLinkRedirectServiceDsl>(
                            context, dsl_repo
                        )
                    );
                })
            }
        )->Execute();

        LOG_INFO << "Registered: ISmartLinkRedirectService (DSL implementation)";
    } else {
        // Регистрация JSON реализации (legacy)
        ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
            "IoC.Register.Scoped",
            ioc::Args{
                std::string("ISmartLinkRedirectService"),
                ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                    auto redirectable = ioc::IoC::Resolve<
                        std::shared_ptr<domain::IRedirectableSmartLink>
                    >("IRedirectableSmartLink");

                    auto rules_repo = ioc::IoC::Resolve<
                        std::shared_ptr<domain::ISmartLinkRedirectRulesRepository>
                    >("ISmartLinkRedirectRulesRepository");

                    return std::static_pointer_cast<domain::ISmartLinkRedirectService>(
                        std::make_shared<impl::SmartLinkRedirectService>(
                            redirectable, rules_repo
                        )
                    );
                })
            }
        )->Execute();

        LOG_INFO << "Registered: ISmartLinkRedirectService (JSON implementation)";
    }

    // ========== Scoped: IFreezeSmartLinkService ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Scoped",
        ioc::Args{
            std::string("IFreezeSmartLinkService"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                auto repository = ioc::IoC::Resolve<
                    std::shared_ptr<domain::IStatableSmartLinkRepository>
                >("IStatableSmartLinkRepository");

                return std::static_pointer_cast<domain::IFreezeSmartLinkService>(
                    std::make_shared<impl::FreezeSmartLinkService>(repository)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: IFreezeSmartLinkService (Scoped)";

    // ========== DSL Парсеры и плагины ==========

    // ========== Singleton: ParserRegistry ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Singleton",
        ioc::Args{
            std::string("ParserRegistry"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                return std::make_shared<dsl::parser::ParserRegistry>();
            })
        }
    )->Execute();

    LOG_INFO << "Registered: ParserRegistry (Singleton)";

    // ========== Команда Parser.Register ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("Parser.Register"),
            ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                auto name = std::any_cast<std::string>(args[0]);
                auto parser = std::any_cast<std::shared_ptr<dsl::parser::IParser>>(args[1]);

                return std::static_pointer_cast<ioc::ICommand>(
                    std::make_shared<dsl::parser::RegisterParserCommand>(name, parser)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: Parser.Register command";

    // ========== Команда Parser.Unregister ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("Parser.Unregister"),
            ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                auto name = std::any_cast<std::string>(args[0]);

                return std::static_pointer_cast<ioc::ICommand>(
                    std::make_shared<dsl::parser::UnregisterParserCommand>(name)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: Parser.Unregister command";

    // ========== Singleton: PluginLoader (загружает плагины при создании) ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Singleton",
        ioc::Args{
            std::string("PluginLoader"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                // Singleton pattern: создать экземпляр один раз и кэшировать
                static std::shared_ptr<dsl::plugin::PluginLoader> instance;
                static std::once_flag init_flag;

                std::call_once(init_flag, []() {
                    instance = std::make_shared<dsl::plugin::PluginLoader>();

                    // Загрузить плагины из директории plugins/
                    std::string plugin_dir = "./plugins";
                    LOG_INFO << "Loading DSL plugins from: " << plugin_dir;

                    try {
                        instance->LoadPluginsFromDirectory(plugin_dir);
                    } catch (const std::exception& e) {
                        LOG_ERROR << "Failed to load plugins: " << e.what();
                    }
                });

                return instance;
            })
        }
    )->Execute();

    LOG_INFO << "Registered: PluginLoader (Singleton)";

    // Инициализировать PluginLoader (загрузить плагины)
    ioc::IoC::Resolve<std::shared_ptr<dsl::plugin::PluginLoader>>("PluginLoader");

    // ========== Scoped: ICombinedParser ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Scoped",
        ioc::Args{
            std::string("ICombinedParser"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                LOG_INFO << "[CombinedParser Factory] Resolving ParserRegistry from IoC";
                auto registry = ioc::IoC::Resolve<std::shared_ptr<dsl::parser::ParserRegistry>>(
                    "ParserRegistry"
                );
                LOG_INFO << "[CombinedParser Factory] Got ParserRegistry: " << registry.get();

                LOG_INFO << "[CombinedParser Factory] Creating CombinedParser instance";
                auto parser = std::make_shared<dsl::parser::CombinedParser>(registry);
                LOG_INFO << "[CombinedParser Factory] CombinedParser created: " << parser.get();

                // Возвращаем указатель на интерфейс
                return std::static_pointer_cast<dsl::parser::ICombinedParser>(parser);
            })
        }
    )->Execute();

    LOG_INFO << "Registered: ICombinedParser (Scoped)";

    // ========== Scoped: ISmartLinkRedirectRulesRepositoryDsl ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Scoped",
        ioc::Args{
            std::string("ISmartLinkRedirectRulesRepositoryDsl"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                auto mongo_repo = ioc::IoC::Resolve<std::shared_ptr<domain::IMongoDbRepository>>(
                    "IMongoDbRepository"
                );
                auto parser = ioc::IoC::Resolve<std::shared_ptr<dsl::parser::ICombinedParser>>(
                    "ICombinedParser"
                );

                return std::static_pointer_cast<domain::ISmartLinkRedirectRulesRepositoryDsl>(
                    std::make_shared<impl::SmartLinkRedirectRulesRepositoryDsl>(mongo_repo, parser)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: ISmartLinkRedirectRulesRepositoryDsl (Scoped)";

    // ========== TRANSIENT: Middleware (создаются заново на каждый запрос!) ==========

    // MethodNotAllowedMiddleware
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("MethodNotAllowedMiddleware"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                auto supported = ioc::IoC::Resolve<
                    std::shared_ptr<domain::ISupportedHttpRequest>
                >("ISupportedHttpRequest");

                return std::static_pointer_cast<middleware::IMiddleware>(
                    std::make_shared<middleware::MethodNotAllowedMiddleware>(supported)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: MethodNotAllowedMiddleware (Transient)";

    // NotFoundMiddleware
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("NotFoundMiddleware"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                auto repository = ioc::IoC::Resolve<
                    std::shared_ptr<domain::IStatableSmartLinkRepository>
                >("IStatableSmartLinkRepository");

                return std::static_pointer_cast<middleware::IMiddleware>(
                    std::make_shared<middleware::NotFoundMiddleware>(repository)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: NotFoundMiddleware (Transient)";

    // UnprocessableContentMiddleware
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("UnprocessableContentMiddleware"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                auto freeze_service = ioc::IoC::Resolve<
                    std::shared_ptr<domain::IFreezeSmartLinkService>
                >("IFreezeSmartLinkService");

                return std::static_pointer_cast<middleware::IMiddleware>(
                    std::make_shared<middleware::UnprocessableContentMiddleware>(freeze_service)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: UnprocessableContentMiddleware (Transient)";

    // TemporaryRedirectMiddleware
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("TemporaryRedirectMiddleware"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                auto redirect_service = ioc::IoC::Resolve<
                    std::shared_ptr<domain::ISmartLinkRedirectService>
                >("ISmartLinkRedirectService");

                return std::static_pointer_cast<middleware::IMiddleware>(
                    std::make_shared<middleware::TemporaryRedirectMiddleware>(redirect_service)
                );
            })
        }
    )->Execute();

    LOG_INFO << "Registered: TemporaryRedirectMiddleware (Transient)";

    // ========== Регистрация ExceptionHandler ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("ExceptionHandler"),
            ioc::DependencyFactory([](const ioc::Args&) -> std::any {
                return std::function<void(
                    std::shared_ptr<middleware::IMiddleware>,
                    std::exception_ptr
                )>([](auto middleware, auto ex_ptr) {
                    try {
                        std::rethrow_exception(ex_ptr);
                    } catch (const std::exception& e) {
                        LOG_ERROR << "Middleware exception: " << e.what();
                    } catch (...) {
                        LOG_ERROR << "Unknown middleware exception";
                    }
                });
            })
        }
    )->Execute();

    LOG_INFO << "Registered: ExceptionHandler";

    // ========== Регистрация команды для получения middleware pipeline ==========
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("GetMiddlewarePipeline"),
            ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                try {
                    LOG_INFO << "[GetMiddlewarePipeline] Step A: Extracting middlewares reference";
                    // Получаем ссылку на вектор из аргументов
                    auto& middlewares = std::any_cast<std::reference_wrapper<
                        std::vector<std::shared_ptr<middleware::IMiddleware>>
                    >>(args[0]).get();
                    LOG_INFO << "[GetMiddlewarePipeline] Step A: Success";

                    // Создаём middleware в нужном порядке (как в C# Program.cs)
                    LOG_INFO << "[GetMiddlewarePipeline] Step B: Creating MethodNotAllowedMiddleware";
                    middlewares.push_back(
                        ioc::IoC::Resolve<std::shared_ptr<middleware::IMiddleware>>(
                            "MethodNotAllowedMiddleware"
                        )
                    );
                    LOG_INFO << "[GetMiddlewarePipeline] Step B: Success";

                    LOG_INFO << "[GetMiddlewarePipeline] Step C: Creating NotFoundMiddleware";
                    middlewares.push_back(
                        ioc::IoC::Resolve<std::shared_ptr<middleware::IMiddleware>>(
                            "NotFoundMiddleware"
                        )
                    );
                    LOG_INFO << "[GetMiddlewarePipeline] Step C: Success";

                    LOG_INFO << "[GetMiddlewarePipeline] Step D: Creating UnprocessableContentMiddleware";
                    middlewares.push_back(
                        ioc::IoC::Resolve<std::shared_ptr<middleware::IMiddleware>>(
                            "UnprocessableContentMiddleware"
                        )
                    );
                    LOG_INFO << "[GetMiddlewarePipeline] Step D: Success";

                    LOG_INFO << "[GetMiddlewarePipeline] Step E: Creating TemporaryRedirectMiddleware";
                    middlewares.push_back(
                        ioc::IoC::Resolve<std::shared_ptr<middleware::IMiddleware>>(
                            "TemporaryRedirectMiddleware"
                        )
                    );
                    LOG_INFO << "[GetMiddlewarePipeline] Step E: Success";

                    // Возвращаем команду-заглушку (фактически работа уже сделана)
                    LOG_INFO << "[GetMiddlewarePipeline] Step F: Returning NullCommand";
                    return std::static_pointer_cast<ioc::ICommand>(
                        std::make_shared<ioc::NullCommand>()
                    );
                } catch (const std::bad_any_cast& e) {
                    LOG_ERROR << "[GetMiddlewarePipeline] bad_any_cast: " << e.what();
                    throw;
                } catch (const std::exception& e) {
                    LOG_ERROR << "[GetMiddlewarePipeline] Exception: " << e.what();
                    throw;
                }
            })
        }
    )->Execute();

    LOG_INFO << "Registered: GetMiddlewarePipeline command";
}

int main() {
    try {
        // Инициализировать MongoDB driver (Singleton инстанс)
        mongocxx::instance instance{};

        LOG_INFO << "Starting SmartLinks C++ microservice...";

        // Загрузить конфигурацию из JSON
        auto config = LoadConfig("./config/config.json");

        LOG_INFO << "Configuration loaded";

        // Зарегистрировать зависимости в IoC
        RegisterDependencies(config);

        LOG_INFO << "All dependencies registered";

        // Настроить Drogon сервер
        auto app_config = config["app"];
        int port = app_config["port"].asInt();
        std::string host = app_config["host"].asString();
        int threads = app_config["threads_num"].asInt();

        drogon::app()
            .setLogLevel(trantor::Logger::kTrace)  // TRACE для детального логирования
            .addListener(host, port)
            .setThreadNum(threads)
            .enableSession(0);  // Отключить сессии (не нужны для API)

        LOG_INFO << "Drogon configured: " << host << ":" << port
                 << " with " << threads << " threads";

        // Зарегистрировать глобальный handler через DrogonMiddlewareAdapter
        drogon::app().registerHandler(
            "/{slug}",
            [](const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                // Создать adapter и обработать запрос
                middleware::DrogonMiddlewareAdapter adapter;
                // nextCb не используется, передаём пустую функцию
                std::function<void(const drogon::HttpResponsePtr&)> empty_next =
                    [](const drogon::HttpResponsePtr&){};
                adapter.invoke(req, std::move(empty_next), std::move(callback));
            },
            {drogon::Get, drogon::Head, drogon::Post, drogon::Put, drogon::Delete, drogon::Patch, drogon::Options}
        );

        LOG_INFO << "DrogonMiddlewareAdapter registered as global handler";

        // Запустить сервер
        LOG_INFO << "Starting HTTP server...";
        drogon::app().run();

    } catch (const std::exception& e) {
        LOG_ERROR << "Fatal error: " << e.what();
        return 1;
    } catch (...) {
        LOG_ERROR << "Unknown fatal error";
        return 1;
    }

    return 0;
}
