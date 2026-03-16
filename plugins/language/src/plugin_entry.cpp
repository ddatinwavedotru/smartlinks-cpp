#include "language_parser.hpp"
#include "i_language_accessible.hpp"
#include "context_2_language_accessible.hpp"
#include "ioc/ioc.hpp"
#include "ioc/command.hpp"
#include "ioc/scope.hpp"
#include "dsl/parser/i_parser.hpp"
#include "dsl/context.hpp"
#include <memory>
#include <iostream>
#include <vector>

using namespace smartlinks;
using namespace smartlinks::dsl::plugins::language;

// Список операторов для регистрации/дерегистрации
static const std::vector<std::string> LANGUAGE_OPERATORS = {"=", "!="};

// Авто-регистрация при загрузке .so
extern "C" {

__attribute__((constructor))
void RegisterLanguagePlugin() {
    std::cout << "[LanguagePlugin] Registering..." << std::endl;

    try {
        // Зарегистрировать адаптер ILanguageAccessible (Transient)
        std::cout << "[LanguagePlugin] Registering ILanguageAccessible adapter..." << std::endl;
        {
            ioc::Args args;
            args.push_back(std::string("ILanguageAccessible"));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& factory_args) -> std::any {
                // Извлечь IContext из аргументов фабрики
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(factory_args[0]);

                // Создать адаптер
                using namespace dsl::plugins::language;
                auto adapter = std::make_shared<Context2LanguageAccessible>(context);

                // КРИТИЧНО: привести к интерфейсу
                return std::static_pointer_cast<ILanguageAccessible>(adapter);
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }
        std::cout << "[LanguagePlugin] ILanguageAccessible adapter registered" << std::endl;

        // Зарегистрировать AST node фабрики
        std::cout << "[LanguagePlugin] Registering AST node factories..." << std::endl;

        // Language.AST.Node.=
        {
            ioc::Args args;
            args.push_back(std::string("Language.AST.Node.="));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(args[0]);
                auto value = std::any_cast<std::string>(args[1]);

                ioc::Args adapter_args;
                adapter_args.push_back(context);
                auto adapter = ioc::IoC::Resolve<std::shared_ptr<ILanguageAccessible>>(
                    "ILanguageAccessible",
                    adapter_args
                );

                return std::static_pointer_cast<dsl::ast::IBoolExpression>(
                    std::make_shared<dsl::ast::LanguageEqualExpression>(adapter, value)
                );
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }

        // Language.AST.Node.!=
        {
            ioc::Args args;
            args.push_back(std::string("Language.AST.Node.!="));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(args[0]);
                auto value = std::any_cast<std::string>(args[1]);

                ioc::Args adapter_args;
                adapter_args.push_back(context);
                auto adapter = ioc::IoC::Resolve<std::shared_ptr<ILanguageAccessible>>(
                    "ILanguageAccessible",
                    adapter_args
                );

                return std::static_pointer_cast<dsl::ast::IBoolExpression>(
                    std::make_shared<dsl::ast::LanguageNotEqualExpression>(adapter, value)
                );
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }

        std::cout << "[LanguagePlugin] AST node factories registered" << std::endl;

        // Зарегистрировать парсер
        std::cout << "[LanguagePlugin] Registering parser..." << std::endl;
        auto parser = std::make_shared<dsl::plugins::language::LanguageParser>();

        auto command = ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
            "Parser.Register",
            ioc::Args{
                std::string("Language"),
                std::static_pointer_cast<dsl::parser::IParser>(parser)
            }
        );

        command->Execute();

        std::cout << "[LanguagePlugin] Parser registered successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[LanguagePlugin] Registration failed: " << e.what() << std::endl;
    }
}

__attribute__((destructor))
void UnregisterLanguagePlugin() {
    std::cout << "[LanguagePlugin] Unregistering..." << std::endl;

    try {
        // Дерегистрировать парсер
        auto command = ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
            "Parser.Unregister",
            ioc::Args{std::string("Language")}
        );

        command->Execute();

        std::cout << "[LanguagePlugin] Parser unregistered" << std::endl;

        // Дерегистрировать адаптер
        {
            ioc::Args args;
            args.push_back(std::string("ILanguageAccessible"));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Unregister", args)->Execute();
        }
        std::cout << "[LanguagePlugin] ILanguageAccessible adapter unregistered" << std::endl;

        // Дерегистрировать AST node фабрики
        for (const auto& op : LANGUAGE_OPERATORS) {
            try {
                std::string key = "Language.AST.Node." + op;

                ioc::Args args;
                args.push_back(key);
                ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Unregister", args)->Execute();

                std::cout << "[LanguagePlugin] Unregistered factory: " << key << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[LanguagePlugin] Failed to unregister factory " << op
                          << ": " << e.what() << std::endl;
            }
        }

        std::cout << "[LanguagePlugin] Unregistered successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[LanguagePlugin] Unregistration failed: " << e.what() << std::endl;
    }
}

} // extern "C"
