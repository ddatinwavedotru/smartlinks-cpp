// MongoDB migration script: JSON rules → DSL rules
// Преобразует поле "rules" (массив JSON) в поле "rules_dsl" (строка DSL)
//
// Использование:
// mongosh mongodb://localhost:27017/Links mongo-init/migrate_rules_to_dsl.js

db = db.getSiblingDB('Links');

print("=== Starting migration: JSON rules → DSL rules ===");

let totalDocuments = 0;
let migratedDocuments = 0;
let skippedDocuments = 0;

// Обработать каждый документ в коллекции links
db.links.find().forEach(function(doc) {
    totalDocuments++;

    // Пропустить документы без правил
    if (!doc.rules || doc.rules.length === 0) {
        print("Skipping document '" + doc.slug + "': no rules");
        skippedDocuments++;
        return;
    }

    // Пропустить документы, уже имеющие rules_dsl
    if (doc.rules_dsl) {
        print("Skipping document '" + doc.slug + "': already has rules_dsl");
        skippedDocuments++;
        return;
    }

    try {
        // Преобразовать каждое правило в DSL формат
        var dslRules = doc.rules.map(function(rule) {
            var conditions = [];

            // 1. Language условие (если не "any")
            if (rule.language && rule.language !== "any") {
                conditions.push("LANGUAGE = " + rule.language);
            }

            // 2. Datetime условие (IN[start, end])
            if (rule.start && rule.end) {
                var start = rule.start.toISOString();
                var end = rule.end.toISOString();
                conditions.push("DATETIME IN[" + start + ", " + end + "]");
            }

            // 3. Authorized условие
            if (rule.authorized === true) {
                conditions.push("AUTHORIZED");
            }

            // Объединить условия через AND
            var condition;
            if (conditions.length === 0) {
                // Нет условий - правило всегда срабатывает
                // Используем тавтологию: сравнение с далёким будущим
                condition = "DATETIME < 9999-12-31T23:59:59Z";
            } else {
                condition = conditions.join(" AND ");
            }

            // Добавить redirect
            return condition + " -> " + rule.redirect;
        }).join("; ");

        // Обновить документ
        db.links.updateOne(
            { _id: doc._id },
            {
                $set: { rules_dsl: dslRules },
                $rename: { rules: "rules_legacy" }
            }
        );

        print("Migrated document '" + doc.slug + "':");
        print("  DSL: " + dslRules);
        migratedDocuments++;

    } catch (e) {
        print("ERROR migrating document '" + doc.slug + "': " + e);
    }
});

print("\n=== Migration complete ===");
print("Total documents: " + totalDocuments);
print("Migrated: " + migratedDocuments);
print("Skipped: " + skippedDocuments);
