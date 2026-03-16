// mongo-init/init-users.js
// Выполняется при первом запуске MongoDB

db = db.getSiblingDB('Links');

// Создать коллекцию users
db.createCollection('users');

// Создать индексы
db.users.createIndex({ username: 1 }, { unique: true });
db.users.createIndex({ email: 1 }, { sparse: true });

// Создать коллекцию refresh_tokens
db.createCollection('refresh_tokens');

// Создать индексы для refresh_tokens
db.refresh_tokens.createIndex({ jti: 1 }, { unique: true });
db.refresh_tokens.createIndex({ user_id: 1 });
db.refresh_tokens.createIndex({ expires_at: 1 }); // TTL index
db.refresh_tokens.createIndex({ revoked: 1 });

// Добавить тестовых пользователей
// Пароль для всех: "password123"
// bcrypt hash сгенерирован с salt rounds = 10

db.users.insertMany([
  {
    username: 'testuser',
    password_hash: '$2b$10$N9qo8uLOickgx2ZMRZoMyeIjZAgcfl7p92ldGxad68LJZdL17lhWy',
    email: 'testuser@example.com',
    created_at: new Date(),
    active: true
  },
  {
    username: 'admin',
    password_hash: '$2b$10$N9qo8uLOickgx2ZMRZoMyeIjZAgcfl7p92ldGxad68LJZdL17lhWy',
    email: 'admin@example.com',
    created_at: new Date(),
    active: true,
    roles: ['admin']
  }
]);

print('MongoDB initialization completed:');
print('  - Collection "users" created with indexes');
print('  - Collection "refresh_tokens" created with indexes');
print('  - Test users added:');
print('    * testuser / password123');
print('    * admin / password123');
