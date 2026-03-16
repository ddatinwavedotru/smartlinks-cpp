// tests/jwt/auth.test.js
// Тесты для endpoints аутентификации

const request = require('supertest');
const { TEST_CONFIG, setupTestDB, teardownTestDB, cleanTestDB, seedTestUsers, getTestDB } = require('./setup');

const baseURL = TEST_CONFIG.JWT_SERVICE_URL;

describe('JWT Authentication Endpoints', () => {
  let db;

  // Инициализация перед всеми тестами
  beforeAll(async () => {
    db = await setupTestDB();
  });

  // Очистка после всех тестов
  afterAll(async () => {
    await teardownTestDB();
  });

  // Очистка и пересоздание данных перед каждым тестом
  beforeEach(async () => {
    await cleanTestDB(db);
    await seedTestUsers(db);
  });

  describe('POST /auth/login', () => {
    test('должен вернуть токены при корректных credentials', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect('Content-Type', /json/)
        .expect(200);

      expect(response.body).toHaveProperty('access_token');
      expect(response.body).toHaveProperty('refresh_token');
      expect(response.body).toHaveProperty('token_type', 'Bearer');
      expect(response.body).toHaveProperty('expires_in', 600);

      // Проверить что токены не пустые
      expect(response.body.access_token).toBeTruthy();
      expect(response.body.refresh_token).toBeTruthy();

      // Проверить формат JWT (3 части разделенные точками)
      expect(response.body.access_token.split('.')).toHaveLength(3);
      expect(response.body.refresh_token.split('.')).toHaveLength(3);
    });

    test('должен сохранить refresh токен в БД', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      // Декодировать refresh token чтобы получить jti
      const payload = JSON.parse(
        Buffer.from(response.body.refresh_token.split('.')[1], 'base64').toString()
      );

      // Проверить что токен есть в БД
      const tokenInDB = await db.collection('refresh_tokens').findOne({
        jti: payload.jti
      });

      expect(tokenInDB).toBeTruthy();
      expect(tokenInDB.username).toBe('testuser');
      expect(tokenInDB.revoked).toBe(false);
    });

    test('должен вернуть 400 если отсутствует username', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          password: 'password123'
        })
        .expect('Content-Type', /json/)
        .expect(400);

      expect(response.body).toHaveProperty('error');
      expect(response.body.error).toMatch(/username.*required/i);
    });

    test('должен вернуть 400 если отсутствует password', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser'
        })
        .expect('Content-Type', /json/)
        .expect(400);

      expect(response.body).toHaveProperty('error');
      expect(response.body.error).toMatch(/password.*required/i);
    });

    test('должен вернуть 401 при неверном пароле', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'wrongpassword'
        })
        .expect('Content-Type', /json/)
        .expect(401);

      expect(response.body).toHaveProperty('error');
      expect(response.body.error).toMatch(/Invalid username or password/i);
    });

    test('должен вернуть 401 при несуществующем пользователе', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'nonexistent',
          password: 'password123'
        })
        .expect('Content-Type', /json/)
        .expect(401);

      expect(response.body).toHaveProperty('error');
      expect(response.body.error).toMatch(/Invalid username or password/i);
    });

    test('должен вернуть 403 для деактивированного пользователя', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'inactive',
          password: 'password123'
        })
        .expect('Content-Type', /json/)
        .expect(403);

      expect(response.body).toHaveProperty('error');
      expect(response.body.error).toMatch(/disabled/i);
    });

    test('access токен должен иметь TTL 10 минут', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      const payload = JSON.parse(
        Buffer.from(response.body.access_token.split('.')[1], 'base64').toString()
      );

      const ttl = payload.exp - payload.iat;
      expect(ttl).toBe(600); // 10 минут = 600 секунд
    });

    test('refresh токен должен иметь TTL 14 дней', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      const payload = JSON.parse(
        Buffer.from(response.body.refresh_token.split('.')[1], 'base64').toString()
      );

      const ttl = payload.exp - payload.iat;
      expect(ttl).toBe(1209600); // 14 дней = 1209600 секунд
    });

    test('токены должны содержать корректные claims', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      // Access token payload
      const accessPayload = JSON.parse(
        Buffer.from(response.body.access_token.split('.')[1], 'base64').toString()
      );

      expect(accessPayload).toHaveProperty('sub');
      expect(accessPayload).toHaveProperty('username', 'testuser');
      expect(accessPayload).toHaveProperty('email', 'testuser@example.com');
      expect(accessPayload).toHaveProperty('type', 'access');
      expect(accessPayload).toHaveProperty('iss', 'smartlinks-jwt-service');
      expect(accessPayload).toHaveProperty('aud', 'smartlinks-api');

      // Refresh token payload
      const refreshPayload = JSON.parse(
        Buffer.from(response.body.refresh_token.split('.')[1], 'base64').toString()
      );

      expect(refreshPayload).toHaveProperty('sub');
      expect(refreshPayload).toHaveProperty('jti');
      expect(refreshPayload).toHaveProperty('type', 'refresh');
      expect(refreshPayload).toHaveProperty('iss', 'smartlinks-jwt-service');
      expect(refreshPayload).toHaveProperty('aud', 'smartlinks-api');
    });
  });

  describe('POST /auth/refresh', () => {
    let validRefreshToken;

    beforeEach(async () => {
      // Получить валидный refresh токен перед каждым тестом
      const loginResponse = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        });

      validRefreshToken = loginResponse.body.refresh_token;
    });

    test('должен вернуть новый access токен при валидном refresh токене', async () => {
      const response = await request(baseURL)
        .post('/auth/refresh')
        .send({
          refresh_token: validRefreshToken
        })
        .expect('Content-Type', /json/)
        .expect(200);

      expect(response.body).toHaveProperty('access_token');
      expect(response.body).toHaveProperty('token_type', 'Bearer');
      expect(response.body).toHaveProperty('expires_in', 600);
      expect(response.body.access_token).toBeTruthy();
      expect(response.body.access_token.split('.')).toHaveLength(3);
    });

    test('новый access токен должен иметь актуальный iat', async () => {
      const response = await request(baseURL)
        .post('/auth/refresh')
        .send({
          refresh_token: validRefreshToken
        })
        .expect(200);

      const payload = JSON.parse(
        Buffer.from(response.body.access_token.split('.')[1], 'base64').toString()
      );

      const now = Math.floor(Date.now() / 1000);
      expect(payload.iat).toBeGreaterThanOrEqual(now - 5); // В пределах 5 секунд
      expect(payload.iat).toBeLessThanOrEqual(now + 5);
    });

    test('должен вернуть 400 если отсутствует refresh_token', async () => {
      const response = await request(baseURL)
        .post('/auth/refresh')
        .send({})
        .expect('Content-Type', /json/)
        .expect(400);

      expect(response.body).toHaveProperty('error');
      expect(response.body.error).toMatch(/refresh_token.*required/i);
    });

    test('должен вернуть 401 при невалидном refresh токене', async () => {
      const response = await request(baseURL)
        .post('/auth/refresh')
        .send({
          refresh_token: 'invalid.token.here'
        })
        .expect('Content-Type', /json/)
        .expect(401);

      expect(response.body).toHaveProperty('error');
    });

    test('должен вернуть 401 при отозванном refresh токене', async () => {
      // Отозвать токен
      await request(baseURL)
        .post('/auth/revoke')
        .send({
          refresh_token: validRefreshToken
        })
        .expect(200);

      // Попытаться использовать отозванный токен
      const response = await request(baseURL)
        .post('/auth/refresh')
        .send({
          refresh_token: validRefreshToken
        })
        .expect('Content-Type', /json/)
        .expect(401);

      expect(response.body).toHaveProperty('error');
      expect(response.body.error).toMatch(/revoked/i);
    });

    test('должен вернуть 401 если использован access токен вместо refresh', async () => {
      const loginResponse = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        });

      const response = await request(baseURL)
        .post('/auth/refresh')
        .send({
          refresh_token: loginResponse.body.access_token
        })
        .expect('Content-Type', /json/)
        .expect(401);

      expect(response.body).toHaveProperty('error');
      expect(response.body.error).toMatch(/Invalid token type/i);
    });
  });

  describe('POST /auth/revoke', () => {
    let validRefreshToken;

    beforeEach(async () => {
      const loginResponse = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        });

      validRefreshToken = loginResponse.body.refresh_token;
    });

    test('должен отозвать refresh токен', async () => {
      const response = await request(baseURL)
        .post('/auth/revoke')
        .send({
          refresh_token: validRefreshToken
        })
        .expect('Content-Type', /json/)
        .expect(200);

      expect(response.body).toHaveProperty('success', true);
    });

    test('отозванный токен должен быть помечен в БД', async () => {
      await request(baseURL)
        .post('/auth/revoke')
        .send({
          refresh_token: validRefreshToken
        })
        .expect(200);

      const payload = JSON.parse(
        Buffer.from(validRefreshToken.split('.')[1], 'base64').toString()
      );

      const tokenInDB = await db.collection('refresh_tokens').findOne({
        jti: payload.jti
      });

      expect(tokenInDB).toBeTruthy();
      expect(tokenInDB.revoked).toBe(true);
      expect(tokenInDB.revoked_at).toBeTruthy();
    });

    test('отозванный токен нельзя использовать для refresh', async () => {
      await request(baseURL)
        .post('/auth/revoke')
        .send({
          refresh_token: validRefreshToken
        })
        .expect(200);

      await request(baseURL)
        .post('/auth/refresh')
        .send({
          refresh_token: validRefreshToken
        })
        .expect(401);
    });

    test('должен вернуть 400 если отсутствует refresh_token', async () => {
      const response = await request(baseURL)
        .post('/auth/revoke')
        .send({})
        .expect('Content-Type', /json/)
        .expect(400);

      expect(response.body).toHaveProperty('error');
    });

    test('должен вернуть 400 при невалидном формате токена', async () => {
      const response = await request(baseURL)
        .post('/auth/revoke')
        .send({
          refresh_token: 'not-a-jwt-token'
        })
        .expect('Content-Type', /json/)
        .expect(400);

      expect(response.body).toHaveProperty('error');
    });
  });
});
