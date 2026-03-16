# SmartLinks DSL Grammar (EBNF)

Формальное описание грамматики DSL для правил редиректа SmartLinks в расширенной форме Бэкуса-Наура (EBNF).

**Версия:** 1.0.0
**Дата:** 2026-03-16
**Статус:** Production

---

## 📋 Общее описание

DSL (Domain-Specific Language) для SmartLinks предназначен для декларативного описания правил редиректа на основе условий:
- Языка запроса (Accept-Language header)
- Временного диапазона
- JWT авторизации

---

## 🔤 EBNF Грамматика

### Основная структура

```ebnf
RulesSet        = Rule { ";" Rule } [ ";" ] ;

Rule            = Condition "->" URL ;

Condition       = OrExpression ;

OrExpression    = AndExpression { "OR" AndExpression } ;

AndExpression   = PrimaryExpression { "AND" PrimaryExpression } ;

PrimaryExpression =
                  | "(" OrExpression ")"
                  | LeafExpression
                  ;

LeafExpression  =
                  | DatetimeExpression
                  | LanguageExpression
                  | AuthorizedExpression
                  ;
```

---

### Datetime Expressions

```ebnf
DatetimeExpression =
                  | DatetimeComparison
                  | DatetimeIn
                  ;

DatetimeComparison = "DATETIME" ComparisonOp ISO8601DateTime ;

DatetimeIn      = "DATETIME" "IN" "[" ISO8601DateTime "," ISO8601DateTime "]" ;

ComparisonOp    = "=" | "!=" | "<" | "<=" | ">" | ">=" ;
```

**ISO8601DateTime** (терминальный узел с regex):

```regex
Pattern: YYYY-MM-DDTHH:MM:SS(Z|[+-]HH:MM)

Regex: \d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(Z|[+-]\d{2}:\d{2})

Примеры:
  - 2026-01-01T00:00:00Z
  - 2026-12-31T23:59:59Z
  - 2026-06-15T12:30:00+03:00
  - 2026-03-20T09:45:30-05:00
```

**Примечания:**
- Case-insensitive (DATETIME = datetime = Datetime)
- Whitespace толерантен вокруг операторов
- Временная зона опциональна, по умолчанию UTC

---

### Language Expressions

```ebnf
LanguageExpression = "LANGUAGE" EqualityOp LanguageToken ;

EqualityOp      = "=" | "!=" ;
```

**LanguageToken** (терминальный узел с regex):

```regex
Pattern: Accept-Language token (BCP 47)

Regex: [a-z]{2,3}(-[A-Z][a-z]{3})?(-[A-Z]{2})?

Примеры:
  - en
  - en-US
  - ru-RU
  - zh-Hant
  - sr-Cyrl-RS

Специальные значения:
  - any   (любой язык)
  - *     (любой язык)
```

**Примечания:**
- Case-insensitive для ключевого слова LANGUAGE
- Case-sensitive для языковых токенов (рекомендуется lowercase)
- Поддержка BCP 47 формата (язык[-скрипт][-регион])

---

### Authorized Expression

```ebnf
AuthorizedExpression = "AUTHORIZED" ;
```

**Примечания:**
- Case-insensitive
- Не принимает параметров
- Проверяет наличие валидного JWT токена в Authorization header

---

### URL

**URL** (терминальный узел):

```regex
Pattern: Произвольная строка до конца правила (после "->")

Trim: Whitespace удаляется с начала и конца

Примеры:
  - https://example.com
  - https://example.com/path?query=value
  - http://localhost:3000/api/v1/resource
```

**Примечания:**
- Валидация URL не выполняется парсером
- URL может содержать пробелы (если не в начале/конце)

---

## 📐 Приоритет операторов

| Приоритет | Оператор | Ассоциативность | Описание |
|-----------|----------|-----------------|----------|
| 1 (высший) | `( )` | N/A | Скобки (группировка) |
| 2 | LeafExpression | N/A | Datetime, Language, Authorized |
| 3 | `AND` | Левая | Логическое И |
| 4 (низший) | `OR` | Левая | Логическое ИЛИ |

**Примечания:**
- AND имеет более высокий приоритет, чем OR
- Скобки переопределяют приоритет
- Операторы одного уровня вычисляются слева направо

---

## 🔍 Примеры правил

### 1. Простое правило (язык)

```dsl
LANGUAGE = ru-RU -> https://example.ru
```

**AST:**
```
Rule(
  condition: LanguageEqualExpression("ru-RU"),
  url: "https://example.ru"
)
```

---

### 2. Временной диапазон

```dsl
DATETIME IN[2026-01-01T00:00:00Z, 2026-12-31T23:59:59Z] -> https://example.com/2026
```

**AST:**
```
Rule(
  condition: DatetimeInExpression(
    start: 2026-01-01T00:00:00Z,
    end: 2026-12-31T23:59:59Z
  ),
  url: "https://example.com/2026"
)
```

---

### 3. JWT авторизация

```dsl
AUTHORIZED -> https://private.example.com
```

**AST:**
```
Rule(
  condition: AuthorizedExpression(),
  url: "https://private.example.com"
)
```

---

### 4. Комбинация с AND

```dsl
LANGUAGE = en-US AND DATETIME > 2026-01-01T00:00:00Z -> https://example.com/en/new
```

**AST:**
```
Rule(
  condition: AndExpression(
    left: LanguageEqualExpression("en-US"),
    right: DatetimeGreaterExpression(2026-01-01T00:00:00Z)
  ),
  url: "https://example.com/en/new"
)
```

---

### 5. Комбинация с OR

```dsl
LANGUAGE = ru-RU OR LANGUAGE = uk-UA -> https://example.ru
```

**AST:**
```
Rule(
  condition: OrExpression(
    left: LanguageEqualExpression("ru-RU"),
    right: LanguageEqualExpression("uk-UA")
  ),
  url: "https://example.ru"
)
```

---

### 6. Приоритет AND выше OR

```dsl
LANGUAGE = en-US AND AUTHORIZED OR LANGUAGE = ru-RU -> https://example.com
```

**Интерпретация:**
```
(LANGUAGE = en-US AND AUTHORIZED) OR LANGUAGE = ru-RU
```

**AST:**
```
Rule(
  condition: OrExpression(
    left: AndExpression(
      left: LanguageEqualExpression("en-US"),
      right: AuthorizedExpression()
    ),
    right: LanguageEqualExpression("ru-RU")
  ),
  url: "https://example.com"
)
```

---

### 7. Переопределение приоритета скобками

```dsl
LANGUAGE = en-US AND (AUTHORIZED OR LANGUAGE = ru-RU) -> https://example.com
```

**Интерпретация:**
```
LANGUAGE = en-US AND (AUTHORIZED OR LANGUAGE = ru-RU)
```

**AST:**
```
Rule(
  condition: AndExpression(
    left: LanguageEqualExpression("en-US"),
    right: OrExpression(
      left: AuthorizedExpression(),
      right: LanguageEqualExpression("ru-RU")
    )
  ),
  url: "https://example.com"
)
```

---

### 8. Множественные правила

```dsl
LANGUAGE = ru-RU -> https://example.ru;
LANGUAGE = en-US -> https://example.com;
AUTHORIZED -> https://private.example.com
```

**Примечания:**
- Правила разделяются точкой с запятой ";"
- Точка с запятой в конце опциональна
- Правила вычисляются по порядку (первое совпадение)

---

### 9. Сложное правило

```dsl
(LANGUAGE = en-US OR LANGUAGE = en-GB) AND DATETIME IN[2026-06-01T00:00:00Z, 2026-08-31T23:59:59Z] AND AUTHORIZED -> https://example.com/summer-en-premium
```

**AST:**
```
Rule(
  condition: AndExpression(
    left: AndExpression(
      left: OrExpression(
        left: LanguageEqualExpression("en-US"),
        right: LanguageEqualExpression("en-GB")
      ),
      right: DatetimeInExpression(
        start: 2026-06-01T00:00:00Z,
        end: 2026-08-31T23:59:59Z
      )
    ),
    right: AuthorizedExpression()
  ),
  url: "https://example.com/summer-en-premium"
)
```

---

## 💼 Практические примеры использования

### Сценарий 1: Многоязычный сайт

**Задача:** Перенаправлять пользователей на версию сайта на их языке.

```dsl
LANGUAGE = ru-RU -> https://example.ru;
LANGUAGE = en-US -> https://example.com;
LANGUAGE = en-GB -> https://example.co.uk;
LANGUAGE = de-DE -> https://example.de;
LANGUAGE = fr-FR -> https://example.fr;
LANGUAGE = es-ES -> https://example.es;
LANGUAGE = zh-CN -> https://example.cn;
LANGUAGE = ja-JP -> https://example.jp
```

**Fallback версия:**
```dsl
LANGUAGE = ru-RU -> https://example.ru;
LANGUAGE = en-US -> https://example.com;
LANGUAGE = en-GB -> https://example.co.uk;
LANGUAGE = de-DE -> https://example.de;
LANGUAGE = * -> https://example.com
```

---

### Сценарий 2: Временная акция

**Задача:** Показывать промо-страницу только в период новогодней акции.

```dsl
DATETIME IN[2026-12-20T00:00:00Z, 2027-01-10T23:59:59Z] -> https://example.com/newyear-promo;
DATETIME >= 2027-01-11T00:00:00Z -> https://example.com
```

**Или с языковой локализацией:**
```dsl
DATETIME IN[2026-12-20T00:00:00Z, 2027-01-10T23:59:59Z] AND LANGUAGE = ru-RU -> https://example.ru/novogodnyaya-akciya;
DATETIME IN[2026-12-20T00:00:00Z, 2027-01-10T23:59:59Z] AND LANGUAGE = en-US -> https://example.com/newyear-sale;
DATETIME >= 2027-01-11T00:00:00Z -> https://example.com
```

---

### Сценарий 3: Ограниченный доступ (Premium контент)

**Задача:** Перенаправлять на премиум-контент только авторизованных пользователей.

```dsl
AUTHORIZED -> https://example.com/premium/exclusive-content;
LANGUAGE = * -> https://example.com/signup
```

**С языковой локализацией страницы регистрации:**
```dsl
AUTHORIZED -> https://example.com/premium/exclusive-content;
LANGUAGE = ru-RU -> https://example.ru/registraciya;
LANGUAGE = en-US -> https://example.com/signup;
LANGUAGE = * -> https://example.com/signup
```

---

### Сценарий 4: Beta-тестирование для авторизованных

**Задача:** Новая функция доступна только авторизованным пользователям до официального релиза.

```dsl
DATETIME < 2026-06-01T00:00:00Z AND AUTHORIZED -> https://example.com/beta/new-feature;
DATETIME >= 2026-06-01T00:00:00Z -> https://example.com/new-feature;
LANGUAGE = * -> https://example.com
```

---

### Сценарий 5: Региональные версии с временными акциями

**Задача:** Летняя распродажа для англоязычных стран.

```dsl
DATETIME IN[2026-06-01T00:00:00Z, 2026-08-31T23:59:59Z] AND (LANGUAGE = en-US OR LANGUAGE = en-GB) -> https://example.com/summer-sale;
LANGUAGE = en-US -> https://example.com;
LANGUAGE = en-GB -> https://example.co.uk;
LANGUAGE = ru-RU -> https://example.ru
```

---

### Сценарий 6: Контент для разных часовых поясов

**Задача:** Утренняя акция (00:00-12:00 UTC).

```dsl
DATETIME >= 2026-03-16T00:00:00Z AND DATETIME < 2026-03-16T12:00:00Z -> https://example.com/morning-deals;
LANGUAGE = * -> https://example.com
```

**Вечерняя акция (18:00-23:59 UTC):**
```dsl
DATETIME >= 2026-03-16T18:00:00Z AND DATETIME <= 2026-03-16T23:59:59Z -> https://example.com/evening-deals;
LANGUAGE = * -> https://example.com
```

---

### Сценарий 7: A/B тестирование для разных языков

**Задача:** Тестировать новый дизайн для русскоязычных пользователей.

```dsl
LANGUAGE = ru-RU AND AUTHORIZED -> https://example.ru/new-design;
LANGUAGE = ru-RU -> https://example.ru/old-design;
LANGUAGE = en-US -> https://example.com;
LANGUAGE = * -> https://example.com
```

---

### Сценарий 8: Ранний доступ для премиум-пользователей

**Задача:** Премиум-пользователи получают доступ на неделю раньше.

```dsl
DATETIME >= 2026-05-01T00:00:00Z AND AUTHORIZED -> https://example.com/product-launch;
DATETIME >= 2026-05-08T00:00:00Z -> https://example.com/product-launch;
LANGUAGE = * -> https://example.com/coming-soon
```

---

### Сценарий 9: Сложная бизнес-логика (Black Friday)

**Задача:** Black Friday акция для разных регионов с премиум-доступом.

```dsl
DATETIME IN[2026-11-27T00:00:00Z, 2026-11-29T23:59:59Z] AND AUTHORIZED AND LANGUAGE = en-US -> https://example.com/black-friday-vip;
DATETIME IN[2026-11-27T00:00:00Z, 2026-11-29T23:59:59Z] AND LANGUAGE = en-US -> https://example.com/black-friday;
DATETIME IN[2026-11-27T00:00:00Z, 2026-11-29T23:59:59Z] AND AUTHORIZED AND LANGUAGE = ru-RU -> https://example.ru/chernaya-pyatnica-vip;
DATETIME IN[2026-11-27T00:00:00Z, 2026-11-29T23:59:59Z] AND LANGUAGE = ru-RU -> https://example.ru/chernaya-pyatnica;
LANGUAGE = en-US -> https://example.com;
LANGUAGE = ru-RU -> https://example.ru
```

---

### Сценарий 10: Постепенный rollout новой функции

**Задача:** Выкатить новую функцию сначала для авторизованных, потом для всех.

**Фаза 1 (только авторизованные):**
```dsl
AUTHORIZED -> https://example.com/app/v2;
LANGUAGE = * -> https://example.com/app/v1
```

**Фаза 2 (неделя спустя - для всех):**
```dsl
DATETIME >= 2026-04-01T00:00:00Z -> https://example.com/app/v2;
LANGUAGE = * -> https://example.com/app/v1
```

---

### Сценарий 11: Региональные события

**Задача:** День независимости США (4 июля).

```dsl
DATETIME = 2026-07-04T00:00:00Z AND LANGUAGE = en-US -> https://example.com/independence-day-sale;
LANGUAGE = en-US -> https://example.com;
LANGUAGE = * -> https://example.com
```

**День России (12 июня):**
```dsl
DATETIME = 2026-06-12T00:00:00Z AND LANGUAGE = ru-RU -> https://example.ru/den-rossii-akciya;
LANGUAGE = ru-RU -> https://example.ru;
LANGUAGE = * -> https://example.com
```

---

### Сценарий 12: Контент по подписке (paywall)

**Задача:** Статья доступна только авторизованным пользователям.

```dsl
AUTHORIZED -> https://example.com/article/full-version;
LANGUAGE = en-US -> https://example.com/article/preview-subscribe;
LANGUAGE = ru-RU -> https://example.ru/article/preview-podpiska;
LANGUAGE = * -> https://example.com/article/preview
```

---

### Сценарий 13: Maintenance mode (техническое обслуживание)

**Задача:** Показывать страницу обслуживания всем, кроме авторизованных админов.

```dsl
AUTHORIZED -> https://example.com/app;
DATETIME IN[2026-03-20T02:00:00Z, 2026-03-20T06:00:00Z] -> https://example.com/maintenance;
LANGUAGE = * -> https://example.com/app
```

---

### Сценарий 14: Образовательный контент по уровням

**Задача:** Разный контент для разных уровней (требует авторизации для определения уровня).

```dsl
AUTHORIZED -> https://example.com/course/personalized;
LANGUAGE = ru-RU -> https://example.ru/course/demo;
LANGUAGE = en-US -> https://example.com/course/demo;
LANGUAGE = * -> https://example.com/signup
```

---

### Сценарий 15: Сезонный контент

**Задача:** Зимнее и летнее меню ресторана.

**Зимнее меню (декабрь-февраль):**
```dsl
DATETIME IN[2026-12-01T00:00:00Z, 2027-02-28T23:59:59Z] AND LANGUAGE = ru-RU -> https://restaurant.ru/menu/winter;
DATETIME IN[2026-12-01T00:00:00Z, 2027-02-28T23:59:59Z] AND LANGUAGE = en-US -> https://restaurant.com/menu/winter;
LANGUAGE = ru-RU -> https://restaurant.ru/menu;
LANGUAGE = en-US -> https://restaurant.com/menu
```

**Летнее меню (июнь-август):**
```dsl
DATETIME IN[2026-06-01T00:00:00Z, 2026-08-31T23:59:59Z] AND LANGUAGE = ru-RU -> https://restaurant.ru/menu/summer;
DATETIME IN[2026-06-01T00:00:00Z, 2026-08-31T23:59:59Z] AND LANGUAGE = en-US -> https://restaurant.com/menu/summer;
LANGUAGE = ru-RU -> https://restaurant.ru/menu;
LANGUAGE = en-US -> https://restaurant.com/menu
```

---

### Сценарий 16: API versioning

**Задача:** Старая версия API доступна только до определённой даты.

```dsl
DATETIME < 2026-12-31T23:59:59Z -> https://api.example.com/v1;
DATETIME >= 2026-12-31T23:59:59Z -> https://api.example.com/v2
```

**С поддержкой legacy клиентов (авторизованных):**
```dsl
DATETIME >= 2026-12-31T23:59:59Z AND AUTHORIZED -> https://api.example.com/v1/legacy;
DATETIME >= 2026-12-31T23:59:59Z -> https://api.example.com/v2;
DATETIME < 2026-12-31T23:59:59Z -> https://api.example.com/v1
```

---

### Сценарий 17: Конференция / мероприятие

**Задача:** Регистрация до события, трансляция во время, запись после.

**До события:**
```dsl
DATETIME < 2026-09-15T09:00:00Z AND LANGUAGE = en-US -> https://conference.com/register;
DATETIME < 2026-09-15T09:00:00Z AND LANGUAGE = ru-RU -> https://conference.ru/registraciya
```

**Во время события (live трансляция):**
```dsl
DATETIME IN[2026-09-15T09:00:00Z, 2026-09-15T18:00:00Z] AND AUTHORIZED -> https://conference.com/livestream;
DATETIME IN[2026-09-15T09:00:00Z, 2026-09-15T18:00:00Z] -> https://conference.com/register-now
```

**После события (запись):**
```dsl
DATETIME > 2026-09-15T18:00:00Z AND AUTHORIZED -> https://conference.com/recording;
DATETIME > 2026-09-15T18:00:00Z -> https://conference.com/get-access
```

---

### Сценарий 18: Географическая локализация через Accept-Language

**Задача:** Перенаправление на региональные сайты с учётом диалектов.

```dsl
LANGUAGE = en-US -> https://example.com;
LANGUAGE = en-GB -> https://example.co.uk;
LANGUAGE = en-AU -> https://example.com.au;
LANGUAGE = en-CA -> https://example.ca;
LANGUAGE = es-ES -> https://example.es;
LANGUAGE = es-MX -> https://example.mx;
LANGUAGE = pt-BR -> https://example.com.br;
LANGUAGE = pt-PT -> https://example.pt;
LANGUAGE = zh-CN -> https://example.cn;
LANGUAGE = zh-TW -> https://example.tw;
LANGUAGE = * -> https://example.com
```

---

### Сценарий 19: Expired content redirect

**Задача:** Контент доступен только ограниченное время.

```dsl
DATETIME < 2026-12-31T23:59:59Z -> https://example.com/article/special-report-2026;
DATETIME >= 2026-12-31T23:59:59Z -> https://example.com/archive/2026
```

---

### Сценарий 20: Сложная логика приоритетов

**Задача:** VIP пользователи всегда получают доступ, остальные - по расписанию.

```dsl
AUTHORIZED -> https://example.com/vip-area;
DATETIME IN[2026-03-16T09:00:00Z, 2026-03-16T17:00:00Z] -> https://example.com/public-area;
LANGUAGE = ru-RU -> https://example.ru/closed;
LANGUAGE = en-US -> https://example.com/closed;
LANGUAGE = * -> https://example.com/closed
```

---

## 🎯 Шаблоны и паттерны

### Паттерн: Default fallback

Всегда добавляйте fallback правило в конце:

```dsl
<specific rules here>
LANGUAGE = * -> https://example.com/default
```

---

### Паттерн: Временное окно с fallback

```dsl
DATETIME IN[start, end] AND <condition> -> <special-url>;
<condition> -> <normal-url>
```

---

### Паттерн: Премиум доступ с fallback

```dsl
AUTHORIZED AND <condition> -> <premium-url>;
<condition> -> <free-url>
```

---

### Паттерн: Многоязычный с временной акцией

```dsl
DATETIME IN[start, end] AND LANGUAGE = lang1 -> <promo-lang1-url>;
DATETIME IN[start, end] AND LANGUAGE = lang2 -> <promo-lang2-url>;
LANGUAGE = lang1 -> <normal-lang1-url>;
LANGUAGE = lang2 -> <normal-lang2-url>;
LANGUAGE = * -> <default-url>
```

---

### Паттерн: A/B тест (50/50 через авторизацию)

```dsl
AUTHORIZED -> <version-A-url>;
LANGUAGE = * -> <version-B-url>
```

**Примечание:** Для настоящего A/B теста требуется дополнительная логика (cookie, session, random), которая пока не поддерживается DSL.

---

## 🛠️ Парсинг и вычисление

### Алгоритм парсинга (Recursive Descent)

1. **RulesSet Parser**: Разделяет входную строку по ";" → список правил
2. **Rule Parser**: Разделяет правило по "->" → Condition + URL
3. **OrExpression Parser**: Разделяет условие по "OR" → создает OrExpression узлы
4. **AndExpression Parser**: Разделяет по "AND" → создает AndExpression узлы
5. **PrimaryExpression Parser**:
   - Если начинается с "(", извлекает содержимое скобок → рекурсивно парсит через OrExpression
   - Иначе пробует leaf парсеры (по приоритету)
6. **Leaf Parsers**:
   - DatetimeParser (приоритет 50)
   - LanguageParser (приоритет 50)
   - AuthorizedParser (приоритет 60)

---

### Алгоритм вычисления (AST Evaluation)

```
Evaluate(RulesSet, Context):
  For each Rule in RulesSet:
    result = Evaluate(Rule.Condition, Context)
    If result == true:
      Return 307 Redirect to Rule.URL
  Return 429 Too Many Requests (no match)

Evaluate(AndExpression, Context):
  left_result = Evaluate(AndExpression.Left, Context)
  If left_result == false:
    Return false  (short-circuit)
  right_result = Evaluate(AndExpression.Right, Context)
  Return right_result

Evaluate(OrExpression, Context):
  left_result = Evaluate(OrExpression.Left, Context)
  If left_result == true:
    Return true  (short-circuit)
  right_result = Evaluate(OrExpression.Right, Context)
  Return right_result

Evaluate(DatetimeExpression, Context):
  current_time = Context.current_time
  Return CompareDateTime(current_time, Expression.Value, Expression.Operator)

Evaluate(LanguageExpression, Context):
  accept_language = Context.accept_language
  Return MatchLanguage(accept_language, Expression.Value, Expression.Operator)

Evaluate(AuthorizedExpression, Context):
  jwt_token = Context.jwt_token
  Return ValidateJWT(jwt_token, Context.jwt_validator)
```

---

## 🔒 Типизация и семантика

### Типы выражений

| Тип | Возвращаемое значение | Описание |
|-----|----------------------|----------|
| **AndExpression** | `boolean` | Логическое И (short-circuit) |
| **OrExpression** | `boolean` | Логическое ИЛИ (short-circuit) |
| **DatetimeExpression** | `boolean` | Сравнение времени |
| **LanguageExpression** | `boolean` | Сравнение языка |
| **AuthorizedExpression** | `boolean` | Проверка авторизации |

---

### Контекст выполнения (IContext)

```cpp
interface IContext {
  // HTTP Request
  HttpRequest request;

  // Current time (from system clock)
  time_t current_time;

  // Accept-Language header value
  std::string accept_language;

  // JWT Validator (для проверки токенов)
  IJWTValidator jwt_validator;
}
```

---

## 📝 Лексическая структура

### Ключевые слова (case-insensitive)

```
DATETIME
LANGUAGE
AUTHORIZED
IN
AND
OR
```

---

### Операторы

```
Сравнения:    =  !=  <  <=  >  >=
Логические:   AND  OR
Разделители:  ;  ->
Группировка:  ( )  [ ]
Списки:       ,
```

---

### Whitespace

- Пробелы, табуляция, переводы строк игнорируются (кроме строковых литералов)
- Whitespace может использоваться для разделения токенов
- Whitespace вокруг операторов опционален

---

### Комментарии

**НЕ ПОДДЕРЖИВАЮТСЯ** в текущей версии DSL.

---

## 🔧 Расширяемость

### Плагинная архитектура

DSL поддерживает расширение через плагины (динамические библиотеки `.so`):

1. **IParser** - интерфейс парсера для нового LeafExpression
2. **IAdapter** - интерфейс адаптера для доступа к данным Context
3. **IBoolExpression** - AST узел с методом Evaluate()

### Добавление нового LeafExpression

Шаги:
1. Создать плагин с реализацией IParser
2. Определить regex паттерн для парсинга
3. Реализовать AST узел (IBoolExpression)
4. Реализовать адаптер (IAdapter)
5. Зарегистрировать в IoC Container через plugin_entry.cpp

**Пример:** Добавление `GEOLOCATION = US`

```cpp
// Новый LeafExpression в EBNF:
LeafExpression  = ... | GeolocationExpression ;

GeolocationExpression = "GEOLOCATION" EqualityOp CountryCode ;

CountryCode = [A-Z]{2} ;  // ISO 3166-1 alpha-2
```

---

## 🐛 Обработка ошибок

### Ошибки парсинга

| Ошибка | Описание | Пример |
|--------|----------|--------|
| `Missing '->' in rule` | Нет разделителя условия и URL | `LANGUAGE = en-US` |
| `Empty condition in rule` | Условие пустое | `-> https://example.com` |
| `Empty URL in rule` | URL пустой | `LANGUAGE = en-US ->` |
| `Invalid datetime format` | Неверный формат ISO8601 | `DATETIME = 2026-13-01` |
| `Invalid language token` | Неверный формат языка | `LANGUAGE = english` |
| `Unknown expression` | Не распознано ни одним парсером | `FOO = bar` |
| `Mismatched parentheses` | Несбалансированные скобки | `(LANGUAGE = en-US` |
| `Empty parentheses` | Пустые скобки | `() -> url` |

---

### Ошибки вычисления

| Ошибка | Описание | HTTP код |
|--------|----------|----------|
| `No matching rule` | Ни одно условие не выполнилось | 429 |
| `JWT validation failed` | Невалидный JWT токен для AUTHORIZED | 429 |
| `Plugin not loaded` | Плагин не загружен (парсер доступен, адаптер нет) | 500 |

---

## 📚 Связанная документация

- [ARCHITECTURE.md](./ARCHITECTURE.md) - Архитектура SmartLinks и DSL движка
- [diagrams/dsl-evaluation-flow.mmd](./diagrams/dsl-evaluation-flow.mmd) - Диаграмма процесса парсинга и вычисления
- [diagrams/dsl-plugin-system.mmd](./diagrams/dsl-plugin-system.mmd) - Архитектура плагинной системы
- [TESTING.md](./TESTING.md) - Тестирование DSL правил

---

## 📊 Статистика грамматики

| Метрика | Значение |
|---------|----------|
| Ключевых слов | 6 (DATETIME, LANGUAGE, AUTHORIZED, IN, AND, OR) |
| Операторов сравнения | 6 (=, !=, <, <=, >, >=) |
| Логических операторов | 2 (AND, OR) |
| LeafExpression типов | 3 (Datetime, Language, Authorized) |
| Datetime операторов | 7 (=, !=, <, <=, >, >=, IN) |
| Language операторов | 2 (=, !=) |
| Authorized операторов | 1 (keyword only) |

---

## 🔄 История версий

### v1.0.0 (2026-03-16)

- ✅ Базовая грамматика (AND, OR, скобки)
- ✅ DatetimeExpression (7 операторов)
- ✅ LanguageExpression (2 оператора)
- ✅ AuthorizedExpression
- ✅ Плагинная архитектура
- ✅ Recursive descent парсер
- ✅ AST evaluator с short-circuit

### Планы на будущее

- ⏳ NOT оператор (унарная операция)
- ⏳ GEOLOCATION expression (на основе IP)
- ⏳ USER_AGENT expression (на основе User-Agent header)
- ⏳ COOKIE expression (проверка cookie)
- ⏳ Комментарии в DSL (`//` или `#`)
- ⏳ Константы и переменные

---

**Автор:** SmartLinks Development Team
**Последнее обновление:** 2026-03-16
**Лицензия:** MIT
