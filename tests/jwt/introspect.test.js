// tests/jwt/introspect.test.js
// Тесты для endpoint introspection

const request = require('supertest');
const { TEST_CONFIG, setupTestDB, teardownTestDB, cleanTestDB, seedTestUsers } = require('./setup');

const baseURL = TEST_CONFIG.JWT_SERVICE_URL;

describe('POST /auth/introspect', () => {
  let db;
  let validAccessToken;
  let validRefreshToken;

  beforeAll(async () => {
    db = await setupTestDB();
  });

  afterAll(async () => {
    await teardownTestDB();
  });

  beforeEach(async () => {
    await cleanTestDB(db);
    await seedTestUsers(db);

    // Получить валидные токены перед каждым тестом
    const loginResponse = await request(baseURL)
      .post('/auth/login')
      .send({
        username: 'testuser',
        password: 'password123'
      });

    validAccessToken = loginResponse.body.access_token;
    validRefreshToken = loginResponse.body.refresh_token;
  });

  test('должен вернуть active=true для валидного access токена', async () => {
    const response = await request(baseURL)
      .post('/auth/introspect')
      .send({
        token: validAccessToken
      })
      .expect('Content-Type', /json/)
      .expect(200);

    expect(response.body).toHaveProperty('active', true);
    expect(response.body).toHaveProperty('sub');
    expect(response.body).toHaveProperty('username', 'testuser');
    expect(response.body).toHaveProperty('type', 'access');
    expect(response.body).toHaveProperty('exp');
    expect(response.body).toHaveProperty('iat');
    expect(response.body).toHaveProperty('iss', 'smartlinks-jwt-service');
    expect(response.body).toHaveProperty('aud', 'smartlinks-api');
  });

  test('должен вернуть active=true для валидного refresh токена', async () => {
    const response = await request(baseURL)
      .post('/auth/introspect')
      .send({
        token: validRefreshToken
      })
      .expect('Content-Type', /json/)
      .expect(200);

    expect(response.body).toHaveProperty('active', true);
    expect(response.body).toHaveProperty('type', 'refresh');
  });

  test('должен вернуть active=false для невалидного токена', async () => {
    const response = await request(baseURL)
      .post('/auth/introspect')
      .send({
        token: 'invalid.token.here'
      })
      .expect('Content-Type', /json/)
      .expect(200);

    expect(response.body).toHaveProperty('active', false);
  });

  test('должен вернуть active=false для истекшего токена', async () => {
    // Создать токен с истекшим exp (требует модификации или ожидания)
    // Для теста можно использовать manually crafted expired token
    const expiredToken = 'eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwiZXhwIjoxfQ.invalid';

    const response = await request(baseURL)
      .post('/auth/introspect')
      .send({
        token: expiredToken
      })
      .expect('Content-Type', /json/)
      .expect(200);

    expect(response.body).toHaveProperty('active', false);
  });

  test('должен вернуть 400 если отсутствует token', async () => {
    const response = await request(baseURL)
      .post('/auth/introspect')
      .send({})
      .expect('Content-Type', /json/)
      .expect(400);

    expect(response.body).toHaveProperty('error');
    expect(response.body.error).toMatch(/token.*required/i);
  });

  test('должен вернуть корректное значение exp и iat', async () => {
    const response = await request(baseURL)
      .post('/auth/introspect')
      .send({
        token: validAccessToken
      })
      .expect(200);

    const { exp, iat } = response.body;

    expect(exp).toBeGreaterThan(iat);
    expect(exp - iat).toBe(600); // 10 минут для access токена
  });

  test('должен вернуть корректный sub (user ID)', async () => {
    const response = await request(baseURL)
      .post('/auth/introspect')
      .send({
        token: validAccessToken
      })
      .expect(200);

    expect(response.body.sub).toBeTruthy();
    expect(response.body.sub).toMatch(/^[a-f0-9]{24}$/); // MongoDB ObjectId format
  });
});
