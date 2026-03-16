#pragma once

namespace smartlinks::ioc {

// Интерфейс команды (аналог C# ICommand)
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void Execute() = 0;
};

// Команда-заглушка (ничего не делает)
class NullCommand : public ICommand {
public:
    void Execute() override {}
};

} // namespace smartlinks::ioc
