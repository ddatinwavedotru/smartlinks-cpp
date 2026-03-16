#include <gtest/gtest.h>
#include "ioc/ioc.hpp"
#include "ioc/init_command.hpp"
#include "ioc/command.hpp"
#include "ioc/scope.hpp"
#include "ioc/dependency_resolver.hpp"
#include "ioc/register_transient_dependency_command.hpp"
#include "ioc/register_scoped_dependency_command.hpp"
#include "ioc/register_singleton_dependency_command.hpp"
#include "ioc/set_current_scope_command.hpp"
#include "ioc/clear_current_scope_command.hpp"
#include <memory>
#include <string>
#include <thread>

using namespace smartlinks::ioc;

// Тестовый класс для проверки DI
class TestService {
private:
    std::string value_;
public:
    explicit TestService(const std::string& value) : value_(value) {}
    std::string GetValue() const { return value_; }
};

class IoCTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Инициализировать IoC перед каждым тестом
        auto init_cmd = std::make_shared<InitCommand>();
        init_cmd->Execute();
    }

    void TearDown() override {
        // Очистить текущий scope после каждого теста
        Scope::ClearCurrent();
    }
};

// ========== Тест 1: Инициализация IoC ==========
TEST_F(IoCTest, InitCommand_InitializesRootScope) {
    // Проверяем, что root scope был инициализирован
    auto root = Scope::GetRoot();
    ASSERT_NE(root, nullptr);
    EXPECT_FALSE(root->empty());
}

// ========== Тест 2: Базовые зависимости после инициализации ==========
TEST_F(IoCTest, InitCommand_RegistersBaseDependencies) {
    // Проверяем наличие базовых зависимостей
    EXPECT_NO_THROW({
        auto scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create.Empty");
    });

    EXPECT_NO_THROW({
        auto scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
    });

    EXPECT_NO_THROW({
        auto current = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Current");
    });
}

// ========== Тест 3: Регистрация зависимости через IoC.Register ==========
TEST_F(IoCTest, RegisterDependency_ViaIoCRegister_Success) {
    // Регистрируем зависимость через IoC.Register (паттерн из плана)
    DependencyFactory test_factory = [](const Args&) -> std::any {
        return std::make_shared<TestService>("test_value");
    };

    Args test_args{
        std::string("TestService"),
        test_factory
    };

    auto cmd = IoC::Resolve<std::shared_ptr<ICommand>>("IoC.Register.Transient", test_args);
    cmd->Execute();

    // Проверяем, что можем разрешить зависимость
    auto service = IoC::Resolve<std::shared_ptr<TestService>>("TestService");
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->GetValue(), "test_value");
}

// ========== Тест 4: Регистрация с аргументами ==========
TEST_F(IoCTest, RegisterDependency_WithArguments_Success) {
    // Регистрируем фабрику, которая использует аргументы
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Transient",
        Args{
            std::string("TestServiceWithArgs"),
            DependencyFactory([](const Args& args) -> std::any {
                auto value = std::any_cast<std::string>(args[0]);
                return std::make_shared<TestService>(value);
            })
        }
    )->Execute();

    // Разрешаем зависимость с аргументами
    auto service = IoC::Resolve<std::shared_ptr<TestService>>(
        "TestServiceWithArgs",
        Args{std::string("dynamic_value")}
    );
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->GetValue(), "dynamic_value");
}

// ========== Тест 5: Создание нового scope ==========
TEST_F(IoCTest, CreateScope_CreatesNewScope) {
    auto scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
    ASSERT_NE(scope, nullptr);

    // Новый scope должен содержать ссылку на parent
    EXPECT_TRUE(scope->contains("IoC.Scope.Parent"));
}

// ========== Тест 6: Установка текущего scope ==========
TEST_F(IoCTest, SetCurrentScope_UpdatesThreadLocalScope) {
    // Создаём новый scope
    auto new_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");

    // Устанавливаем как текущий
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Scope.Current.Set",
        Args{new_scope}
    )->Execute();

    // Проверяем, что текущий scope обновился
    auto current = Scope::GetCurrent();
    EXPECT_EQ(current, new_scope);
}

// ========== Тест 7: Очистка текущего scope ==========
TEST_F(IoCTest, ClearCurrentScope_ResetsThreadLocalScope) {
    // Создаём и устанавливаем scope
    auto new_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Scope.Current.Set",
        Args{new_scope}
    )->Execute();

    // Очищаем
    IoC::Resolve<std::shared_ptr<ICommand>>("IoC.Scope.Current.Clear")->Execute();

    // Проверяем, что current scope теперь nullptr
    auto current = Scope::GetCurrent();
    EXPECT_EQ(current, nullptr);
}

// ========== Тест 8: Иерархическое разрешение зависимостей ==========
TEST_F(IoCTest, HierarchicalResolution_ResolvesFromParent) {
    // Регистрируем зависимость в root scope
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Transient",
        Args{
            std::string("RootService"),
            DependencyFactory([](const Args&) -> std::any {
                return std::make_shared<TestService>("from_root");
            })
        }
    )->Execute();

    // Создаём child scope и устанавливаем как текущий
    auto child_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Scope.Current.Set",
        Args{child_scope}
    )->Execute();

    // Регистрируем другую зависимость в child scope
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Transient",
        Args{
            std::string("ChildService"),
            DependencyFactory([](const Args&) -> std::any {
                return std::make_shared<TestService>("from_child");
            })
        }
    )->Execute();

    // Должны разрешиться обе зависимости
    auto root_service = IoC::Resolve<std::shared_ptr<TestService>>("RootService");
    EXPECT_EQ(root_service->GetValue(), "from_root");

    auto child_service = IoC::Resolve<std::shared_ptr<TestService>>("ChildService");
    EXPECT_EQ(child_service->GetValue(), "from_child");
}

// ========== Тест 9: Child scope переопределяет parent ==========
TEST_F(IoCTest, ChildScope_OverridesParentDependency) {
    // Регистрируем зависимость в root
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Transient",
        Args{
            std::string("Service"),
            DependencyFactory([](const Args&) -> std::any {
                return std::make_shared<TestService>("parent_version");
            })
        }
    )->Execute();

    // Создаём child scope
    auto child_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Scope.Current.Set",
        Args{child_scope}
    )->Execute();

    // Переопределяем ту же зависимость в child
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Transient",
        Args{
            std::string("Service"),
            DependencyFactory([](const Args&) -> std::any {
                return std::make_shared<TestService>("child_version");
            })
        }
    )->Execute();

    // Должна разрешиться версия из child scope
    auto service = IoC::Resolve<std::shared_ptr<TestService>>("Service");
    EXPECT_EQ(service->GetValue(), "child_version");
}

// ========== Тест 10: Ошибка при отсутствии зависимости ==========
TEST_F(IoCTest, Resolve_NonExistentDependency_ThrowsException) {
    EXPECT_THROW({
        IoC::Resolve<std::shared_ptr<TestService>>("NonExistentService");
    }, std::runtime_error);
}

// ========== Тест 11: Thread-local изоляция scopes ==========
TEST_F(IoCTest, ThreadLocalScope_IsIsolatedBetweenThreads) {
    // Создаём scope в главном потоке
    auto main_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Scope.Current.Set",
        Args{main_scope}
    )->Execute();

    // Регистрируем зависимость в main scope
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Transient",
        Args{
            std::string("ThreadService"),
            DependencyFactory([](const Args&) -> std::any {
                return std::make_shared<TestService>("main_thread");
            })
        }
    )->Execute();

    // Запускаем другой поток
    std::thread worker([&]() {
        // В новом потоке current scope должен быть nullptr
        auto current = Scope::GetCurrent();
        EXPECT_EQ(current, nullptr);

        // Создаём отдельный scope для этого потока
        auto thread_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
        IoC::Resolve<std::shared_ptr<ICommand>>(
            "IoC.Scope.Current.Set",
            Args{thread_scope}
        )->Execute();

        // Регистрируем зависимость в thread scope
        IoC::Resolve<std::shared_ptr<ICommand>>(
            "IoC.Register.Transient",
            Args{
                std::string("ThreadService"),
                DependencyFactory([](const Args&) -> std::any {
                    return std::make_shared<TestService>("worker_thread");
                })
            }
        )->Execute();

        // Разрешаем - должна быть версия из worker потока
        auto service = IoC::Resolve<std::shared_ptr<TestService>>("ThreadService");
        EXPECT_EQ(service->GetValue(), "worker_thread");
    });

    worker.join();

    // В main потоке должна остаться версия main
    auto service = IoC::Resolve<std::shared_ptr<TestService>>("ThreadService");
    EXPECT_EQ(service->GetValue(), "main_thread");
}

// ========== Тест 12: IoC.Scope.Current возвращает root если current == nullptr ==========
TEST_F(IoCTest, GetCurrentScope_ReturnsRootWhenCurrentIsNull) {
    // Убедимся, что current scope == nullptr
    Scope::ClearCurrent();

    // IoC.Scope.Current должен вернуть root
    auto current = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Current");
    auto root = Scope::GetRoot();

    EXPECT_EQ(current, root);
}

// ========== Тест 13: Создание пустого scope ==========
TEST_F(IoCTest, CreateEmptyScope_DoesNotHaveParent) {
    auto empty_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create.Empty");
    ASSERT_NE(empty_scope, nullptr);

    // Пустой scope НЕ должен содержать parent
    EXPECT_TRUE(!empty_scope->contains("IoC.Scope.Parent"));
}

// ========== Тест 14: Создание scope с явным parent ==========
TEST_F(IoCTest, CreateScope_WithExplicitParent) {
    // Создаём первый scope
    auto parent_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");

    // Создаём child scope с явным parent
    auto child_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>(
        "IoC.Scope.Create",
        Args{parent_scope}
    );

    // Проверяем, что parent установлен
    ASSERT_TRUE(child_scope->contains("IoC.Scope.Parent"));

    auto parent_factory_opt = child_scope->get("IoC.Scope.Parent");
    ASSERT_TRUE(parent_factory_opt.has_value());
    auto resolved_parent = std::any_cast<std::shared_ptr<IScopeDict>>(
        parent_factory_opt.value()({})
    );
    EXPECT_EQ(resolved_parent, parent_scope);
}

// ========== Тест 15: NullCommand не делает ничего ==========
TEST_F(IoCTest, NullCommand_DoesNothing) {
    auto cmd = std::make_shared<NullCommand>();
    EXPECT_NO_THROW(cmd->Execute());
}

// ========== Тест 16: DependencyResolver прямая проверка ==========
TEST_F(IoCTest, DependencyResolver_DirectTest) {
    auto scope = std::make_shared<ScopeDict>();

    // Регистрируем зависимость напрямую в scope
    scope->set("DirectService", [](const Args&) -> std::any {
        return std::make_shared<TestService>("direct");
    });

    // Используем DependencyResolver
    DependencyResolver resolver(scope);
    auto result = resolver.Resolve("DirectService", {});

    auto service = std::any_cast<std::shared_ptr<TestService>>(result);
    EXPECT_EQ(service->GetValue(), "direct");
}

// ========== Тест 17: RegisterTransientDependencyCommand в текущий scope ==========
TEST_F(IoCTest, RegisterTransientDependencyCommand_RegistersInCurrentScope) {
    // Создаём новый scope и устанавливаем как текущий
    auto new_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Scope.Current.Set",
        Args{new_scope}
    )->Execute();

    // Создаём команду регистрации напрямую
    auto cmd = std::make_shared<RegisterTransientDependencyCommand>(
        "DirectRegistered",
        DependencyFactory([](const Args&) -> std::any {
            return std::make_shared<TestService>("direct_registered");
        })
    );
    cmd->Execute();

    // Проверяем, что зависимость зарегистрирована в new_scope
    EXPECT_TRUE(new_scope->contains("DirectRegistered"));

    auto service = IoC::Resolve<std::shared_ptr<TestService>>("DirectRegistered");
    EXPECT_EQ(service->GetValue(), "direct_registered");
}

// ========== Тест 18: Двойная инициализация InitCommand безопасна ==========
TEST_F(IoCTest, InitCommand_DoubleInitializationIsSafe) {
    auto init1 = std::make_shared<InitCommand>();
    auto init2 = std::make_shared<InitCommand>();

    EXPECT_NO_THROW({
        init1->Execute();
        init2->Execute(); // Не должна повторно инициализировать
    });

    // Root scope должен остаться корректным
    auto root = Scope::GetRoot();
    EXPECT_NE(root, nullptr);
}

// ========== Тест 19: Resolve с неправильным типом бросает std::bad_any_cast ==========
TEST_F(IoCTest, Resolve_WrongType_ThrowsBadAnyCast) {
    // Регистрируем зависимость, возвращающую int
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Transient",
        Args{
            std::string("IntDependency"),
            DependencyFactory([](const Args&) -> std::any {
                return 42;  // Возвращаем int
            })
        }
    )->Execute();

    // Попытка разрешить как std::string должна бросить std::bad_any_cast
    EXPECT_THROW({
        auto wrong = IoC::Resolve<std::string>("IntDependency");
    }, std::bad_any_cast);
}

// ========== Тест 20: Resolve с неправильным типом указателя ==========
TEST_F(IoCTest, Resolve_WrongPointerType_ThrowsBadAnyCast) {
    // Регистрируем зависимость, возвращающую shared_ptr<TestService>
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Transient",
        Args{
            std::string("ServiceDependency"),
            DependencyFactory([](const Args&) -> std::any {
                return std::make_shared<TestService>("value");
            })
        }
    )->Execute();

    // Попытка разрешить как int должна бросить std::bad_any_cast
    EXPECT_THROW({
        auto wrong = IoC::Resolve<int>("ServiceDependency");
    }, std::bad_any_cast);
}

// ========================================
// Тесты для Singleton lifetime
// ========================================

// ========== Тест 21: Singleton создается один раз ==========
TEST_F(IoCTest, Singleton_CreatesOnlyOneInstance) {
    int creation_count = 0;

    // Регистрируем Singleton с счетчиком создания
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Singleton",
        Args{
            std::string("SingletonService"),
            DependencyFactory([&creation_count](const Args&) -> std::any {
                creation_count++;
                return std::make_shared<TestService>("singleton_value");
            })
        }
    )->Execute();

    // Разрешаем несколько раз
    auto instance1 = IoC::Resolve<std::shared_ptr<TestService>>("SingletonService");
    auto instance2 = IoC::Resolve<std::shared_ptr<TestService>>("SingletonService");
    auto instance3 = IoC::Resolve<std::shared_ptr<TestService>>("SingletonService");

    // Фабрика должна быть вызвана только один раз
    EXPECT_EQ(creation_count, 1);

    // Все экземпляры должны быть одним и тем же объектом
    EXPECT_EQ(instance1, instance2);
    EXPECT_EQ(instance2, instance3);
    EXPECT_EQ(instance1->GetValue(), "singleton_value");
}

// ========== Тест 22: Singleton разделяется между scopes ==========
TEST_F(IoCTest, Singleton_SharedBetweenScopes) {
    int creation_count = 0;

    // Регистрируем Singleton в root scope
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Singleton",
        Args{
            std::string("SharedSingleton"),
            DependencyFactory([&creation_count](const Args&) -> std::any {
                creation_count++;
                return std::make_shared<TestService>("shared");
            })
        }
    )->Execute();

    // Разрешаем в root scope
    auto instance1 = IoC::Resolve<std::shared_ptr<TestService>>("SharedSingleton");

    // Создаем child scope
    auto child_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Scope.Current.Set",
        Args{child_scope}
    )->Execute();

    // Разрешаем в child scope
    auto instance2 = IoC::Resolve<std::shared_ptr<TestService>>("SharedSingleton");

    // Должен быть создан только один раз
    EXPECT_EQ(creation_count, 1);
    // Должны быть одним и тем же объектом
    EXPECT_EQ(instance1, instance2);
}

// ========== Тест 23: Singleton thread-safe инициализация ==========
TEST_F(IoCTest, Singleton_ThreadSafeInitialization) {
    int creation_count = 0;
    std::mutex count_mutex;

    // Регистрируем Singleton
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Singleton",
        Args{
            std::string("ThreadSafeSingleton"),
            DependencyFactory([&creation_count, &count_mutex](const Args&) -> std::any {
                std::lock_guard<std::mutex> lock(count_mutex);
                creation_count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Имитация долгой инициализации
                return std::make_shared<TestService>("thread_safe");
            })
        }
    )->Execute();

    // Разрешаем из нескольких потоков одновременно
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<TestService>> instances(10);

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&instances, i]() {
            instances[i] = IoC::Resolve<std::shared_ptr<TestService>>("ThreadSafeSingleton");
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Фабрика должна быть вызвана только один раз
    EXPECT_EQ(creation_count, 1);

    // Все экземпляры должны быть одним и тем же объектом
    for (int i = 1; i < 10; ++i) {
        EXPECT_EQ(instances[0], instances[i]);
    }
}

// ========================================
// Тесты для Scoped lifetime
// ========================================

// ========== Тест 24: Scoped кэширует в пределах scope ==========
TEST_F(IoCTest, Scoped_CachesWithinScope) {
    int creation_count = 0;

    // Регистрируем Scoped
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Scoped",
        Args{
            std::string("ScopedService"),
            DependencyFactory([&creation_count](const Args&) -> std::any {
                creation_count++;
                return std::make_shared<TestService>("scoped_value");
            })
        }
    )->Execute();

    // Разрешаем несколько раз в одном scope
    auto instance1 = IoC::Resolve<std::shared_ptr<TestService>>("ScopedService");
    auto instance2 = IoC::Resolve<std::shared_ptr<TestService>>("ScopedService");
    auto instance3 = IoC::Resolve<std::shared_ptr<TestService>>("ScopedService");

    // Фабрика должна быть вызвана только один раз (кэшировано в scope)
    EXPECT_EQ(creation_count, 1);

    // Все экземпляры должны быть одним и тем же объектом
    EXPECT_EQ(instance1, instance2);
    EXPECT_EQ(instance2, instance3);
}

// ========== Тест 25: Scoped создает новый экземпляр для каждого scope ==========
TEST_F(IoCTest, Scoped_CreatesNewInstancePerScope) {
    int creation_count = 0;

    // Регистрируем Scoped в root scope
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Scoped",
        Args{
            std::string("ScopedPerRequest"),
            DependencyFactory([&creation_count](const Args&) -> std::any {
                creation_count++;
                return std::make_shared<TestService>("scoped_" + std::to_string(creation_count));
            })
        }
    )->Execute();

    // Разрешаем в root scope
    auto instance1 = IoC::Resolve<std::shared_ptr<TestService>>("ScopedPerRequest");
    EXPECT_EQ(creation_count, 1);

    // Создаем первый child scope
    auto scope1 = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Scope.Current.Set",
        Args{scope1}
    )->Execute();

    // Разрешаем в scope1 - должен создаться новый экземпляр
    auto instance2 = IoC::Resolve<std::shared_ptr<TestService>>("ScopedPerRequest");
    EXPECT_EQ(creation_count, 2);
    EXPECT_NE(instance1, instance2); // Разные объекты

    // Разрешаем еще раз в scope1 - должен вернуться кэшированный
    auto instance2_again = IoC::Resolve<std::shared_ptr<TestService>>("ScopedPerRequest");
    EXPECT_EQ(creation_count, 2); // Счетчик не увеличился
    EXPECT_EQ(instance2, instance2_again); // Тот же объект

    // Создаем второй child scope
    auto scope2 = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Scope.Current.Set",
        Args{scope2}
    )->Execute();

    // Разрешаем в scope2 - должен создаться третий экземпляр
    auto instance3 = IoC::Resolve<std::shared_ptr<TestService>>("ScopedPerRequest");
    EXPECT_EQ(creation_count, 3);
    EXPECT_NE(instance1, instance3);
    EXPECT_NE(instance2, instance3);
}

// ========== Тест 26: Scoped изолирован между потоками ==========
TEST_F(IoCTest, Scoped_IsolatedBetweenThreads) {
    int creation_count = 0;
    std::mutex count_mutex;

    // Регистрируем Scoped в root
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Scoped",
        Args{
            std::string("ThreadScopedService"),
            DependencyFactory([&creation_count, &count_mutex](const Args&) -> std::any {
                std::lock_guard<std::mutex> lock(count_mutex);
                creation_count++;
                return std::make_shared<TestService>("thread_scoped");
            })
        }
    )->Execute();

    // Разрешаем в главном потоке (root scope)
    auto main_instance = IoC::Resolve<std::shared_ptr<TestService>>("ThreadScopedService");
    EXPECT_EQ(creation_count, 1);

    // Создаем поток с отдельным scope
    std::thread worker([&creation_count]() {
        // Создаем scope для worker потока
        auto worker_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
        IoC::Resolve<std::shared_ptr<ICommand>>(
            "IoC.Scope.Current.Set",
            Args{worker_scope}
        )->Execute();

        // Разрешаем - должен создаться новый экземпляр
        auto worker_instance1 = IoC::Resolve<std::shared_ptr<TestService>>("ThreadScopedService");

        // Разрешаем еще раз - должен вернуться тот же (кэшированный в worker_scope)
        auto worker_instance2 = IoC::Resolve<std::shared_ptr<TestService>>("ThreadScopedService");

        EXPECT_EQ(worker_instance1, worker_instance2);
    });

    worker.join();

    // Всего должно быть создано 2 экземпляра (main + worker)
    EXPECT_EQ(creation_count, 2);
}

// ========================================
// Тесты для Transient lifetime
// ========================================

// ========== Тест 27: Transient создает новый экземпляр каждый раз ==========
TEST_F(IoCTest, Transient_CreatesNewInstanceEveryTime) {
    int creation_count = 0;

    // Регистрируем Transient
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Transient",
        Args{
            std::string("TransientService"),
            DependencyFactory([&creation_count](const Args&) -> std::any {
                creation_count++;
                return std::make_shared<TestService>("transient_" + std::to_string(creation_count));
            })
        }
    )->Execute();

    // Разрешаем несколько раз
    auto instance1 = IoC::Resolve<std::shared_ptr<TestService>>("TransientService");
    auto instance2 = IoC::Resolve<std::shared_ptr<TestService>>("TransientService");
    auto instance3 = IoC::Resolve<std::shared_ptr<TestService>>("TransientService");

    // Фабрика должна быть вызвана каждый раз
    EXPECT_EQ(creation_count, 3);

    // Все экземпляры должны быть разными объектами
    EXPECT_NE(instance1, instance2);
    EXPECT_NE(instance2, instance3);
    EXPECT_NE(instance1, instance3);

    EXPECT_EQ(instance1->GetValue(), "transient_1");
    EXPECT_EQ(instance2->GetValue(), "transient_2");
    EXPECT_EQ(instance3->GetValue(), "transient_3");
}

// ========== Тест 28: Сравнение всех трех lifetime в одном тесте ==========
TEST_F(IoCTest, AllLifetimes_Comparison) {
    int singleton_count = 0;
    int scoped_count = 0;
    int transient_count = 0;

    // Регистрируем все три типа
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Singleton",
        Args{
            std::string("ComparisonSingleton"),
            DependencyFactory([&singleton_count](const Args&) -> std::any {
                singleton_count++;
                return std::make_shared<TestService>("singleton");
            })
        }
    )->Execute();

    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Scoped",
        Args{
            std::string("ComparisonScoped"),
            DependencyFactory([&scoped_count](const Args&) -> std::any {
                scoped_count++;
                return std::make_shared<TestService>("scoped");
            })
        }
    )->Execute();

    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Transient",
        Args{
            std::string("ComparisonTransient"),
            DependencyFactory([&transient_count](const Args&) -> std::any {
                transient_count++;
                return std::make_shared<TestService>("transient");
            })
        }
    )->Execute();

    // === Scope 1 (root) ===
    auto s1_singleton1 = IoC::Resolve<std::shared_ptr<TestService>>("ComparisonSingleton");
    auto s1_scoped1 = IoC::Resolve<std::shared_ptr<TestService>>("ComparisonScoped");
    auto s1_transient1 = IoC::Resolve<std::shared_ptr<TestService>>("ComparisonTransient");

    auto s1_singleton2 = IoC::Resolve<std::shared_ptr<TestService>>("ComparisonSingleton");
    auto s1_scoped2 = IoC::Resolve<std::shared_ptr<TestService>>("ComparisonScoped");
    auto s1_transient2 = IoC::Resolve<std::shared_ptr<TestService>>("ComparisonTransient");

    EXPECT_EQ(singleton_count, 1);  // Singleton: создан 1 раз
    EXPECT_EQ(scoped_count, 1);     // Scoped: создан 1 раз в scope1
    EXPECT_EQ(transient_count, 2);  // Transient: создан 2 раза

    EXPECT_EQ(s1_singleton1, s1_singleton2);  // Singleton: одинаковые
    EXPECT_EQ(s1_scoped1, s1_scoped2);        // Scoped: одинаковые в одном scope
    EXPECT_NE(s1_transient1, s1_transient2);  // Transient: разные

    // === Scope 2 (child) ===
    auto scope2 = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Create");
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Scope.Current.Set",
        Args{scope2}
    )->Execute();

    auto s2_singleton = IoC::Resolve<std::shared_ptr<TestService>>("ComparisonSingleton");
    auto s2_scoped = IoC::Resolve<std::shared_ptr<TestService>>("ComparisonScoped");
    auto s2_transient = IoC::Resolve<std::shared_ptr<TestService>>("ComparisonTransient");

    EXPECT_EQ(singleton_count, 1);  // Singleton: все еще 1
    EXPECT_EQ(scoped_count, 2);     // Scoped: создан новый для scope2
    EXPECT_EQ(transient_count, 3);  // Transient: еще один

    EXPECT_EQ(s2_singleton, s1_singleton1);  // Singleton: тот же объект
    EXPECT_NE(s2_scoped, s1_scoped1);        // Scoped: разные объекты в разных scopes
    EXPECT_NE(s2_transient, s1_transient1);  // Transient: разные объекты
}

// ========================================
// Тесты для Unregister
// ========================================

// ========== Тест 29: Unregister удаляет кэш Scoped зависимости ==========
TEST_F(IoCTest, Unregister_RemovesScopedCache) {
    int creation_count = 0;

    // Регистрируем Scoped
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Scoped",
        Args{
            std::string("ScopedToUnregister"),
            DependencyFactory([&creation_count](const Args&) -> std::any {
                creation_count++;
                return std::make_shared<TestService>("scoped");
            })
        }
    )->Execute();

    // Разрешаем - создается и кэшируется
    auto instance1 = IoC::Resolve<std::shared_ptr<TestService>>("ScopedToUnregister");
    EXPECT_EQ(creation_count, 1);

    // Разрешаем еще раз - возвращается из кэша
    auto instance2 = IoC::Resolve<std::shared_ptr<TestService>>("ScopedToUnregister");
    EXPECT_EQ(creation_count, 1);  // Счетчик не увеличился
    EXPECT_EQ(instance1, instance2);  // Тот же объект

    // Дерегистрируем
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Unregister",
        Args{std::string("ScopedToUnregister")}
    )->Execute();

    // Попытка разрешить должна бросить исключение
    EXPECT_THROW({
        auto instance3 = IoC::Resolve<std::shared_ptr<TestService>>("ScopedToUnregister");
    }, std::runtime_error);

    // instance1 и instance2 все еще валидны (shared_ptr держит объект)
    EXPECT_EQ(instance1->GetValue(), "scoped");
    EXPECT_EQ(instance2->GetValue(), "scoped");
}

// ========== Тест 30: Unregister удаляет кэш Singleton ==========
TEST_F(IoCTest, Unregister_RemovesSingletonCache) {
    int creation_count = 0;

    // Регистрируем Singleton
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Singleton",
        Args{
            std::string("SingletonToUnregister"),
            DependencyFactory([&creation_count](const Args&) -> std::any {
                creation_count++;
                return std::make_shared<TestService>("singleton");
            })
        }
    )->Execute();

    // Разрешаем несколько раз
    auto instance1 = IoC::Resolve<std::shared_ptr<TestService>>("SingletonToUnregister");
    auto instance2 = IoC::Resolve<std::shared_ptr<TestService>>("SingletonToUnregister");
    EXPECT_EQ(creation_count, 1);
    EXPECT_EQ(instance1, instance2);

    // Дерегистрируем
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Unregister",
        Args{std::string("SingletonToUnregister")}
    )->Execute();

    // Попытка разрешить должна бросить исключение
    EXPECT_THROW({
        auto instance3 = IoC::Resolve<std::shared_ptr<TestService>>("SingletonToUnregister");
    }, std::runtime_error);

    // Старые экземпляры все еще валидны
    EXPECT_EQ(instance1->GetValue(), "singleton");
}

// ========== Тест 31: Повторная регистрация после Unregister создает новый экземпляр ==========
TEST_F(IoCTest, ReregisterAfterUnregister_CreatesNewInstance) {
    int creation_count = 0;

    // Регистрируем Scoped
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Scoped",
        Args{
            std::string("ReregisterService"),
            DependencyFactory([&creation_count](const Args&) -> std::any {
                creation_count++;
                return std::make_shared<TestService>("version_1");
            })
        }
    )->Execute();

    // Разрешаем
    auto instance1 = IoC::Resolve<std::shared_ptr<TestService>>("ReregisterService");
    EXPECT_EQ(instance1->GetValue(), "version_1");
    EXPECT_EQ(creation_count, 1);

    // Дерегистрируем
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Unregister",
        Args{std::string("ReregisterService")}
    )->Execute();

    // Регистрируем заново с другой фабрикой
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "IoC.Register.Scoped",
        Args{
            std::string("ReregisterService"),
            DependencyFactory([&creation_count](const Args&) -> std::any {
                creation_count++;
                return std::make_shared<TestService>("version_2");
            })
        }
    )->Execute();

    // Разрешаем - должен создаться НОВЫЙ экземпляр (не из кэша)
    auto instance2 = IoC::Resolve<std::shared_ptr<TestService>>("ReregisterService");
    EXPECT_EQ(instance2->GetValue(), "version_2");
    EXPECT_EQ(creation_count, 2);  // Создано 2 экземпляра
    EXPECT_NE(instance1, instance2);  // Разные объекты

    // Старый экземпляр все еще валиден
    EXPECT_EQ(instance1->GetValue(), "version_1");
}
