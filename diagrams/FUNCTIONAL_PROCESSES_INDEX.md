# Functional Processes - Index

Индекс функциональных (технических) процессов в проекте SmartLinks.

**Версия:** 1.0.0
**Дата:** 2026-03-16

---

## 📋 Список процессов

| # | Process ID | Название | Описание | Файл |
|---|------------|----------|----------|------|
| 1 | `dsl_parsing` | DSL Parsing | Парсинг DSL правил в AST | [FUNCTIONAL_PROCESSES_dsl_parsing.md](./FUNCTIONAL_PROCESSES_dsl_parsing.md) |
| 2 | `ioc_resolution` | IoC Container Resolution | Разрешение зависимостей через IoC | [FUNCTIONAL_PROCESSES_ioc_resolution.md](./FUNCTIONAL_PROCESSES_ioc_resolution.md) |
| 3 | `http_lifecycle` | HTTP Request Lifecycle | Полный жизненный цикл HTTP запроса | [FUNCTIONAL_PROCESSES_http_lifecycle.md](./FUNCTIONAL_PROCESSES_http_lifecycle.md) |
| 4 | `plugin_loading` | Plugin Loading | Динамическая загрузка DSL плагинов | [FUNCTIONAL_PROCESSES_plugin_loading.md](./FUNCTIONAL_PROCESSES_plugin_loading.md) |

---

## 🎯 Быстрый доступ по категориям

### DSL System

| Процесс | Описание |
|---------|----------|
| [DSL Parsing](./FUNCTIONAL_PROCESSES_dsl_parsing.md) | Как DSL текст превращается в AST |
| [Plugin Loading](./FUNCTIONAL_PROCESSES_plugin_loading.md) | Как загружаются плагины парсеров |

### Dependency Injection

| Процесс | Описание |
|---------|----------|
| [IoC Resolution](./FUNCTIONAL_PROCESSES_ioc_resolution.md) | Как работает IoC контейнер |

### HTTP Processing

| Процесс | Описание |
|---------|----------|
| [HTTP Lifecycle](./FUNCTIONAL_PROCESSES_http_lifecycle.md) | От HTTP request до response |

---

## 📖 Как читать

### Структура каждого документа

1. **Описание** - краткое описание процесса
2. **Диаграмма** - Mermaid flowchart/sequence diagram
3. **Технические детали** - пошаговое описание с кодом
4. **Производительность** - временная сложность, метрики
5. **Примеры** - конкретные use cases
6. **Обработка ошибок** - типичные проблемы и решения
7. **Связанные процессы** - ссылки на другие процессы
8. **Файлы кода** - где найти реализацию

---

## 🔗 Связь с другой документацией

### Business Processes

- [doc/BUSINESS_PROCESSES.md](../doc/BUSINESS_PROCESSES.md) - бизнес-процессы (что делает система)
- vs Functional Processes - технические процессы (как работает система)

### Architecture

- [ARCHITECTURE.md](../ARCHITECTURE.md) - статическая архитектура
- vs Functional Processes - динамическое поведение

### Code

- Functional Processes → указывают на конкретные файлы кода
- Используйте для навигации по кодовой базе

---

## 🆕 Добавление новых процессов

Создайте файл по шаблону:

```markdown
# Functional Process: <Process Name>

**Process ID:** `process_id`
**Type:** Functional/Technical Process
**Version:** 1.0.0
**Date:** 2026-03-16

---

## 📋 Описание

<Краткое описание процесса>

**Входные данные:**
- <input 1>
- <input 2>

**Выходные данные:**
- <output 1>
- <output 2>

---

## 🔄 Диаграмма процесса

```mermaid
flowchart TB
    ...
```

---

## 📝 Технические детали

...

---

## ⚡ Производительность

...

---

## 🧪 Примеры

...

---

## 🐛 Обработка ошибок

...

---

## 🔗 Связанные процессы

...

---

## 📚 Файлы кода

...

---

**Версия:** 1.0.0
**Дата:** 2026-03-16
```

Затем добавьте в этот индекс.

---

## 📊 Статистика

- **Всего процессов:** 4
- **Диаграмм Mermaid:** 4
- **Примеров кода:** ~40
- **Покрытие кодовой базы:** ~60% ключевых компонентов

---

**Автор:** SmartLinks Development Team
**Последнее обновление:** 2026-03-16
**Версия:** 1.0.0
