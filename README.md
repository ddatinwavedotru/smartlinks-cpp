# SmartLinks C++

[![CMake](https://github.com/ddatinwavedotru/smartlinks-cpp/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/ddatinwavedotru/smartlinks-cpp/actions/workflows/cmake-multi-platform.yml)
[![Coverage](https://codecov.io/gh/ddatinwavedotru/smartlinks-cpp/branch/main/graph/badge.svg)](https://codecov.io/gh/ddatinwavedotru/smartlinks-cpp)

Полный порт SmartLinks с C#/.NET на C++17 без использования .NET framework. Работает как микросервис в Docker контейнере под Linux с поддержкой GitHub CI для сборки, тестирования и code coverage.

## 📋 Описание

SmartLinks - это микросервисная архитектура для управления умными ссылками с JWT аутентификацией:

**Основные возможности:**
- **DSL режим** - декларативный язык для правил редиректа с поддержкой условий
- **JWT авторизация** - валидация токенов через AUTHORIZED expression в DSL
- Редиректит HTTP запросы на основе правил (язык, временной интервал, авторизация)
- **Плагинная архитектура** - расширяемая система адаптеров для DSL
- Управляет состоянием ссылок (active, deleted, freezed)
- Возвращает соответствующие HTTP статусы (307, 404, 405, 422, 429)
- Хранит данные в MongoDB
- **JWT микросервис** - выдача и обновление JWT токенов (RS256, access 10 мин, refresh 14 дней)
- **API Gateway** - единая точка входа (Nginx) с маршрутизацией запросов

**Ключевые особенности:**
- ✅ **Микросервисная архитектура** - SmartLinks + JWT + API Gateway
- ✅ **DSL движок** - парсер, AST, evaluator для декларативных правил
- ✅ **Плагинная система** - расширяемые адаптеры для DSL (AUTHORIZED, LANGUAGE, TIMEWINDOW)
- ✅ **JWT интеграция** - RS256 валидация через JWKS, поддержка Authorization header
- ✅ **Двухрежимная работа** - JSON/Legacy mode + DSL mode
- ✅ **Строгое соблюдение C# архитектуры** - все интерфейсы и паттерны сохранены
- ✅ **10+ интерфейсов + реализации** - полная доменная модель + DSL компоненты
- ✅ **Custom IoC контейнер** - функциональный подход, иерархические scopes
- ✅ **Middleware pipeline** - Chain of Responsibility с DI через конструктор
- ✅ **MongoDB интеграция** - официальный mongocxx driver
- ✅ **JWT микросервис** - Node.js, RS256, автогенерация ключей, 40 тестов
- ✅ **API Gateway (Nginx)** - единая точка входа, маршрутизация, логирование
- ✅ **Docker готов** - multi-stage Dockerfile, docker-compose
- ✅ **CI/CD** - GitHub Actions для build, test, coverage
- ✅ **Полное тестовое покрытие** - 95+ тестов (IoC + JWT + Integration JSON + DSL)

## 🏗️ Технологический стек

| Компонент | Технология | Версия |
|-----------|------------|--------|
| **Основной язык** | **C++** | **17** |
| HTTP framework | Drogon | latest |
| MongoDB driver | mongocxx | 3.8.x |
| БД | MongoDB | 7.0 |
| IoC container | Custom (порт из appserver) | - |
| **DSL компоненты** | **Parser + AST + Evaluator** | **Custom** |
| Plugin система | Динамические адаптеры | - |
| JWT validator | jwt-cpp | latest |
| **JWT микросервис** | **Node.js** | **18+** |
| JWT библиотека | jsonwebtoken | 9.0+ |
| JWT алгоритм | RS256 (RSA 2048-bit) | - |
| **API Gateway** | **Nginx** | **Alpine** |
| Маршрутизация | Nginx reverse proxy | - |
| Testing (C++) | GoogleTest | latest |
| Testing (JWT) | Jest + Supertest | 29.x |
| Build system | CMake | 3.20+ |
| Контейнеризация | Docker + Docker Compose | - |

## 🚀 Быстрый старт

### Требования

- Docker и Docker Compose
- (Для локальной сборки) CMake 3.20+, C++17 компилятор, Drogon, mongocxx

### Запуск в Docker

```bash
# Клонировать репозиторий
git clone https://github.com/ddatinwavedotru/smartlinks-cpp.git
cd smartlinks-cpp

# Запустить все сервисы (MongoDB + SmartLinks + JWT + Gateway)
docker-compose up --build

# Проверить работу через API Gateway
curl -I http://localhost/test-redirect

# Проверить JWT сервис
curl http://localhost/jwt/health

# Проверить Gateway health
curl http://localhost/gateway/health
```

**Доступные порты:**
- **`http://localhost`** - API Gateway (единая точка входа)
- **`http://localhost:8081`** - Mongo Express (веб UI для MongoDB)

**Примеры использования:**

```bash
# JWT Login
curl -X POST http://localhost/jwt/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "testuser", "password": "password123"}'

# SmartLinks redirect
curl -I http://localhost/my-link

# JWKS публичные ключи
curl http://localhost/.well-known/jwks.json
```

### Локальная сборка

```bash
# Установить зависимости (Ubuntu/Debian)
sudo apt-get install -y \
    build-essential cmake git \
    libssl-dev zlib1g-dev libjsoncpp-dev \
    uuid-dev libcurl4-openssl-dev

# Установить Drogon
git clone https://github.com/drogonframework/drogon.git
cd drogon && git submodule update --init
mkdir build && cd build && cmake .. && make && sudo make install

# Установить MongoDB C/C++ drivers
# ... (см. Dockerfile для команд)

# Собрать проект
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Запустить
./smartlinks
```

## 📁 Структура проекта

```
smartlinks-cpp/
├── include/
│   ├── ioc/              # IoC контейнер (8 файлов)
│   ├── domain/           # Доменные интерфейсы (10 файлов)
│   ├── impl/             # Реализации (11 файлов)
│   ├── middleware/       # Middleware (5 файлов + адаптер)
│   └── dsl/              # DSL компоненты (Parser, AST, Context)
├── src/
│   ├── ioc/              # Реализация IoC
│   ├── impl/             # Реализация доменных классов
│   ├── middleware/       # Реализация middleware
│   ├── dsl/              # Реализация DSL (Parser, AST, Evaluator)
│   └── main.cpp          # Entry point
├── plugins/
│   └── authorized/       # AUTHORIZED адаптер для JWT валидации
│       ├── include/
│       └── src/
├── jwt-service/          # JWT микросервис (Node.js)
│   ├── src/
│   │   └── server.js     # JWT endpoints (RS256, access + refresh)
│   ├── package.json
│   ├── Dockerfile
│   └── entrypoint.sh     # Автогенерация RSA ключей
├── tests/
│   ├── unit/
│   │   └── ioc_tests.cpp # IoC unit тесты (GoogleTest)
│   ├── bdd/
│   │   └── smartlinks_tests.cpp # Интеграционные тесты
│   ├── jwt/
│   │   ├── *.test.js     # JWT тесты (Jest, 40 тестов)
│   │   ├── setup.js      # Тестовое окружение
│   │   ├── Dockerfile    # Контейнер для тестов
│   │   └── run-tests.sh  # Запуск JWT тестов
│   └── run_bdd_tests.sh  # Запуск BDD тестов
├── mongo-init/
│   └── init-users.js     # Инициализация users коллекции
├── config/
│   └── config.json       # Конфигурация
├── diagrams/             # Архитектурные диаграммы (Mermaid, C4)
├── doc/                  # Документация
│   ├── DSL_GRAMMAR.md    # Формальная грамматика DSL (EBNF)
│   ├── COMPLEXITY.md     # Анализ проблем масштабируемости
│   ├── BUSINESS_PROCESSES.md  # Бизнес-процессы
│   ├── INFORMATION_SCHEMA.md  # Схема базы данных MongoDB
│   └── JWT.md            # JWT Service документация
├── .github/workflows/    # CI workflows
├── Dockerfile            # Multi-stage build
├── docker-compose.yml    # Production окружение
├── docker-compose.test.yml # Test окружение (JWT)
├── run_all_tests.sh      # МАСТЕР-СКРИПТ (все тесты)
├── run_integration_tests.sh # Интеграционные тесты
├── ARCHITECTURE.md       # Детальная архитектура SmartLinks
├── GATEWAY.md            # Документация API Gateway
├── INTEGRATIONS.md       # Интеграции сервисов и IPC
├── JWT.md                # Документация JWT сервиса
└── TESTING.md            # Документация по тестированию
```

## ⚙️ Конфигурация

Конфигурация находится в `config/config.json`:

```json
{
  "app": {
    "threads_num": 4,
    "port": 8080,
    "host": "0.0.0.0"
  },
  "mongodb": {
    "connection_uri": "mongodb://root:password@localhost:27017",
    "database": "Links",
    "collection": "links"
  }
}
```

## 🧪 Тестирование

### Быстрый старт - запуск всех тестов

**Рекомендуется:** Используйте мастер-скрипт для запуска всех тестов:

```bash
./run_all_tests.sh
```

**Что запускается:**
1. ✅ IoC Container Unit Tests (C++, GoogleTest) - 20 тестов
2. ✅ JWT Microservice Tests (Node.js, Jest, Docker) - 40 тестов
3. ✅ SmartLinks Integration Tests - JSON/Legacy mode (C++, GoogleTest) - 17 тестов
4. ✅ SmartLinks DSL Integration Tests - DSL mode (C++, GoogleTest) - 18+ тестов

**Автоматическая очистка:**
- Останавливает все Docker контейнеры
- Убивает все процессы smartlinks
- Очищает логи и временные файлы

**Подробная документация:** См. [TESTING.md](TESTING.md)

---

### Отдельные тестовые наборы

#### 1. IoC Container Unit Tests

```bash
cd build
./ioc_tests
# или через ctest:
ctest -R IoC_Tests --output-on-failure
```

**Покрытие:**
- ✅ 20 тестов для IoC контейнера
- ✅ Регистрация зависимостей через IoC.Register
- ✅ Иерархическое разрешение (parent scopes)
- ✅ Thread-local изоляция
- ✅ Обработка исключений

#### 2. JWT Microservice Tests

```bash
./tests/jwt/run-tests.sh
# или напрямую:
docker-compose -f docker-compose.test.yml run --rm jwt-tests
```

**Покрытие:**
- ✅ 40 тестов JWT микросервиса
- ✅ Login, refresh, revoke endpoints
- ✅ Token introspection
- ✅ JWKS public keys
- ✅ Health checks

#### 3. SmartLinks Integration Tests

```bash
# Все тесты (JSON + DSL режимы)
./run_all_tests.sh

# Только JSON/Legacy mode
./run_integration_tests.sh
```

**Покрытие JSON/Legacy Mode (17 тестов):**
- ✅ Method Not Allowed (405) - 5 тестов
- ✅ Not Found (404) - 3 теста
- ✅ Unprocessable Content (422) - 2 теста
- ✅ Temporary Redirect (307/429) - 7 тестов

**Покрытие DSL Mode (18 тестов):**
- ✅ DSL парсинг и выполнение правил
- ✅ LANGUAGE expressions (=, !=, any)
- ✅ DATETIME expressions (<, <=, >, >=, =, !=, IN[start,end])
- ✅ AUTHORIZED expression (JWT validation)
- ✅ Логические операторы (AND, OR)
- ✅ Множественные правила с fallback
- ✅ Интеграция с JWT микросервисом

---

### Code coverage

Для измерения покрытия кода тестами используется **gcovr**:

```bash
# Запустите MongoDB и JWT service
docker-compose up -d mongodb jwt-service

# Запустите coverage скрипт
./coverage.sh
```

Результаты:
- HTML отчет: `build/coverage_report.html`
- XML для Codecov: `build/coverage.xml`

Coverage автоматически измеряется в GitHub Actions и загружается в Codecov для C++ кода (src/, include/, plugins/).

## 🐳 Docker

### Multi-stage build

Dockerfile использует 3 stage:
1. **base** - установка Drogon, mongocxx, зависимостей
2. **builder** - сборка SmartLinks
3. **runtime** - минимальный образ (только runtime библиотеки)

### Docker Compose

- `mongodb` - MongoDB 7.0 с init script
- `smartlinks` - микросервис SmartLinks

```bash
docker-compose up -d        # Запустить в фоне
docker-compose logs -f      # Смотреть логи
docker-compose down         # Остановить
```

## 🎯 DSL для правил редиректа

SmartLinks поддерживает два режима работы:

### JSON/Legacy Mode (deprecated)

Правила хранятся как JSON массив в MongoDB:
```json
{
  "slug": "my-link",
  "rules": [
    {
      "language": "en-US",
      "redirect": "https://example.com/en",
      "start": ISODate("2024-01-01"),
      "end": ISODate("2025-12-31")
    }
  ]
}
```

### DSL Mode (current, рекомендуется)

Правила описываются декларативным языком:
```
LANGUAGE = ru-RU -> https://example.ru

DATETIME IN[2020-01-01T00:00:00Z, 2030-12-31T23:59:59Z] -> https://example.com

AUTHORIZED -> https://example-private.com; DATETIME < 9999-12-31T23:59:59Z -> https://example-public.com
```

**Синтаксис DSL:**
```
<condition> -> <url>
<condition1> -> <url1>; <condition2> -> <url2>  # Множественные правила (разделитель ;)
```

**Условия:**
- `LANGUAGE = en-US` - проверка языка
- `LANGUAGE != ru-RU` - отрицание языка
- `DATETIME < 2026-12-31T23:59:59Z` - сравнение даты
- `DATETIME IN[start, end]` - временной интервал
- `AUTHORIZED` - JWT авторизация
- `<cond1> AND <cond2>` - логическое И
- `<cond1> OR <cond2>` - логическое ИЛИ

**Пример DSL правила в MongoDB:**
```javascript
db.links.insertOne({
  slug: "smart-link",
  state: "active",
  rules_dsl: "AUTHORIZED AND LANGUAGE = en-US -> https://private.example.com/en; AUTHORIZED -> https://private.example.com; LANGUAGE = en-US -> https://public.example.com/en; DATETIME < 9999-12-31T23:59:59Z -> https://public.example.com"
})
```

**Преимущества DSL:**
- ✅ Читаемый синтаксис
- ✅ Сложные условия (AND, OR)
- ✅ Расширяемость через плагины
- ✅ JWT авторизация (AUTHORIZED)
- ✅ Валидация при парсинге

**Переключение режимов:**
```bash
# DSL mode (по умолчанию)
./smartlinks

# JSON/Legacy mode
USE_DSL_RULES=false ./smartlinks
```

## 🔍 Архитектура

См. [ARCHITECTURE.md](ARCHITECTURE.md) для детального описания:

- IoC контейнер (функциональный подход)
- Middleware pipeline (Chain of Responsibility)
- Dependency Injection через конструктор
- MongoDB repository pattern
- Бизнес-логика редиректа

См. также:
- [doc/DSL_GRAMMAR.md](doc/DSL_GRAMMAR.md) - Формальная грамматика DSL (EBNF)
- [doc/COMPLEXITY.md](doc/COMPLEXITY.md) - Анализ проблем масштабируемости
- [doc/BUSINESS_PROCESSES.md](doc/BUSINESS_PROCESSES.md) - Описание бизнес-процессов микросервиса
- [doc/INFORMATION_SCHEMA.md](doc/INFORMATION_SCHEMA.md) - Схема базы данных MongoDB
- [diagrams/FUNCTIONAL_PROCESSES_*.md](diagrams/) - Функциональные (технические) процессы
- [INTEGRATIONS.md](INTEGRATIONS.md) - Интеграции между сервисами (Gateway, SmartLinks, JWT, MongoDB)
- [GATEWAY.md](GATEWAY.md) - Конфигурация API Gateway
- [JWT.md](JWT.md) - JWT Service документация
- [diagrams/](diagrams/) - Архитектурные диаграммы (Mermaid, C4, Structurizr)

### Middleware порядок

1. **MethodNotAllowedMiddleware** (405) - проверка GET/HEAD
2. **NotFoundMiddleware** (404) - проверка существования
3. **UnprocessableContentMiddleware** (422) - проверка state=freezed
4. **TemporaryRedirectMiddleware** (307) - редирект или 429

## 📊 HTTP статусы

| Статус | Условие |
|--------|---------|
| 307 Temporary Redirect | Найдено подходящее правило редиректа |
| 404 Not Found | SmartLink не найдена или state="deleted" |
| 405 Method Not Allowed | Метод не GET/HEAD |
| 422 Unprocessable Entity | state="freezed" |
| 429 Too Many Requests | Не найдено подходящее правило |
| 500 Internal Server Error | Исключение в middleware |

## 🔧 Разработка

### Добавление новой зависимости

```cpp
// В main.cpp RegisterDependencies():
ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
    "IoC.Register",
    ioc::Args{
        std::string("MyService"),
        ioc::DependencyFactory([](const ioc::Args&) -> std::any {
            return std::make_shared<MyService>(/* dependencies */);
        })
    }
)->Execute();
```

### Добавление middleware

1. Создать класс наследующий `IMiddleware`
2. Реализовать `Invoke(request, response, next)`
3. Зарегистрировать в `RegisterDependencies()` как Transient
4. Добавить в `GetMiddlewarePipeline` команду

## 📝 Лицензия

MIT

## 🙏 Благодарности

- Команде Otus и лично Тюменцеву Евгению Александровичу за интересный курс
- Оригинальный C# проект SmartLinks
- Проект appserver за IoC контейнер
- Drogon framework
- MongoDB C++ driver team

## 📮 Контакты

Вопросы и предложения: [Issues](https://github.com/ddatinwavedotru/smartlinks-cpp/issues)
