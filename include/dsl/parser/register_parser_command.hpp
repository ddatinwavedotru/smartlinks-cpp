#pragma once

#include "ioc/command.hpp"
#include "ioc/ioc.hpp"
#include "parser_registry.hpp"
#include "i_parser.hpp"
#include <memory>
#include <string>

namespace smartlinks::dsl::parser {

/**
 * @brief Команда регистрации парсера в реестре
 *
 * Вызывается из плагинов при загрузке (__attribute__((constructor))):
 * IoC::Resolve("Parser.Register", name, parser)->Execute()
 */
class RegisterParserCommand : public ioc::ICommand {
private:
    std::string parser_name_;
    std::shared_ptr<IParser> parser_;

public:
    RegisterParserCommand(std::string name, std::shared_ptr<IParser> parser)
        : parser_name_(std::move(name))
        , parser_(std::move(parser)) {}

    void Execute() override {
        // Получить ParserRegistry из IoC
        auto registry = ioc::IoC::Resolve<std::shared_ptr<ParserRegistry>>(
            "ParserRegistry"
        );

        // Зарегистрировать парсер
        registry->RegisterLeafParser(parser_name_, parser_);
    }
};

/**
 * @brief Команда дерегистрации парсера из реестра
 *
 * Вызывается из плагинов при выгрузке (__attribute__((destructor))):
 * IoC::Resolve("Parser.Unregister", name)->Execute()
 */
class UnregisterParserCommand : public ioc::ICommand {
private:
    std::string parser_name_;

public:
    explicit UnregisterParserCommand(std::string name)
        : parser_name_(std::move(name)) {}

    void Execute() override {
        // Получить ParserRegistry из IoC
        auto registry = ioc::IoC::Resolve<std::shared_ptr<ParserRegistry>>(
            "ParserRegistry"
        );

        // Дерегистрировать парсер
        registry->UnregisterLeafParser(parser_name_);
    }
};

} // namespace smartlinks::dsl::parser
