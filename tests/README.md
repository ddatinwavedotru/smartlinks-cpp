# SmartLinks C++ Tests

Этот проект содержит два типа тестов:

## 1. Unit Tests (GoogleTest)

Unit тесты проверяют отдельные компоненты системы, особенно IoC контейнер.

### Запуск unit тестов

```bash
cd build
ctest -R IoC_Tests --output-on-failure
```

Или напрямую:

```bash
./build/tests/ioc_tests
```

### Что тестируется

- **IoC контейнер**: регистрация зависимостей, разрешение, иерархические скоупы
- **Scope управление**: создание, установка текущего, очистка
- **Command паттерн**: выполнение команд через IoC
- **Thread-local изоляция**: проверка изоляции скоупов между потоками

## 2. BDD Tests (Cucumber-cpp)

BDD тесты проверяют поведение приложения как черного ящика через HTTP API.

### Требования

1. **Cucumber-cpp** должен быть установлен:
   ```bash
   # Gentoo
   emerge -av dev-cpp/cucumber-cpp

   # Ubuntu/Debian
   # Собрать из исходников: https://github.com/cucumber/cucumber-cpp
   ```

2. **MongoDB** должен быть запущен:
   ```bash
   docker-compose up -d mongodb
   ```

3. **SmartLinks приложение** должно быть запущено:
   ```bash
   ./build/smartlinks &
   # или
   docker-compose up -d smartlinks
   ```

### Запуск BDD тестов

```bash
cd build
ctest -R BDD_Tests --output-on-failure
```

Или напрямую:

```bash
cd build/tests
./smartlinks_bdd_tests
```

### Feature файлы

- **returns_method_not_allowed.feature**: Проверка 405 для неподдерживаемых HTTP методов
- **returns_not_found.feature**: Проверка 404 для несуществующих/удалённых ссылок
- **returns_unprocessable_content.feature**: Проверка 422 для замороженных ссылок
- **returns_temporary_redirect.feature**: Проверка 307 редиректов с правилами

### Сценарии тестирования

#### Method Not Allowed (405)
- POST, PUT, DELETE запросы → 405
- GET, HEAD запросы → не 405

#### Not Found (404)
- Несуществующая ссылка → 404
- Ссылка со state="deleted" → 404
- Ссылка со state="active" → не 404

#### Unprocessable Content (422)
- Ссылка со state="freezed" → 422
- Ссылка со state="active" → не 422

#### Temporary Redirect (307)
- Безусловное правило (language="any") → 307 с Location
- Правило с подходящим языком → 307
- Правило с wildcard языком (*) → 307
- Нет подходящего правила → 429
- Правило вне временного окна → 429
- Несколько правил → выбирается первое подходящее

## Измерение покрытия кода

```bash
# Автоматический способ
./measure_coverage.sh

# Ручной способ
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
make -j$(nproc)
ctest --output-on-failure
lcov --directory . --capture --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
lcov --list coverage.info
genhtml coverage.info --output-directory coverage_html
```

Откройте `build/coverage_html/index.html` для просмотра детального отчёта.

## Архитектура тестов

### TestContext

Класс `TestContext` управляет тестовым окружением:

- **MongoDB**: подключение, вставка/удаление тестовых данных
- **HTTP Client**: отправка запросов к приложению
- **State**: хранение последнего ответа, текущего slug

### Step Definitions

- **common_steps.cpp**: Общие шаги для всех feature файлов
  - GIVEN: настройка тестовых данных в MongoDB
  - WHEN: отправка HTTP запросов
  - THEN: проверка статус-кодов и заголовков

- **redirect_steps.cpp**: Специфичные шаги для редиректов
  - Поддержка табличных данных (rules)
  - Парсинг относительного времени (-24 hours, +24 hours)

## Troubleshooting

### Cucumber-cpp не найден

```
CMake Warning: Cucumber-cpp not found, BDD tests will be skipped
```

**Решение**: Установите cucumber-cpp или соберите проект без BDD тестов (unit тесты будут работать).

### MongoDB недоступен

```
Failed to connect to MongoDB: No suitable servers found
```

**Решение**: Запустите MongoDB:
```bash
docker-compose up -d mongodb
```

### SmartLinks приложение не отвечает

```
HTTP request timeout
```

**Решение**: Убедитесь, что приложение запущено:
```bash
ps aux | grep smartlinks
# или
curl http://localhost:8080/test-redirect
```

### Тесты падают с bad_any_cast

Это известная проблема с типами в IoC контейнере. Убедитесь, что используется последняя версия кода с исправлениями `std::static_pointer_cast`.
