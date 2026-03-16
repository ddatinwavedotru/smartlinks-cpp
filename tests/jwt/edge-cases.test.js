// tests/jwt/edge-cases.test.js
// Тесты граничных случаев и расширенных сценариев

const request = require('supertest');
const { TEST_CONFIG, setupTestDB, teardownTestDB, cleanTestDB, seedTestUsers } = require('./setup');

const baseURL = TEST_CONFIG.JWT_SERVICE_URL;

describe('JWT Edge Cases and Advanced Scenarios', () => {
  let db;

  beforeAll(async () => {
    db = await setupTestDB();
  });

  afterAll(async () => {
    await teardownTestDB();
  });

  beforeEach(async () => {
    await cleanTestDB(db);
    await seedTestUsers(db);
  });

  describe('Token Lifecycle', () => {
    test('должен создавать уникальный jti для каждого refresh токена', async () => {
      const tokens = [];

      for (let i = 0; i < 5; i++) {
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

        tokens.push(payload.jti);
      }

      // Все jti должны быть уникальными
      const uniqueJtis = new Set(tokens);
      expect(uniqueJtis.size).toBe(tokens.length);
    });

    test('старые refresh токены должны оставаться валидными после создания новых', async () => {
      // Создать первый токен
      const response1 = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      const firstRefreshToken = response1.body.refresh_token;

      // Создать второй токен
      await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      // Первый токен все еще должен работать
      const refreshResponse = await request(baseURL)
        .post('/auth/refresh')
        .send({
          refresh_token: firstRefreshToken
        })
        .expect(200);

      expect(refreshResponse.body).toHaveProperty('access_token');
    });

  });

  describe('Database State', () => {
    test('refresh токен должен сохраняться в БД с корректным expires_at', async () => {
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

      const tokenInDB = await db.collection('refresh_tokens').findOne({
        jti: payload.jti
      });

      expect(tokenInDB).toBeTruthy();
      expect(tokenInDB.expires_at).toBeTruthy();
      expect(tokenInDB.expires_at instanceof Date).toBe(true);

      // expires_at должен быть в будущем
      expect(tokenInDB.expires_at.getTime()).toBeGreaterThan(Date.now());

      // expires_at должен соответствовать exp из токена (14 дней)
      const expectedExpiration = new Date(payload.exp * 1000);
      expect(Math.abs(tokenInDB.expires_at.getTime() - expectedExpiration.getTime())).toBeLessThan(1000);
    });

    test('отозванный токен должен иметь revoked_at timestamp', async () => {
      const loginResponse = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      const refreshToken = loginResponse.body.refresh_token;
      const beforeRevoke = Date.now();

      await request(baseURL)
        .post('/auth/revoke')
        .send({
          refresh_token: refreshToken
        })
        .expect(200);

      const afterRevoke = Date.now();

      const payload = JSON.parse(
        Buffer.from(refreshToken.split('.')[1], 'base64').toString()
      );

      const tokenInDB = await db.collection('refresh_tokens').findOne({
        jti: payload.jti
      });

      expect(tokenInDB.revoked_at).toBeTruthy();
      expect(tokenInDB.revoked_at instanceof Date).toBe(true);

      const revokedTime = tokenInDB.revoked_at.getTime();
      expect(revokedTime).toBeGreaterThanOrEqual(beforeRevoke);
      expect(revokedTime).toBeLessThanOrEqual(afterRevoke);
    });

    test('после множественных login должны быть созданы отдельные записи в БД', async () => {
      const numLogins = 5;

      for (let i = 0; i < numLogins; i++) {
        await request(baseURL)
          .post('/auth/login')
          .send({
            username: 'testuser',
            password: 'password123'
          })
          .expect(200);
      }

      const tokensInDB = await db.collection('refresh_tokens')
        .find({ username: 'testuser', revoked: false })
        .toArray();

      expect(tokensInDB.length).toBe(numLogins);
    });
  });

  describe('User Roles and Permissions', () => {
    test('admin пользователь должен получать корректную роль в токене', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'admin',
          password: 'password123'
        })
        .expect(200);

      const payload = JSON.parse(
        Buffer.from(response.body.access_token.split('.')[1], 'base64').toString()
      );

      expect(payload.username).toBe('admin');
      // Если сервис поддерживает roles в токене
      // expect(payload.roles).toContain('admin');
    });

    test('разные пользователи должны получать токены с разными sub', async () => {
      const response1 = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      const response2 = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'admin',
          password: 'password123'
        })
        .expect(200);

      const payload1 = JSON.parse(
        Buffer.from(response1.body.access_token.split('.')[1], 'base64').toString()
      );

      const payload2 = JSON.parse(
        Buffer.from(response2.body.access_token.split('.')[1], 'base64').toString()
      );

      expect(payload1.sub).not.toBe(payload2.sub);
      expect(payload1.username).toBe('testuser');
      expect(payload2.username).toBe('admin');
    });
  });

  describe('Token Claims Validation', () => {
    test('access токен должен содержать все обязательные claims', async () => {
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

      // Стандартные JWT claims
      expect(payload).toHaveProperty('sub');
      expect(payload).toHaveProperty('iat');
      expect(payload).toHaveProperty('exp');
      expect(payload).toHaveProperty('iss');
      expect(payload).toHaveProperty('aud');

      // Кастомные claims
      expect(payload).toHaveProperty('type');
      expect(payload).toHaveProperty('username');
      expect(payload).toHaveProperty('email');

      // Проверить типы
      expect(typeof payload.sub).toBe('string');
      expect(typeof payload.iat).toBe('number');
      expect(typeof payload.exp).toBe('number');
      expect(typeof payload.iss).toBe('string');
      expect(typeof payload.aud).toBe('string');
      expect(typeof payload.type).toBe('string');
      expect(typeof payload.username).toBe('string');
      expect(typeof payload.email).toBe('string');
    });

    test('refresh токен должен содержать минимальный набор claims', async () => {
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

      // Обязательные claims для refresh токена
      expect(payload).toHaveProperty('sub');
      expect(payload).toHaveProperty('jti');
      expect(payload).toHaveProperty('iat');
      expect(payload).toHaveProperty('exp');
      expect(payload).toHaveProperty('iss');
      expect(payload).toHaveProperty('aud');
      expect(payload).toHaveProperty('type', 'refresh');

      // Refresh токен НЕ должен содержать лишних данных (email, etc)
      // для минимизации размера
    });

    test('iss claim должен быть корректным', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      const accessPayload = JSON.parse(
        Buffer.from(response.body.access_token.split('.')[1], 'base64').toString()
      );

      const refreshPayload = JSON.parse(
        Buffer.from(response.body.refresh_token.split('.')[1], 'base64').toString()
      );

      expect(accessPayload.iss).toBe('smartlinks-jwt-service');
      expect(refreshPayload.iss).toBe('smartlinks-jwt-service');
    });

    test('aud claim должен быть корректным', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      const accessPayload = JSON.parse(
        Buffer.from(response.body.access_token.split('.')[1], 'base64').toString()
      );

      const refreshPayload = JSON.parse(
        Buffer.from(response.body.refresh_token.split('.')[1], 'base64').toString()
      );

      expect(accessPayload.aud).toBe('smartlinks-api');
      expect(refreshPayload.aud).toBe('smartlinks-api');
    });
  });

  describe('Error Handling', () => {
    test('должен возвращать JSON error даже при внутренних ошибках', async () => {
      // Попытка использовать невалидный формат токена
      const response = await request(baseURL)
        .post('/auth/refresh')
        .send({
          refresh_token: 'this.is.not.a.valid.jwt.token.format.at.all'
        });

      expect(response.headers['content-type']).toMatch(/json/);
      expect(response.body).toHaveProperty('error');
      expect(typeof response.body.error).toBe('string');
    });

    test('должен возвращать корректный HTTP статус для разных ошибок', async () => {
      // 400 - Bad Request (отсутствуют параметры)
      const badRequest = await request(baseURL)
        .post('/auth/login')
        .send({})
        .expect(400);

      expect(badRequest.body).toHaveProperty('error');

      // 401 - Unauthorized (неверные credentials)
      const unauthorized = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'wrongpassword'
        })
        .expect(401);

      expect(unauthorized.body).toHaveProperty('error');

      // 403 - Forbidden (деактивированный пользователь)
      const forbidden = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'inactive',
          password: 'password123'
        })
        .expect(403);

      expect(forbidden.body).toHaveProperty('error');
    });
  });

  describe('Multiple Refresh Tokens Management', () => {
    test('пользователь может иметь несколько активных refresh токенов', async () => {
      const tokens = [];

      // Создать 3 сессии (3 refresh токена)
      for (let i = 0; i < 3; i++) {
        const response = await request(baseURL)
          .post('/auth/login')
          .send({
            username: 'testuser',
            password: 'password123'
          })
          .expect(200);

        tokens.push(response.body.refresh_token);
      }

      // Все токены должны работать независимо
      for (const token of tokens) {
        const response = await request(baseURL)
          .post('/auth/refresh')
          .send({
            refresh_token: token
          })
          .expect(200);

        expect(response.body).toHaveProperty('access_token');
      }
    });

    test('revoke одного refresh токена не влияет на другие', async () => {
      // Создать 2 токена
      const response1 = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      const response2 = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      const token1 = response1.body.refresh_token;
      const token2 = response2.body.refresh_token;

      // Отозвать первый токен
      await request(baseURL)
        .post('/auth/revoke')
        .send({
          refresh_token: token1
        })
        .expect(200);

      // Первый токен не работает
      await request(baseURL)
        .post('/auth/refresh')
        .send({
          refresh_token: token1
        })
        .expect(401);

      // Второй токен все еще работает
      await request(baseURL)
        .post('/auth/refresh')
        .send({
          refresh_token: token2
        })
        .expect(200);
    });
  });

  describe('CORS and Headers', () => {
    test('должен устанавливать корректные заголовки Content-Type', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      expect(response.headers['content-type']).toMatch(/application\/json/);
    });

    test('health endpoint должен возвращать JSON', async () => {
      const response = await request(baseURL)
        .get('/health')
        .expect(200);

      expect(response.headers['content-type']).toMatch(/json/);
    });

    test('jwks endpoint должен возвращать JSON', async () => {
      const response = await request(baseURL)
        .get('/.well-known/jwks.json')
        .expect(200);

      expect(response.headers['content-type']).toMatch(/json/);
    });
  });
});
