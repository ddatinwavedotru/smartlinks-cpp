// MongoDB initialization script for both JSON and DSL modes
// Инициализирует все базы данных: Links, LinksTest, LinksDSL, LinksDSLTest

print("=== Initializing MongoDB databases ===");

// 1. Links (JSON production)
print("\n1. Initializing Links (JSON production)...");
load("mongo-init/init-db.js");

// 2. LinksTest (JSON test) - создаётся автоматически тестами

// 3. LinksDSL (DSL production)
print("\n2. Initializing LinksDSL (DSL production)...");
load("mongo-init/init-db-dsl.js");

// 4. LinksDSLTest (DSL test)
print("\n3. Initializing LinksDSLTest (DSL test)...");
load("mongo-init/init-db-dsl-test.js");

print("\n=== All databases initialized ===");
