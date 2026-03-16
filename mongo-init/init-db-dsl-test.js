// MongoDB initialization script for DSL tests
// Создаёт тестовую базу данных LinksDSLTest с DSL правилами

db = db.getSiblingDB('LinksDSLTest');

// Создать коллекцию links
db.createCollection('links');

// Вставить тестовые данные с DSL правилами
db.links.insertMany([
    {
        slug: "/test-dsl-simple",
        state: "active",
        rules_dsl: "DATETIME < 9999-12-31T23:59:59Z -> https://example.com"
    },
    {
        slug: "/test-dsl-language",
        state: "active",
        rules_dsl: "LANGUAGE = ru-RU -> https://example.ru; LANGUAGE = en-US -> https://example.com"
    },
    {
        slug: "/test-dsl-datetime",
        state: "active",
        rules_dsl: "DATETIME IN[2026-01-01T00:00:00Z, 2026-12-31T23:59:59Z] -> https://example-2026.com; DATETIME < 9999-12-31T23:59:59Z -> https://example-fallback.com"
    },
    {
        slug: "/test-dsl-authorized",
        state: "active",
        rules_dsl: "AUTHORIZED -> https://example-private.com"
    },
    {
        slug: "/test-dsl-complex",
        state: "active",
        rules_dsl: "LANGUAGE = ru-RU AND DATETIME IN[2026-03-01T00:00:00Z, 2026-03-31T23:59:59Z] -> https://example-ru-march.com; LANGUAGE = en-US OR AUTHORIZED -> https://example-en-or-auth.com; DATETIME < 9999-12-31T23:59:59Z -> https://example-fallback.com"
    },
    {
        slug: "/deleted-link-dsl",
        state: "deleted",
        rules_dsl: ""
    },
    {
        slug: "/freezed-link-dsl",
        state: "freezed",
        rules_dsl: ""
    }
]);

// Создать индекс на поле slug для быстрого поиска
db.links.createIndex({ slug: 1 }, { unique: true });

print("MongoDB DSL Test initialization complete. Test data with DSL rules inserted into LinksDSLTest.");
