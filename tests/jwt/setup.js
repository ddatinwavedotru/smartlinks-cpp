// tests/jwt/setup.js
// Настройка тестового окружения

const { MongoClient } = require('mongodb');
const bcrypt = require('bcrypt');

// Тестовая конфигурация
const TEST_CONFIG = {
  MONGO_URI: process.env.TEST_MONGO_URI || 'mongodb://root:password@mongodb:27017',
  MONGO_DB: 'LinksTest',
  JWT_SERVICE_URL: process.env.JWT_SERVICE_URL || 'http://jwt-service-test:3000'
};

// Глобальная переменная для MongoDB клиента
let testMongoClient;

/**
 * Подключение к тестовой MongoDB
 */
async function connectToTestDB() {
  testMongoClient = await MongoClient.connect(TEST_CONFIG.MONGO_URI);
  return testMongoClient.db(TEST_CONFIG.MONGO_DB);
}

/**
 * Очистка тестовой БД
 */
async function cleanTestDB(db) {
  await db.collection('users').deleteMany({});
  await db.collection('refresh_tokens').deleteMany({});
}

/**
 * Создание тестовых пользователей
 */
async function seedTestUsers(db) {
  const password_hash = await bcrypt.hash('password123', 10);

  await db.collection('users').insertMany([
    {
      username: 'testuser',
      password_hash,
      email: 'testuser@example.com',
      created_at: new Date(),
      active: true
    },
    {
      username: 'admin',
      password_hash,
      email: 'admin@example.com',
      created_at: new Date(),
      active: true,
      roles: ['admin']
    },
    {
      username: 'inactive',
      password_hash,
      email: 'inactive@example.com',
      created_at: new Date(),
      active: false
    }
  ]);
}

/**
 * Инициализация тестовой БД перед всеми тестами
 */
async function setupTestDB() {
  const db = await connectToTestDB();

  // Создать коллекции и индексы
  await db.createCollection('users');
  await db.collection('users').createIndex({ username: 1 }, { unique: true });
  await db.collection('users').createIndex({ email: 1 }, { sparse: true });

  await db.createCollection('refresh_tokens');
  await db.collection('refresh_tokens').createIndex({ jti: 1 }, { unique: true });
  await db.collection('refresh_tokens').createIndex({ user_id: 1 });
  await db.collection('refresh_tokens').createIndex({ expires_at: 1 });
  await db.collection('refresh_tokens').createIndex({ revoked: 1 });

  // Очистить и заполнить тестовыми данными
  await cleanTestDB(db);
  await seedTestUsers(db);

  return db;
}

/**
 * Закрытие подключения к тестовой БД
 */
async function teardownTestDB() {
  if (testMongoClient) {
    await testMongoClient.close();
  }
}

/**
 * Получить тестовую БД
 */
function getTestDB() {
  if (!testMongoClient) {
    throw new Error('Test DB not connected. Call setupTestDB() first.');
  }
  return testMongoClient.db(TEST_CONFIG.MONGO_DB);
}

module.exports = {
  TEST_CONFIG,
  setupTestDB,
  teardownTestDB,
  cleanTestDB,
  seedTestUsers,
  getTestDB,
  connectToTestDB
};
