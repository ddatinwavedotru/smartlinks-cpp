// MongoDB initialization script
// Создаёт базу данных Links и коллекцию links с тестовыми данными

db = db.getSiblingDB('Links');

// Создать коллекцию links
db.createCollection('links');

// Вставить тестовые данные
db.links.insertMany([
    {
        slug: "/test-redirect",
        state: "active",
        rules: [
            {
                language: "any",
                redirect: "https://example.com",
                start: new Date(Date.now() - 24 * 60 * 60 * 1000), // 24 часа назад
                end: new Date(Date.now() + 24 * 60 * 60 * 1000)    // через 24 часа
            }
        ]
    },
    {
        slug: "/deleted-link",
        state: "deleted",
        rules: []
    },
    {
        slug: "/freezed-link",
        state: "freezed",
        rules: []
    },
    {
        slug: "/language-test",
        state: "active",
        rules: [
            {
                language: "ru-RU",
                redirect: "https://example.ru",
                start: new Date(Date.now() - 24 * 60 * 60 * 1000),
                end: new Date(Date.now() + 24 * 60 * 60 * 1000)
            },
            {
                language: "en-US",
                redirect: "https://example.com",
                start: new Date(Date.now() - 24 * 60 * 60 * 1000),
                end: new Date(Date.now() + 24 * 60 * 60 * 1000)
            }
        ]
    }
]);

// Создать индекс на поле slug для быстрого поиска
db.links.createIndex({ slug: 1 }, { unique: true });

print("MongoDB initialization complete. Test data inserted.");
