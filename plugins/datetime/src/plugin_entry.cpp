#include "datetime_parser.hpp"
#include "i_time_point_accessible.hpp"
#include "context_2_time_point_accessible.hpp"
#include "ioc/ioc.hpp"
#include "ioc/command.hpp"
#include "ioc/scope.hpp"
#include "dsl/parser/i_parser.hpp"
#include "dsl/ast/datetime_expressions.hpp"
#include "dsl/context.hpp"
#include <memory>
#include <iostream>
#include <vector>

using namespace smartlinks;
using namespace smartlinks::dsl::plugins::datetime;
using smartlinks::dsl::ast::IBoolExpression;
using smartlinks::dsl::ast::DatetimeEqualExpression;
using smartlinks::dsl::ast::DatetimeNotEqualExpression;
using smartlinks::dsl::ast::DatetimeLessExpression;
using smartlinks::dsl::ast::DatetimeLessEqualExpression;
using smartlinks::dsl::ast::DatetimeGreaterExpression;
using smartlinks::dsl::ast::DatetimeGreaterEqualExpression;
using smartlinks::dsl::ast::DatetimeInExpression;

// Список операторов для регистрации/дерегистрации
static const std::vector<std::string> DATETIME_OPERATORS = {
    "=", "!=", "<", "<=", ">", ">=", "IN"
};

// Авто-регистрация при загрузке .so
extern "C" {

__attribute__((constructor))
void RegisterDatetimePlugin() {
    std::cout << "[DatetimePlugin] Registering AST node factories..." << std::endl;

    try {
        // Зарегистрировать фабрики для каждого оператора

        // DateTime.AST.Node.= - создает DatetimeEqualExpression
        {
            ioc::Args args;
            args.push_back(std::string("DateTime.AST.Node.="));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                // Первый аргумент - IContext
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(args[0]);
                // Второй аргумент - значение
                auto value = std::any_cast<time_t>(args[1]);

                // Resolve адаптера
                ioc::Args adapter_args;
                adapter_args.push_back(context);
                auto adapter = ioc::IoC::Resolve<std::shared_ptr<ITimePointAccessible>>(
                    "ITimePointAccessible",
                    adapter_args
                );

                // Инжект через конструктор
                return std::static_pointer_cast<IBoolExpression>(
                    std::make_shared<DatetimeEqualExpression>(adapter, value)
                );
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }

        // DateTime.AST.Node.!= - создает DatetimeNotEqualExpression
        {
            ioc::Args args;
            args.push_back(std::string("DateTime.AST.Node.!="));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(args[0]);
                auto value = std::any_cast<time_t>(args[1]);

                ioc::Args adapter_args;
                adapter_args.push_back(context);
                auto adapter = ioc::IoC::Resolve<std::shared_ptr<ITimePointAccessible>>(
                    "ITimePointAccessible",
                    adapter_args
                );

                return std::static_pointer_cast<IBoolExpression>(
                    std::make_shared<DatetimeNotEqualExpression>(adapter, value)
                );
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }

        // DateTime.AST.Node.< - создает DatetimeLessExpression
        {
            ioc::Args args;
            args.push_back(std::string("DateTime.AST.Node.<"));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(args[0]);
                auto value = std::any_cast<time_t>(args[1]);

                ioc::Args adapter_args;
                adapter_args.push_back(context);
                auto adapter = ioc::IoC::Resolve<std::shared_ptr<ITimePointAccessible>>(
                    "ITimePointAccessible",
                    adapter_args
                );

                return std::static_pointer_cast<IBoolExpression>(
                    std::make_shared<DatetimeLessExpression>(adapter, value)
                );
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }

        // DateTime.AST.Node.<= - создает DatetimeLessEqualExpression
        {
            ioc::Args args;
            args.push_back(std::string("DateTime.AST.Node.<="));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(args[0]);
                auto value = std::any_cast<time_t>(args[1]);

                ioc::Args adapter_args;
                adapter_args.push_back(context);
                auto adapter = ioc::IoC::Resolve<std::shared_ptr<ITimePointAccessible>>(
                    "ITimePointAccessible",
                    adapter_args
                );

                return std::static_pointer_cast<IBoolExpression>(
                    std::make_shared<DatetimeLessEqualExpression>(adapter, value)
                );
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }

        // DateTime.AST.Node.> - создает DatetimeGreaterExpression
        {
            ioc::Args args;
            args.push_back(std::string("DateTime.AST.Node.>"));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(args[0]);
                auto value = std::any_cast<time_t>(args[1]);

                ioc::Args adapter_args;
                adapter_args.push_back(context);
                auto adapter = ioc::IoC::Resolve<std::shared_ptr<ITimePointAccessible>>(
                    "ITimePointAccessible",
                    adapter_args
                );

                return std::static_pointer_cast<IBoolExpression>(
                    std::make_shared<DatetimeGreaterExpression>(adapter, value)
                );
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }

        // DateTime.AST.Node.>= - создает DatetimeGreaterEqualExpression
        {
            ioc::Args args;
            args.push_back(std::string("DateTime.AST.Node.>="));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(args[0]);
                auto value = std::any_cast<time_t>(args[1]);

                ioc::Args adapter_args;
                adapter_args.push_back(context);
                auto adapter = ioc::IoC::Resolve<std::shared_ptr<ITimePointAccessible>>(
                    "ITimePointAccessible",
                    adapter_args
                );

                return std::static_pointer_cast<IBoolExpression>(
                    std::make_shared<DatetimeGreaterEqualExpression>(adapter, value)
                );
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }

        // DateTime.AST.Node.IN - создает DatetimeInExpression
        {
            ioc::Args args;
            args.push_back(std::string("DateTime.AST.Node.IN"));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(args[0]);
                auto start = std::any_cast<time_t>(args[1]);
                auto end = std::any_cast<time_t>(args[2]);

                ioc::Args adapter_args;
                adapter_args.push_back(context);
                auto adapter = ioc::IoC::Resolve<std::shared_ptr<ITimePointAccessible>>(
                    "ITimePointAccessible",
                    adapter_args
                );

                return std::static_pointer_cast<IBoolExpression>(
                    std::make_shared<DatetimeInExpression>(adapter, start, end)
                );
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }

        std::cout << "[DatetimePlugin] AST node factories registered" << std::endl;

        // Зарегистрировать адаптер ITimePointAccessible (Transient)
        std::cout << "[DatetimePlugin] Registering ITimePointAccessible adapter..." << std::endl;
        {
            ioc::Args args;
            args.push_back(std::string("ITimePointAccessible"));
            args.push_back(ioc::DependencyFactory([](const ioc::Args& factory_args) -> std::any {
                // Извлечь IContext из аргументов фабрики
                auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(factory_args[0]);

                // Создать адаптер
                auto adapter = std::make_shared<Context2TimePointAccessible>(context);

                // КРИТИЧНО: привести к интерфейсу
                return std::static_pointer_cast<ITimePointAccessible>(adapter);
            }));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Register.Transient", args)->Execute();
        }
        std::cout << "[DatetimePlugin] ITimePointAccessible adapter registered" << std::endl;

        // Зарегистрировать парсер
        std::cout << "[DatetimePlugin] Registering parser..." << std::endl;

        auto parser = std::make_shared<dsl::plugins::datetime::DatetimeParser>();

        {
            ioc::Args args;
            args.push_back(std::string("Datetime"));
            args.push_back(std::static_pointer_cast<dsl::parser::IParser>(parser));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("Parser.Register", args)->Execute();
        }

        std::cout << "[DatetimePlugin] Parser registered successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DatetimePlugin] Registration failed: " << e.what() << std::endl;
    }
}

__attribute__((destructor))
void UnregisterDatetimePlugin() {
    std::cout << "[DatetimePlugin] Unregistering..." << std::endl;

    try {
        // Дерегистрировать парсер
        {
            ioc::Args args;
            args.push_back(std::string("Datetime"));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("Parser.Unregister", args)->Execute();
        }

        std::cout << "[DatetimePlugin] Parser unregistered" << std::endl;

        // Дерегистрировать адаптер
        {
            ioc::Args args;
            args.push_back(std::string("ITimePointAccessible"));
            ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Unregister", args)->Execute();
        }
        std::cout << "[DatetimePlugin] ITimePointAccessible adapter unregistered" << std::endl;

        // Дерегистрировать AST node фабрики
        for (const auto& op : DATETIME_OPERATORS) {
            try {
                std::string key = "DateTime.AST.Node." + op;

                ioc::Args args;
                args.push_back(key);
                ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>("IoC.Unregister", args)->Execute();

                std::cout << "[DatetimePlugin] Unregistered factory: " << key << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[DatetimePlugin] Failed to unregister factory " << op
                          << ": " << e.what() << std::endl;
            }
        }

        std::cout << "[DatetimePlugin] Unregistered successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DatetimePlugin] Unregistration failed: " << e.what() << std::endl;
    }
}

} // extern "C"
