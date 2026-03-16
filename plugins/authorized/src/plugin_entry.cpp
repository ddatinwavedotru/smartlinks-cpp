#include "authorized_parser.hpp"
#include "i_authorized_accessible.hpp"
#include "context_2_authorized_accessible.hpp"
#include "ioc/ioc.hpp"
#include "ioc/command.hpp"
#include "ioc/scope.hpp"
#include "dsl/parser/i_parser.hpp"
#include "dsl/context.hpp"
#include <memory>
#include <iostream>

using namespace smartlinks;
using namespace smartlinks::dsl::plugins::authorized;

// Авто-регистрация при загрузке .so
extern "C" {

__attribute__((constructor))
void RegisterAuthorizedPlugin() {
    std::cout << "[AuthorizedPlugin] Registering..." << std::endl;

    try {
        // Зарегистрировать адаптер IAuthorizedAccessible (Transient)
        std::cout << "[AuthorizedPlugin] Registering IAuthorizedAccessible adapter..." << std::endl;
        {
            ioc::Args args;
            args.push_back(std::string("IAuthorizedAccessible"));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& factory_args) -> std::any {
                // Извлечь IContext из аргументов фабрики
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(factory_args[0]);

                // Создать адаптер
                using namespace dsl::plugins::authorized;
                auto adapter = std::make_shared<Context2AuthorizedAccessible>(context);

                // КРИТИЧНО: привести к интерфейсу
                return std::static_pointer_cast<IAuthorizedAccessible>(adapter);
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }
        std::cout << "[AuthorizedPlugin] IAuthorizedAccessible adapter registered" << std::endl;

        // Зарегистрировать AST node фабрику
        std::cout << "[AuthorizedPlugin] Registering AST node factory..." << std::endl;

        // Authorized.AST.Node
        {
            ioc::Args args;
            args.push_back(std::string("Authorized.AST.Node"));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(args[0]);

                ioc::Args adapter_args;
                adapter_args.push_back(context);
                auto adapter = ioc::IoC::Resolve<std::shared_ptr<IAuthorizedAccessible>>(
                    "IAuthorizedAccessible",
                    adapter_args
                );

                return std::static_pointer_cast<dsl::ast::IBoolExpression>(
                    std::make_shared<dsl::ast::AuthorizedExpression>(adapter)
                );
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }

        std::cout << "[AuthorizedPlugin] AST node factory registered" << std::endl;

        // Зарегистрировать парсер
        std::cout << "[AuthorizedPlugin] Registering parser..." << std::endl;
        auto parser = std::make_shared<dsl::plugins::authorized::AuthorizedParser>();

        // Получить команду регистрации через IoC
        auto command = ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
            "Parser.Register",
            ioc::Args{
                std::string("Authorized"),
                std::static_pointer_cast<dsl::parser::IParser>(parser)
            }
        );

        // Выполнить регистрацию
        command->Execute();

        std::cout << "[AuthorizedPlugin] Parser registered successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AuthorizedPlugin] Registration failed: " << e.what() << std::endl;
    }
}

__attribute__((destructor))
void UnregisterAuthorizedPlugin() {
    std::cout << "[AuthorizedPlugin] Unregistering..." << std::endl;

    try {
        // Дерегистрировать парсер
        // Получить команду дерегистрации через IoC
        auto command = ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
            "Parser.Unregister",
            ioc::Args{std::string("Authorized")}
        );

        // Выполнить дерегистрацию
        command->Execute();

        std::cout << "[AuthorizedPlugin] Parser unregistered" << std::endl;

        // Дерегистрировать адаптер
        {
            ioc::Args args;
            args.push_back(std::string("IAuthorizedAccessible"));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Unregister", args)->Execute();
        }
        std::cout << "[AuthorizedPlugin] IAuthorizedAccessible adapter unregistered" << std::endl;

        // Дерегистрировать AST node фабрику
        {
            ioc::Args args;
            args.push_back(std::string("Authorized.AST.Node"));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Unregister", args)->Execute();
        }
        std::cout << "[AuthorizedPlugin] AST node factory unregistered" << std::endl;

        std::cout << "[AuthorizedPlugin] Unregistered successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AuthorizedPlugin] Unregistration failed: " << e.what() << std::endl;
    }
}

} // extern "C"
