// MongoDB initialization script for Docker container
// Инициализирует все базы данных: Links, LinksTest, LinksDSL, LinksDSLTest
// Имя начинается с 00- чтобы выполниться первым (алфавитный порядок)

print("=== MongoDB Initialization: All Databases ===");
print("Creating databases for both JSON (Legacy) and DSL modes");
print("");

// ============================================
// 1. Links (JSON Production)
// ============================================
print("1/4: Initializing Links (JSON production)...");

db = db.getSiblingDB('Links');
db.createCollection('links');

db.links.insertMany([
    {
        slug: "/test-redirect",
        state: "active",
        rules: [
            {
                language: "any",
                redirect: "https://example.com",
                start: new Date(Date.now() - 24 * 60 * 60 * 1000),
                end: new Date(Date.now() + 24 * 60 * 60 * 1000)
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

db.links.createIndex({ slug: 1 }, { unique: true });
print("✓ Links initialized with " + db.links.count() + " documents");

// ============================================
// 2. LinksTest (JSON Test) - пустая, создаётся тестами
// ============================================
print("2/4: Initializing LinksTest (JSON test)...");

db = db.getSiblingDB('LinksTest');
db.createCollection('links');
db.links.createIndex({ slug: 1 }, { unique: true });
print("✓ LinksTest initialized (empty, will be populated by tests)");

// ============================================
// 3. LinksDSL (DSL Production)
// ============================================
print("3/4: Initializing LinksDSL (DSL production)...");

db = db.getSiblingDB('LinksDSL');
db.createCollection('links');

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
        slug: "/test-dsl-or-operator",
        state: "active",
        rules_dsl: "LANGUAGE = fr-FR OR LANGUAGE = de-DE -> https://example-europe.com"
    },
    {
        slug: "/test-dsl-parentheses",
        state: "active",
        rules_dsl: "AUTHORIZED AND (LANGUAGE = ru-RU OR LANGUAGE = en-US) -> https://example-auth-lang.com"
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

db.links.createIndex({ slug: 1 }, { unique: true });
print("✓ LinksDSL initialized with " + db.links.count() + " documents");

// ============================================
// 4. LinksDSLTest (DSL Test)
// ============================================
print("4/4: Initializing LinksDSLTest (DSL test)...");

db = db.getSiblingDB('LinksDSLTest');
db.createCollection('links');

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

db.links.createIndex({ slug: 1 }, { unique: true });
print("✓ LinksDSLTest initialized with " + db.links.count() + " documents");

// ============================================
// 5. Create test users for JWT authentication
// ============================================
print("5/5: Creating test users for JWT authentication...");

// Bcrypt hash for password "password123"
// Generated using: bcrypt.hash("password123", 10)
var passwordHash = "$2b$10$yIqlNxsCShTSeRwlT0bdzOJSwF9BM55lJ/qPdyWu/1ZVf05KYV/A2";

// Create users in all databases that need JWT authentication
var databasesToInitUsers = ["Links", "LinksTest", "LinksDSL", "LinksDSLTest"];

databasesToInitUsers.forEach(function(dbName) {
    db = db.getSiblingDB(dbName);

    // Create users collection if doesn't exist
    if (!db.getCollectionNames().includes('users')) {
        db.createCollection('users');
    }

    // Remove existing testuser if present
    db.users.deleteOne({ username: "testuser" });

    // Insert test user
    db.users.insertOne({
        username: "testuser",
        password_hash: passwordHash,
        email: "testuser@example.com",
        active: true,
        created_at: new Date()
    });

    print("  ✓ Created test user in " + dbName);
});

print("✓ Test users created (username: testuser, password: password123)");

// ============================================
// Summary
// ============================================
print("");
print("=== Database Initialization Complete ===");
print("✓ Links (JSON production): " + db.getSiblingDB('Links').links.count() + " documents, " + db.getSiblingDB('Links').users.count() + " users");
print("✓ LinksTest (JSON test): " + db.getSiblingDB('LinksTest').links.count() + " documents (empty), " + db.getSiblingDB('LinksTest').users.count() + " users");
print("✓ LinksDSL (DSL production): " + db.getSiblingDB('LinksDSL').links.count() + " documents, " + db.getSiblingDB('LinksDSL').users.count() + " users");
print("✓ LinksDSLTest (DSL test): " + db.getSiblingDB('LinksDSLTest').links.count() + " documents, " + db.getSiblingDB('LinksDSLTest').users.count() + " users");
print("");
print("All databases ready for both JSON (Legacy) and DSL modes!");
print("Test user credentials: testuser / password123");
