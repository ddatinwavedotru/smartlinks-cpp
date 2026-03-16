# SmartLinks: Анализ проблем масштабируемости

Документ описывает узкие места производительности и масштабируемости проекта SmartLinks, которые проявятся при росте нагрузки.

**Версия:** 1.0.0
**Дата:** 2026-03-16
**Статус:** Analysis

---

## 📊 Введение

SmartLinks спроектирован как микросервис для обработки HTTP редиректов с использованием DSL правил и JWT авторизации. При росте нагрузки (запросов в секунду) и сложности правил (количество DSL операторов) проект столкнется с рядом фундаментальных проблем производительности.

**Текущие метрики:**
- Архитектура: Микросервис C++ (Drogon framework)
- DSL движок: Recursive descent parser + AST evaluator
- Плагинная система: Динамические библиотеки (.so)
- Dependency Injection: Custom IoC контейнер
- Хранилище: MongoDB (сетевые запросы)

---

## 🔴 Проблема 1: Закон Амдала и глобальные мьютексы

### Суть проблемы

**Закон Амдала** утверждает, что максимальное ускорение параллельной программы ограничено долей последовательного кода:

```
Speedup = 1 / (S + P/N)

где:
  S = доля последовательного кода (0..1)
  P = доля параллельного кода (1-S)
  N = количество процессоров
```

**Пример:** Если 5% кода последовательны (защищены мьютексом), максимальное ускорение = **20x**, даже при бесконечном количестве процессоров.

### Где проявляется

В SmartLinks есть несколько глобальных мьютексов, которые создают точки сериализации:

#### 1.1. Root IoC Scope (ConcurrentScopeDict)

**Файл:** `include/ioc/scope.hpp:71-110`

```cpp
class ConcurrentScopeDict : public ScopeDict {
private:
    mutable std::mutex mutex_;  // 🔴 Глобальный мьютекс!

public:
    std::optional<DependencyFactory> get(const std::string& key) const override {
        std::lock_guard<std::mutex> lock(mutex_);  // Блокировка на каждое чтение
        // ...
    }

    void set(const std::string& key, DependencyFactory value) override {
        std::lock_guard<std::mutex> lock(mutex_);  // Блокировка на каждую запись
        // ...
    }
};
```

**Проблема:**
- Каждый HTTP запрос резолвит зависимости через Root Scope
- При N параллельных запросах все потоки конкурируют за один мьютекс
- Критическая секция: чтение из `std::map` (O(log n) операции)

**Частота вызовов на 1 HTTP запрос:**
- ~10-15 резолвов через Root Scope (middleware chain, services, repositories)

#### 1.2. ParserRegistry (регистрация плагинов)

**Файл:** `include/dsl/parser/parser_registry.hpp:32-33`

```cpp
class ParserRegistry {
private:
    std::map<std::string, std::shared_ptr<IParser>> leaf_parsers_;
    mutable std::mutex mutex_;  // 🔴 Глобальный мьютекс!

public:
    std::vector<std::shared_ptr<IParser>> GetLeafParsers() const {
        std::lock_guard<std::mutex> lock(mutex_);  // Блокировка
        // Копирование всех парсеров в вектор
        // Сортировка по приоритету
        return result;
    }
};
```

**Проблема:**
- Каждый HTTP запрос вызывает `GetLeafParsers()` для парсинга DSL
- Мьютекс блокирует параллельный доступ
- Копирование вектора парсеров на каждый запрос

**Частота вызовов на 1 HTTP запрос:**
- 1 вызов `GetLeafParsers()` при парсинге условия

### Расчет влияния

**Предположим:**
- Доля кода под мьютексом: **3%** (IoC resolve + parser registry)
- Количество CPU cores: **16**

**По закону Амдала:**
```
Speedup = 1 / (0.03 + 0.97/16) ≈ 14.5x

Теоретический максимум: 16x
Фактический максимум: 14.5x
Потери производительности: 9%
```

**При 64 cores:**
```
Speedup = 1 / (0.03 + 0.97/64) ≈ 25x

Теоретический максимум: 64x
Фактический максимум: 25x
Потери производительности: 61%
```

### Решения

#### ✅ Краткосрочное решение: Read-Write Lock

Заменить `std::mutex` на `std::shared_mutex`:

```cpp
class ConcurrentScopeDict : public ScopeDict {
private:
    mutable std::shared_mutex mutex_;  // Shared mutex

public:
    std::optional<DependencyFactory> get(const std::string& key) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);  // Множественные читатели
        // ...
    }

    void set(const std::string& key, DependencyFactory value) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);  // Эксклюзивная запись
        // ...
    }
};
```

**Улучшение:** Параллельные read операции (99% случаев)

#### ✅ Среднесрочное решение: Lock-Free структуры

Использовать `folly::ConcurrentHashMap` или `tbb::concurrent_hash_map`:

```cpp
#include <folly/concurrency/ConcurrentHashMap.h>

class LockFreeScopeDict : public IScopeDict {
private:
    folly::ConcurrentHashMap<std::string, DependencyFactory> data_;

public:
    std::optional<DependencyFactory> get(const std::string& key) const override {
        auto it = data_.find(key);
        return it != data_.end() ? std::optional{it->second} : std::nullopt;
    }
};
```

**Улучшение:** Полная lock-free конкуренция

#### ✅ Долгосрочное решение: Thread-Local кэширование

Кэшировать часто используемые зависимости в thread-local storage:

```cpp
thread_local std::unordered_map<std::string, DependencyFactory> tls_cache;

std::optional<DependencyFactory> get(const std::string& key) const {
    // 1. Проверить TLS кэш
    auto tls_it = tls_cache.find(key);
    if (tls_it != tls_cache.end()) {
        return tls_it->second;  // Без блокировки!
    }

    // 2. Fallback к глобальному scope с блокировкой
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end()) {
        tls_cache[key] = it->second;  // Кэшировать для будущих запросов
        return it->second;
    }
    return std::nullopt;
}
```

**Улучшение:** 99% запросов без блокировки

---

## 🔴 Проблема 2: Линейный перебор зарегистрированных парсеров

### Суть проблемы

**Файл:** `src/dsl/parser/primary_expression_parser.cpp:29-54`

```cpp
std::variant<std::shared_ptr<ast::IBoolExpression>, std::string>
PrimaryExpressionParser::TryLeafParsers(
    const std::string& expr_str,
    std::shared_ptr<IContext> context
) const {
    // Получить все leaf парсеры из реестра (отсортированные по приоритету)
    auto leaf_parsers = registry_->GetLeafParsers();

    // 🔴 Линейный перебор O(N)
    for (const auto& parser : leaf_parsers) {
        auto result = parser->TryParse(expr_str, context);

        if (result.success) {
            return result.expression;
        }
    }

    // Ни один парсер не подошел
    return "Unknown expression: " + expr_str;
}
```

### Анализ сложности

**Текущее состояние:**
- Количество leaf парсеров: **3** (Datetime, Language, Authorized)
- Алгоритм: Линейный перебор **O(N)**
- Worst case: Пробуются все 3 парсера

**При росте плагинов:**
- 10 плагинов: до 10 попыток парсинга на каждое primary expression
- 50 плагинов: до 50 попыток парсинга
- 100 плагинов: до 100 попыток парсинга

**Пример DSL с 5 условиями:**
```dsl
LANGUAGE = en-US AND DATETIME > 2026-01-01T00:00:00Z AND AUTHORIZED AND GEOLOCATION = US AND DEVICE = mobile -> https://example.com
```

- Количество primary expressions: **5**
- Worst case попыток парсинга: **5 × N** (где N = количество плагинов)
- При N=50: **250 попыток парсинга** на одно правило!

### Решения

#### ✅ Краткосрочное решение: Приоритетная сортировка

Уже реализовано частично - сортировка по `GetPriority()`:

```cpp
std::sort(result.begin(), result.end(),
    [](const auto& a, const auto& b) {
        return a->GetPriority() > b->GetPriority();
    });
```

**Улучшение:** Наиболее вероятные парсеры проверяются первыми

#### ✅ Среднесрочное решение: Prefix Tree (Trie)

Использовать trie для быстрого матчинга по началу строки:

```cpp
class ParserTrie {
private:
    struct Node {
        std::map<char, std::unique_ptr<Node>> children;
        std::shared_ptr<IParser> parser;  // Если nullptr, то промежуточный узел
    };

    std::unique_ptr<Node> root_;

public:
    void Insert(const std::string& prefix, std::shared_ptr<IParser> parser) {
        auto node = root_.get();
        for (char c : prefix) {
            if (!node->children[c]) {
                node->children[c] = std::make_unique<Node>();
            }
            node = node->children[c].get();
        }
        node->parser = parser;
    }

    std::shared_ptr<IParser> FindParser(const std::string& input) {
        auto node = root_.get();
        for (char c : input) {
            c = std::toupper(c);  // Case-insensitive
            if (!node->children[c]) {
                return nullptr;
            }
            node = node->children[c].get();
            if (node->parser) {
                return node->parser;  // O(k) где k = длина префикса
            }
        }
        return nullptr;
    }
};
```

**Пример:**
```
DATETIME -> DatetimeParser
LANGUAGE -> LanguageParser
AUTHORIZED -> AuthorizedParser
```

**Улучшение:** O(k) вместо O(N), где k = длина ключевого слова (~8-10 символов)

#### ✅ Долгосрочное решение: Perfect Hash Function

Сгенерировать perfect hash function для ключевых слов:

```cpp
// Сгенерировано gperf или аналогичным инструментом
enum class ParserType {
    DATETIME = 0,
    LANGUAGE = 1,
    AUTHORIZED = 2,
    // ...
};

ParserType hash_keyword(const char* str, size_t len);

std::shared_ptr<IParser> GetParserByKeyword(const std::string& keyword) {
    auto type = hash_keyword(keyword.c_str(), keyword.length());
    return parser_map_[static_cast<int>(type)];  // O(1) lookup
}
```

**Улучшение:** O(1) вместо O(N)

---

## 🔴 Проблема 3: Парсинг DSL правил на каждый HTTP запрос

### Суть проблемы

**Файл:** `src/impl/smart_link_redirect_service_dsl.cpp:13-28`

```cpp
std::optional<std::string> SmartLinkRedirectServiceDsl::Evaluate() {
    // 🔴 1. Прочитать DSL правила из MongoDB (сетевой запрос)
    auto rules_set_opt = repository_->Read(context_);

    // 🔴 2. Распарсить DSL строку в AST (CPU-интенсивно)
    // Внутри Read() вызывается:
    //   - CombinedParser::Parse()
    //   - OrExpressionParser::Parse()
    //   - AndExpressionParser::Parse()
    //   - PrimaryExpressionParser::Parse()
    //   - LeafParsers::TryParse()

    // 3. Вычислить правила
    auto redirect_url = rules_set.Evaluate(context_);

    return redirect_url;
}
```

### Анализ накладных расходов

**На каждый HTTP запрос выполняется:**

1. **MongoDB roundtrip** (сетевой запрос)
   - Latency: ~1-5ms (локальный Docker)
   - Latency: ~10-50ms (удаленный сервер)

2. **Парсинг DSL строки**
   - Tokenization: O(m) где m = длина DSL строки
   - AST construction: O(n) где n = количество узлов
   - Regex matching: O(m × k) где k = количество regex паттернов

3. **Создание AST узлов**
   - Аллокация объектов: `std::make_shared` для каждого узла
   - IoC::Resolve для каждого типа узла
   - Инжекция адаптеров через конструкторы

**Пример DSL:**
```dsl
LANGUAGE = en-US AND DATETIME IN[2026-01-01T00:00:00Z, 2026-12-31T23:59:59Z] -> https://example.com;
LANGUAGE = ru-RU -> https://example.ru;
AUTHORIZED -> https://example.com/premium
```

**Количество операций:**
- Парсинг строки: ~200-500 символов
- Создание AST узлов: ~6-8 узлов
- Regex matching: ~10-15 операций
- Аллокации памяти: ~10-20 `std::shared_ptr`

**Общее время:** ~0.5-2ms CPU + 1-5ms I/O

**При 1000 RPS:**
- CPU время на парсинг: **500-2000ms/сек** (50-200% одного ядра!)
- I/O ожидание: **1000-5000ms/сек**

### Решения

#### ✅ Краткосрочное решение: In-Memory кэш AST

Кэшировать распарсенные AST деревья в памяти:

```cpp
class CachedRulesRepository {
private:
    std::shared_ptr<ISmartLinkRedirectRulesRepositoryDsl> inner_;

    // LRU кэш
    mutable std::unordered_map<std::string, CacheEntry> cache_;
    mutable std::shared_mutex cache_mutex_;

    struct CacheEntry {
        ast::RulesSet rules_set;
        std::chrono::steady_clock::time_point timestamp;
    };

public:
    std::optional<ast::RulesSet> Read(std::shared_ptr<IContext> context) {
        std::string link_alias = context->GetLinkAlias();

        // 1. Проверить кэш
        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex_);
            auto it = cache_.find(link_alias);
            if (it != cache_.end()) {
                auto age = std::chrono::steady_clock::now() - it->second.timestamp;
                if (age < std::chrono::seconds(60)) {  // TTL = 60 секунд
                    return it->second.rules_set;  // 🟢 Cache hit!
                }
            }
        }

        // 2. Cache miss - загрузить и распарсить
        auto rules_opt = inner_->Read(context);
        if (!rules_opt) return std::nullopt;

        // 3. Сохранить в кэш
        {
            std::unique_lock<std::shared_mutex> lock(cache_mutex_);
            cache_[link_alias] = CacheEntry{
                .rules_set = *rules_opt,
                .timestamp = std::chrono::steady_clock::now()
            };
        }

        return rules_opt;
    }
};
```

**Улучшение:**
- Cache hit ratio: ~80-95% (зависит от распределения запросов)
- Экономия: ~0.5-2ms CPU + 1-5ms I/O на cache hit
- При 1000 RPS и 90% hit rate: экономия **~1350-6300ms/сек**

#### ✅ Среднесрочное решение: Lazy compilation

Компилировать DSL в байт-код один раз:

```cpp
class CompiledRulesSet {
private:
    std::vector<Instruction> bytecode_;

    enum class OpCode {
        LOAD_LANGUAGE,    // Загрузить Accept-Language в стек
        LOAD_DATETIME,    // Загрузить current_time в стек
        LOAD_AUTHORIZED,  // Проверить JWT токен
        CMP_EQ,           // Сравнить TOS-1 == TOS
        AND,              // TOS-1 AND TOS
        OR,               // TOS-1 OR TOS
        JUMP_IF_FALSE,    // Условный переход
        RETURN_URL,       // Вернуть URL
    };

public:
    std::optional<std::string> Evaluate(const IContext& context) {
        std::stack<std::any> eval_stack;
        size_t pc = 0;  // Program counter

        while (pc < bytecode_.size()) {
            auto& instr = bytecode_[pc];
            switch (instr.opcode) {
                case OpCode::LOAD_LANGUAGE:
                    eval_stack.push(context.GetAcceptLanguage());
                    break;
                case OpCode::CMP_EQ:
                    // Pop two values, compare, push result
                    break;
                // ...
            }
            ++pc;
        }

        return std::any_cast<std::string>(eval_stack.top());
    }
};
```

**Улучшение:**
- Парсинг только один раз при загрузке
- Вычисление через stack machine (очень быстро)
- Можно добавить JIT компиляцию (LLVM)

#### ✅ Долгосрочное решение: Предкомпиляция DSL

Компилировать DSL в нативный код при деплое:

```cpp
// DSL
LANGUAGE = en-US AND DATETIME > 2026-01-01T00:00:00Z -> https://example.com

// Сгенерированный C++ код
bool evaluate_rule_1(const IContext& ctx) {
    return ctx.GetAcceptLanguage() == "en-US" &&
           ctx.GetCurrentTime() > 1735689600;  // 2026-01-01T00:00:00Z
}

std::optional<std::string> evaluate(const IContext& ctx) {
    if (evaluate_rule_1(ctx)) {
        return "https://example.com";
    }
    return std::nullopt;
}
```

**Улучшение:**
- Нулевые накладные расходы на парсинг
- Оптимизация компилятором
- Встраивание (inlining) условий

---

## 🔴 Проблема 4: Накладные расходы std::shared_ptr

### Суть проблемы

**Статистика использования:**
- Файлов с `shared_ptr`: **66**
- Общее количество упоминаний: **428**

**Накладные расходы std::shared_ptr:**

1. **Память:**
   - Control block: **16-24 байта** (указатели на объект и deleter, счетчики weak/strong)
   - Сам указатель: **8 байт**
   - Итого: **24-32 байта overhead** на каждый `shared_ptr`

2. **Атомарные операции:**
   - Increment ref count: **1 атомарная операция** при копировании
   - Decrement ref count: **1 атомарная операция** при уничтожении
   - Memory fence для синхронизации между ядрами

3. **Cache coherence:**
   - Control block расшаривается между потоками
   - Требует синхронизации CPU кэшей (MESI протокол)
   - False sharing при размещении в одной cache line

### Анализ влияния

**Пример: Парсинг одного DSL правила**

```cpp
auto expr = ioc::IoC::Resolve<std::shared_ptr<ast::IBoolExpression>>(
    "Language.AST.Node.=",
    args
);  // 🔴 Создается shared_ptr с control block

return parser::ParseResult{
    .success = true,
    .expression = expr,  // 🔴 Копирование shared_ptr (atomic increment)
    // ...
};
```

**На каждое AST выражение:**
- Аллокация control block: ~50ns
- Атомарный increment: ~10ns
- Атомарный decrement: ~10ns
- Cache coherence penalty: ~50-100ns (при конкуренции)

**Для правила с 5 условиями:**
- 5 AST узлов × (50 + 10 + 10 + 75) ns = **~725ns**
- При 1000 RPS: **~0.725ms/сек** только на управление памятью

**При 10000 RPS и сложных правилах:**
- **~7-10ms/сек** только на `shared_ptr` overhead

### Решения

#### ✅ Краткосрочное решение: Переход на unique_ptr

Где возможно, использовать `std::unique_ptr`:

```cpp
// Вместо
std::shared_ptr<ast::IBoolExpression> expr;

// Использовать
std::unique_ptr<ast::IBoolExpression> expr;
```

**Улучшение:**
- Нет control block (экономия 16-24 байта)
- Нет атомарных операций
- Move-семантика без накладных расходов

**Применимо:**
- Внутри AST узлов (left/right children)
- Внутри parser pipeline
- Временные объекты

#### ✅ Среднесрочное решение: Object Pool

Переиспользовать AST узлы вместо постоянного выделения/освобождения:

```cpp
template<typename T>
class ObjectPool {
private:
    std::vector<std::unique_ptr<T>> pool_;
    std::vector<T*> available_;
    std::mutex mutex_;

public:
    template<typename... Args>
    std::unique_ptr<T, PoolDeleter> Acquire(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!available_.empty()) {
            T* obj = available_.back();
            available_.pop_back();
            // Reset object state
            obj->Reset(std::forward<Args>(args)...);
            return std::unique_ptr<T, PoolDeleter>(obj, PoolDeleter{this});
        }

        // Allocate new object
        auto obj = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = obj.get();
        pool_.push_back(std::move(obj));
        return std::unique_ptr<T, PoolDeleter>(ptr, PoolDeleter{this});
    }

    void Release(T* obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        available_.push_back(obj);
    }
};

// Usage
ObjectPool<ast::LanguageEqualExpression> lang_expr_pool;

auto expr = lang_expr_pool.Acquire(adapter, "en-US");
// Use expr...
// Automatically returned to pool when destroyed
```

**Улучшение:**
- Переиспользование памяти
- Меньше аллокаций/деаллокаций
- Лучшая локальность кэша

#### ✅ Долгосрочное решение: Arena Allocator

Выделять все AST узлы из одной арены памяти:

```cpp
class ArenaAllocator {
private:
    std::vector<std::unique_ptr<char[]>> chunks_;
    char* current_;
    size_t remaining_;
    static constexpr size_t CHUNK_SIZE = 64 * 1024;  // 64KB

public:
    template<typename T, typename... Args>
    T* Allocate(Args&&... args) {
        if (remaining_ < sizeof(T)) {
            AllocateChunk();
        }

        T* ptr = reinterpret_cast<T*>(current_);
        new (ptr) T(std::forward<Args>(args)...);  // Placement new

        current_ += sizeof(T);
        remaining_ -= sizeof(T);

        return ptr;
    }

    void Reset() {
        // Destroy all objects
        // Reset pointers
        current_ = chunks_[0].get();
        remaining_ = CHUNK_SIZE;
    }

private:
    void AllocateChunk() {
        chunks_.push_back(std::make_unique<char[]>(CHUNK_SIZE));
        current_ = chunks_.back().get();
        remaining_ = CHUNK_SIZE;
    }
};

// Usage: один Arena на HTTP запрос
ArenaAllocator arena;
auto expr = arena.Allocate<ast::LanguageEqualExpression>(adapter, "en-US");
// В конце запроса вызвать arena.Reset() - все объекты уничтожены сразу
```

**Улучшение:**
- Очень быстрая аллокация (bump allocator)
- Отличная локальность кэша (все в одном chunk)
- Массовое освобождение памяти (один syscall)

---

## 🔴 Проблема 5: MongoDB roundtrip на каждый запрос (отсутствие кэширования)

### Суть проблемы

**Файл:** `src/impl/smart_link_redirect_rules_repository_dsl.cpp`

Каждый HTTP запрос выполняет:

```cpp
std::optional<ast::RulesSet> Read(std::shared_ptr<IContext> context) {
    // 🔴 Сетевой запрос к MongoDB
    auto doc_opt = repository_->FindById(context->GetLinkAlias());

    if (!doc_opt) return std::nullopt;

    // Извлечь DSL строку
    std::string dsl_text = doc_opt->view()["rules_dsl"].get_string().value;

    // Парсинг DSL
    auto result = parser_->Parse(dsl_text, context);

    return result;
}
```

### Анализ накладных расходов

**MongoDB roundtrip time:**
- Локальный Docker: **0.5-2ms**
- Локальная сеть (1 Gbps): **1-5ms**
- Удаленный сервер (10ms RTT): **10-20ms**

**При нагрузке:**
- 100 RPS: **50-200ms/сек** в ожидании MongoDB
- 1000 RPS: **500-2000ms/сек** в ожидании MongoDB
- 10000 RPS: **5000-20000ms/сек** в ожидании MongoDB

**Проблема:** Даже если все запросы к одному SmartLink, каждый раз выполняется сетевой запрос.

### Решения

#### ✅ Краткосрочное решение: TTL-based кэш

Уже описано в Проблеме 3. Дополнительно:

```cpp
class TtlCache {
private:
    struct Entry {
        ast::RulesSet rules_set;
        std::chrono::steady_clock::time_point expires_at;
    };

    std::unordered_map<std::string, Entry> cache_;
    mutable std::shared_mutex mutex_;
    std::chrono::seconds ttl_;

public:
    explicit TtlCache(std::chrono::seconds ttl) : ttl_(ttl) {}

    std::optional<ast::RulesSet> Get(const std::string& key) {
        std::shared_lock lock(mutex_);

        auto it = cache_.find(key);
        if (it == cache_.end()) return std::nullopt;

        // Проверить TTL
        if (std::chrono::steady_clock::now() > it->second.expires_at) {
            // Expired - удалить асинхронно
            return std::nullopt;
        }

        return it->second.rules_set;
    }

    void Set(const std::string& key, ast::RulesSet rules_set) {
        std::unique_lock lock(mutex_);

        cache_[key] = Entry{
            .rules_set = std::move(rules_set),
            .expires_at = std::chrono::steady_clock::now() + ttl_
        };
    }
};
```

**Конфигурация:**
- TTL: 30-60 секунд (баланс между актуальностью и cache hit rate)
- Max entries: 10000-100000 (ограничение памяти)
- Eviction policy: LRU или TTL-based

**Улучшение:**
- Cache hit rate: 80-95%
- Экономия latency: 0.5-20ms на cache hit

#### ✅ Среднесрочное решение: Redis кэш

Использовать Redis как distributed cache:

```cpp
class RedisRulesCache {
private:
    std::shared_ptr<RedisClient> redis_;

public:
    std::optional<ast::RulesSet> Get(const std::string& link_alias) {
        // GET link:<alias>:rules
        auto dsl_text = redis_->Get("link:" + link_alias + ":rules");

        if (!dsl_text) return std::nullopt;

        // Парсинг (или хранить сериализованный AST)
        auto rules = parser_->Parse(*dsl_text, context);

        return rules;
    }

    void Set(const std::string& link_alias, const std::string& dsl_text) {
        // SETEX link:<alias>:rules 60 <dsl_text>
        redis_->SetEx("link:" + link_alias + ":rules", 60, dsl_text);
    }

    void Invalidate(const std::string& link_alias) {
        // DEL link:<alias>:rules
        redis_->Del("link:" + link_alias + ":rules");
    }
};
```

**Преимущества:**
- Распределенный кэш (доступен всем инстансам SmartLinks)
- Автоматическая инвалидация (TTL)
- Pub/Sub для invalidation events

**Улучшение:**
- Redis latency: ~0.1-0.5ms (vs MongoDB 0.5-2ms)
- Горизонтальное масштабирование

#### ✅ Долгосрочное решение: Event-driven invalidation

Использовать MongoDB Change Streams для инвалидации кэша:

```cpp
class ChangeStreamInvalidator {
private:
    TtlCache& cache_;
    mongocxx::client& client_;

public:
    void Start() {
        auto collection = client_["Links"]["links"];

        // Watch for changes
        mongocxx::options::change_stream opts;
        auto stream = collection.watch(opts);

        while (stream.begin() != stream.end()) {
            auto event = *stream.begin();

            if (event["operationType"].get_string() == "update" ||
                event["operationType"].get_string() == "replace") {

                // Извлечь link_alias
                std::string alias = event["documentKey"]["_id"].get_string().value;

                // Инвалидировать кэш
                cache_.Invalidate(alias);

                LOG_INFO << "Invalidated cache for link: " << alias;
            }
        }
    }
};
```

**Улучшение:**
- Кэш всегда актуален
- Можно увеличить TTL до часов/дней
- Меньше нагрузки на MongoDB

---

## 🔴 Проблема 6: Копирование строк и промежуточные объекты при парсинге

### Суть проблемы

**Файл:** `src/dsl/parser/combined_parser.cpp:46-87`

```cpp
std::variant<ast::RulesSet, std::string> CombinedParser::Parse(
    const std::string& dsl_string,
    std::shared_ptr<IContext> context
) const {
    auto trimmed = Trim(dsl_string);  // 🔴 Копирование строки

    // 🔴 Разбить по ";" - копирование подстрок
    auto rule_strings = SplitRespectingParentheses(trimmed, ";");

    ast::RulesSet rules_set;

    for (const auto& rule_str : rule_strings) {  // 🔴 Копирование каждого правила
        auto trimmed_rule = Trim(rule_str);  // 🔴 Еще одно копирование

        // ...
        auto rule_result = ParseRule(trimmed_rule, context);  // 🔴 Передача копии
        // ...
    }

    return rules_set;
}
```

**Функция SplitRespectingParentheses:**

```cpp
std::vector<std::string> SplitRespectingParentheses(
    const std::string& input,
    const std::string& delimiter
) {
    std::vector<std::string> result;
    // ...
    result.push_back(input.substr(start, pos - start));  // 🔴 Копирование подстроки
    // ...
    return result;  // 🔴 RVO, но все равно копии внутри вектора
}
```

### Анализ накладных расходов

**Для DSL с 3 правилами:**
```dsl
LANGUAGE = en-US -> https://example.com;
LANGUAGE = ru-RU -> https://example.ru;
AUTHORIZED -> https://private.com
```

**Копирования строк:**
1. `Trim(dsl_string)`: копия **~150 байт**
2. `SplitRespectingParentheses`: 3 копии по **~50 байт** = **150 байт**
3. `Trim(rule_str)` × 3: 3 копии по **~45 байт** = **135 байт**
4. Внутри ParseRule еще копии для condition и URL

**Итого:** ~500-1000 байт копирований на одно правило

**При 1000 RPS:**
- **~500KB - 1MB/сек** копирований строк
- CPU time: ~0.1-0.5ms/сек
- Memory allocator pressure

### Решения

#### ✅ Краткосрочное решение: std::string_view

Использовать `std::string_view` для избежания копирований:

```cpp
std::variant<ast::RulesSet, std::string> CombinedParser::Parse(
    std::string_view dsl_string,  // 🟢 View вместо копии
    std::shared_ptr<IContext> context
) const {
    auto trimmed = Trim(dsl_string);  // 🟢 Возвращает string_view

    auto rule_views = SplitRespectingParentheses(trimmed, ";");  // 🟢 Вектор views

    ast::RulesSet rules_set;

    for (const auto& rule_view : rule_views) {  // 🟢 Итерация по views
        auto trimmed_rule = Trim(rule_view);  // 🟢 View на view

        auto rule_result = ParseRule(trimmed_rule, context);
        // ...
    }

    return rules_set;
}

// Новая сигнатура
std::string_view Trim(std::string_view str) {
    auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) return {};

    auto end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);  // 🟢 Нет копирования!
}

std::vector<std::string_view> SplitRespectingParentheses(
    std::string_view input,
    std::string_view delimiter
) {
    std::vector<std::string_view> result;
    // ...
    result.push_back(input.substr(start, pos - start));  // 🟢 View в вектор
    // ...
    return result;
}
```

**Улучшение:**
- Нулевое копирование строк
- Меньше работы для memory allocator
- Лучшая локальность кэша

**ВАЖНО:** Lifetime management - убедиться, что исходная строка живет дольше views

#### ✅ Среднесрочное решение: Zero-copy parsing

Парсить напрямую из MongoDB BSON без копирования:

```cpp
std::optional<ast::RulesSet> Read(std::shared_ptr<IContext> context) {
    auto doc_opt = repository_->FindById(context->GetLinkAlias());
    if (!doc_opt) return std::nullopt;

    // 🟢 Получить string_view напрямую из BSON (zero-copy)
    bsoncxx::stdx::string_view dsl_view =
        doc_opt->view()["rules_dsl"].get_string();

    // Парсить из view
    auto result = parser_->Parse(dsl_view, context);

    return result;
}
```

**Улучшение:**
- Нет копирования из BSON в std::string
- Прямой парсинг из сетевого буфера

#### ✅ Долгосрочное решение: Streaming parser

Парсить DSL потоково, без создания промежуточных структур:

```cpp
class StreamingParser {
private:
    std::string_view input_;
    size_t pos_ = 0;

public:
    StreamingParser(std::string_view input) : input_(input) {}

    ast::RulesSet Parse(std::shared_ptr<IContext> context) {
        ast::RulesSet result;

        while (pos_ < input_.size()) {
            auto rule = ParseNextRule(context);
            if (rule) {
                result.AddRule(std::move(*rule));
            }
        }

        return result;
    }

private:
    std::optional<ast::RedirectRule> ParseNextRule(
        std::shared_ptr<IContext> context
    ) {
        // Парсить правило напрямую из input_[pos_]
        // Обновлять pos_ по мере парсинга
        // Не создавать промежуточные строки
    }

    char Peek() const { return input_[pos_]; }
    char Consume() { return input_[pos_++]; }
    void Skip(size_t n) { pos_ += n; }
};
```

**Улучшение:**
- Минимальное использование памяти
- Cache-friendly sequential access
- Можно парсить прямо из network buffer

---

## 🔴 Проблема 7: IoC::Resolve overhead на каждое AST выражение

### Суть проблемы

**Каждое DSL выражение резолвится через IoC:**

**Файл:** `plugins/language/src/plugin_entry.cpp:44-76`

```cpp
// Регистрация фабрики для Language.AST.Node.=
ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
    auto context = std::any_cast<std::shared_ptr<dsl::IContext>>(args[0]);
    auto value = std::any_cast<std::string>(args[1]);

    // 🔴 Вложенный IoC::Resolve для адаптера
    ioc::Args adapter_args;
    adapter_args.push_back(context);
    auto adapter = ioc::IoC::Resolve<std::shared_ptr<ILanguageAccessible>>(
        "ILanguageAccessible",
        adapter_args
    );

    // Создание AST узла
    return std::static_pointer_cast<dsl::ast::IBoolExpression>(
        std::make_shared<dsl::ast::LanguageEqualExpression>(adapter, value)
    );
})
```

**Вызов при парсинге:**

```cpp
// В LanguageParser
auto expr = ioc::IoC::Resolve<std::shared_ptr<ast::IBoolExpression>>(
    "Language.AST.Node." + op,  // 🔴 Конкатенация строки
    args
);
```

### Анализ накладных расходов

**На каждый IoC::Resolve:**

1. **String lookup в map** - O(log n)
2. **Mutex lock** (в Root Scope) - contention
3. **std::function вызов** (фабрика) - indirect call
4. **std::any_cast** × N (для каждого аргумента) - type checking
5. **Вложенный IoC::Resolve** для адаптера - рекурсия
6. **std::make_shared** аллокация

**Для правила с 5 условиями:**
- 5 вызовов `IoC::Resolve` для AST nodes
- 5 вложенных `IoC::Resolve` для адаптеров
- **Итого: 10 IoC::Resolve вызовов**

**Время на один IoC::Resolve:**
- Mutex lock/unlock: ~20ns
- Map lookup: ~30ns
- Function call overhead: ~10ns
- std::any_cast × 2: ~20ns
- **Итого: ~80ns per resolve**

**Для 5 условий:**
- 10 × 80ns = **800ns** только на IoC overhead

**При 1000 RPS:**
- **~0.8ms/сек** на IoC overhead

### Решения

#### ✅ Краткосрочное решение: Кэширование фабрик

Кэшировать часто используемые фабрики в thread-local storage:

```cpp
class CachingIoC {
private:
    thread_local static std::unordered_map<std::string, DependencyFactory> tls_factories_;

public:
    template<typename T>
    static T Resolve(const std::string& key, const Args& args) {
        // 1. Проверить TLS кэш
        auto it = tls_factories_.find(key);
        if (it != tls_factories_.end()) {
            return std::any_cast<T>(it->second(args));  // 🟢 Без блокировки!
        }

        // 2. Fallback к глобальному IoC (с блокировкой)
        auto factory = GlobalIoC::GetFactory(key);

        // 3. Закэшировать для будущих использований
        tls_factories_[key] = factory;

        return std::any_cast<T>(factory(args));
    }
};
```

**Улучшение:**
- 99% вызовов без mutex
- Прямой вызов фабрики из TLS map

#### ✅ Среднесрочное решение: Compile-time DI

Использовать шаблоны вместо runtime IoC:

```cpp
template<typename Adapter, typename... Args>
class LanguageEqualExpression {
private:
    Adapter adapter_;
    std::string value_;

public:
    LanguageEqualExpression(Adapter adapter, std::string value)
        : adapter_(std::move(adapter))
        , value_(std::move(value)) {}

    bool Evaluate(const IContext& ctx) const {
        return adapter_.language_matches(value_);
    }
};

// Usage
auto expr = LanguageEqualExpression<Context2LanguageAccessible>(
    Context2LanguageAccessible(context),
    "en-US"
);
```

**Улучшение:**
- Нулевой runtime overhead
- Инлайнинг компилятором
- Type-safe на этапе компиляции

#### ✅ Долгосрочное решение: Expression templates

Lazy evaluation через expression templates:

```cpp
template<typename Left, typename Right>
class AndExpression {
private:
    const Left& left_;
    const Right& right_;

public:
    AndExpression(const Left& left, const Right& right)
        : left_(left), right_(right) {}

    bool Evaluate(const IContext& ctx) const {
        return left_.Evaluate(ctx) && right_.Evaluate(ctx);  // 🟢 Inline!
    }
};

// Usage
auto expr = AndExpression(
    LanguageEqualExpression(context, "en-US"),
    DatetimeGreaterExpression(context, timestamp)
);

bool result = expr.Evaluate(ctx);  // 🟢 Полностью inline без виртуальных вызовов
```

**Улучшение:**
- Compile-time полиморфизм
- Все inline (zero overhead abstraction)
- Оптимизация компилятором (constant folding, dead code elimination)

---

## 📊 Сводная таблица проблем

| # | Проблема | Влияние при росте | Сложность решения | Приоритет |
|---|----------|-------------------|-------------------|-----------|
| 1 | Закон Амдала (мьютексы) | **Критическое** - линейный рост блокировок | Средняя (RW lock, lock-free) | 🔴 Высокий |
| 2 | Линейный перебор парсеров | Высокое - O(N) при N плагинах | Низкая (Trie, hash) | 🟡 Средний |
| 3 | Парсинг на каждый запрос | **Критическое** - CPU + I/O на каждый запрос | Низкая (кэш) | 🔴 Высокий |
| 4 | Overhead std::shared_ptr | Среднее - атомики + cache coherence | Средняя (unique_ptr, arena) | 🟡 Средний |
| 5 | MongoDB roundtrip | **Критическое** - I/O latency на каждый запрос | Низкая (кэш, Redis) | 🔴 Высокий |
| 6 | Копирование строк | Среднее - memory allocator pressure | Низкая (string_view) | 🟢 Низкий |
| 7 | IoC::Resolve overhead | Среднее - mutex + индирекция | Высокая (compile-time DI) | 🟡 Средний |

---

## 🎯 Рекомендованная последовательность оптимизаций

### Фаза 1: Quick Wins (1-2 недели)

1. **Кэширование DSL правил** (Проблема 3, 5)
   - Реализовать TTL-based cache для AST
   - Cache hit rate: 80-95%
   - **Ожидаемое улучшение: 5-10x throughput**

2. **Переход на string_view** (Проблема 6)
   - Заменить копирования на views
   - **Ожидаемое улучшение: 10-20% CPU**

3. **Shared mutex для Root Scope** (Проблема 1)
   - Заменить mutex на shared_mutex
   - **Ожидаемое улучшение: 2-3x при >8 cores**

### Фаза 2: Средний срок (1-2 месяца)

4. **Trie для парсеров** (Проблема 2)
   - Реализовать prefix tree
   - **Ожидаемое улучшение: O(N) → O(k)**

5. **Object Pool для AST** (Проблема 4)
   - Переиспользование объектов
   - **Ожидаемое улучшение: 20-30% CPU**

6. **Redis distributed cache** (Проблема 5)
   - Для горизонтального масштабирования
   - **Ожидаемое улучшение: бесконечное горизонтальное масштабирование**

### Фаза 3: Долгий срок (3-6 месяцев)

7. **Lock-free структуры** (Проблема 1)
   - Folly ConcurrentHashMap
   - **Ожидаемое улучшение: близко к линейному масштабированию**

8. **Bytecode compilation** (Проблема 3)
   - DSL → bytecode → stack machine
   - **Ожидаемое улучшение: 10-50x evaluation speed**

9. **Compile-time DI** (Проблема 7)
   - Expression templates
   - **Ожидаемое улучшение: zero-overhead abstraction**

---

## 📈 Ожидаемые метрики после оптимизаций

### Текущее состояние (baseline)

- **Throughput:** ~1000-2000 RPS (на 1 core)
- **Latency p50:** ~2-5ms
- **Latency p99:** ~10-20ms
- **CPU usage:** ~80% на 1000 RPS

### После Фазы 1

- **Throughput:** ~10000-20000 RPS (на 1 core)
- **Latency p50:** ~0.5-1ms
- **Latency p99:** ~2-5ms
- **CPU usage:** ~50% на 10000 RPS

### После Фазы 2

- **Throughput:** ~50000-100000 RPS (на 16 cores)
- **Latency p50:** ~0.2-0.5ms
- **Latency p99:** ~1-2ms
- **CPU usage:** ~60% на 50000 RPS

### После Фазы 3

- **Throughput:** >200000 RPS (на 64 cores)
- **Latency p50:** ~0.1-0.2ms
- **Latency p99:** ~0.5-1ms
- **CPU usage:** ~70% на 200000 RPS
- **Масштабирование:** Близко к линейному

---

## 🔗 Связанная документация

- [ARCHITECTURE.md](../ARCHITECTURE.md) - Архитектура SmartLinks
- [DSL_GRAMMAR.md](./DSL_GRAMMAR.md) - Формальная грамматика DSL
- [INTEGRATIONS.md](../INTEGRATIONS.md) - Интеграции и IPC
- [TESTING.md](../TESTING.md) - Тестирование производительности

---

**Автор:** SmartLinks Development Team
**Последнее обновление:** 2026-03-16
**Статус:** Требует review от Tech Lead
