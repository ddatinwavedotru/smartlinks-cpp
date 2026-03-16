// tests/jwt/security.test.js
// Тесты безопасности JWT сервиса

const request = require('supertest');
const { TEST_CONFIG, setupTestDB, teardownTestDB, cleanTestDB, seedTestUsers } = require('./setup');

const baseURL = TEST_CONFIG.JWT_SERVICE_URL;

describe('JWT Security Tests', () => {
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

  describe('SQL Injection Protection', () => {
    test('должен защищать от SQL injection в username', async () => {
      const sqlInjectionPayloads = [
        "admin' OR '1'='1",
        "admin'--",
        "admin' OR 1=1--",
        "' OR '1'='1' /*",
        "1' UNION SELECT NULL--"
      ];

      for (const payload of sqlInjectionPayloads) {
        const response = await request(baseURL)
          .post('/auth/login')
          .send({
            username: payload,
            password: 'password123'
          })
          .expect(401);

        expect(response.body).toHaveProperty('error');
      }
    });

    test('должен защищать от SQL injection в password', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: "password' OR '1'='1"
        })
        .expect(401);

      expect(response.body).toHaveProperty('error');
    });
  });


  describe('XSS Protection', () => {
    test('должен корректно обрабатывать XSS в username при ошибке', async () => {
      const xssPayload = '<script>alert("XSS")</script>';

      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: xssPayload,
          password: 'password123'
        })
        .expect(401);

      // Убедиться что XSS payload не исполняется и правильно экранируется
      expect(response.body.error).not.toContain('<script>');
    });
  });

  describe('Input Validation', () => {
    test('должен отклонять слишком длинный username (>100 символов)', async () => {
      const longUsername = 'a'.repeat(101);

      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: longUsername,
          password: 'password123'
        });

      // Может быть 400 (валидация) или 401 (не найден пользователь)
      expect([400, 401]).toContain(response.status);
    });

    test('должен отклонять слишком длинный password (>200 символов)', async () => {
      const longPassword = 'a'.repeat(201);

      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: longPassword
        });

      expect([400, 401]).toContain(response.status);
    });

    test('должен отклонять пустой username', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: '',
          password: 'password123'
        })
        .expect(400);

      expect(response.body).toHaveProperty('error');
    });

    test('должен отклонять пустой password', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: ''
        })
        .expect(400);

      expect(response.body).toHaveProperty('error');
    });

    test('должен отклонять username с только пробелами', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .send({
          username: '   ',
          password: 'password123'
        })
        .expect(401);

      expect(response.body).toHaveProperty('error');
    });
  });

  describe('Token Security', () => {
    test('должен отклонять токен с измененной подписью', async () => {
      // Получить валидный токен
      const loginResponse = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      // Изменить последний символ подписи
      const validToken = loginResponse.body.access_token;
      const parts = validToken.split('.');
      const modifiedSignature = parts[2].slice(0, -1) + 'X';
      const tamperedToken = `${parts[0]}.${parts[1]}.${modifiedSignature}`;

      const response = await request(baseURL)
        .post('/auth/introspect')
        .send({
          token: tamperedToken
        })
        .expect(200);

      expect(response.body.active).toBe(false);
    });

    test('должен отклонять токен с измененным payload', async () => {
      const loginResponse = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      const validToken = loginResponse.body.access_token;
      const parts = validToken.split('.');

      // Изменить payload (добавить admin роль)
      const payload = JSON.parse(Buffer.from(parts[1], 'base64').toString());
      payload.role = 'admin';
      const tamperedPayload = Buffer.from(JSON.stringify(payload)).toString('base64')
        .replace(/\+/g, '-').replace(/\//g, '_').replace(/=/g, '');

      const tamperedToken = `${parts[0]}.${tamperedPayload}.${parts[2]}`;

      const response = await request(baseURL)
        .post('/auth/introspect')
        .send({
          token: tamperedToken
        })
        .expect(200);

      expect(response.body.active).toBe(false);
    });

    test('должен отклонять токен без подписи', async () => {
      const loginResponse = await request(baseURL)
        .post('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        })
        .expect(200);

      const parts = loginResponse.body.access_token.split('.');
      const tokenWithoutSignature = `${parts[0]}.${parts[1]}.`;

      const response = await request(baseURL)
        .post('/auth/introspect')
        .send({
          token: tokenWithoutSignature
        })
        .expect(200);

      expect(response.body.active).toBe(false);
    });
  });

  describe('Concurrent Requests', () => {
    test('должен корректно обрабатывать множественные одновременные login запросы', async () => {
      const loginPromises = Array(10).fill(null).map(() =>
        request(baseURL)
          .post('/auth/login')
          .send({
            username: 'testuser',
            password: 'password123'
          })
      );

      const responses = await Promise.all(loginPromises);

      // Все запросы должны быть успешными
      responses.forEach(response => {
        expect(response.status).toBe(200);
        expect(response.body).toHaveProperty('access_token');
        expect(response.body).toHaveProperty('refresh_token');
      });

      // Все refresh токены должны быть уникальными
      const refreshTokens = responses.map(r => r.body.refresh_token);
      const uniqueTokens = new Set(refreshTokens);
      expect(uniqueTokens.size).toBe(refreshTokens.length);
    });

  });

  describe('Special Characters Handling', () => {
    test('должен корректно обрабатывать специальные символы в username', async () => {
      const specialChars = ['user@domain', 'user.name', 'user-name', 'user_name'];

      for (const username of specialChars) {
        const response = await request(baseURL)
          .post('/auth/login')
          .send({
            username: username,
            password: 'password123'
          })
          .expect(401); // Пользователь не существует

        expect(response.body).toHaveProperty('error');
      }
    });

    test('должен корректно обрабатывать unicode символы', async () => {
      const unicodeUsernames = ['пользователь', 'użytkownik', '用户', '🔒user'];

      for (const username of unicodeUsernames) {
        const response = await request(baseURL)
          .post('/auth/login')
          .send({
            username: username,
            password: 'password123'
          })
          .expect(401);

        expect(response.body).toHaveProperty('error');
      }
    });
  });

  describe('Content-Type Security', () => {
    test('должен отклонять запросы без Content-Type: application/json', async () => {
      const response = await request(baseURL)
        .post('/auth/login')
        .set('Content-Type', 'text/plain')
        .send('username=testuser&password=password123');

      // Может быть 400 или 415 (Unsupported Media Type)
      expect([400, 415]).toContain(response.status);
    });
  });

  describe('HTTP Methods Security', () => {
    test('должен отклонять GET запросы к POST endpoints', async () => {
      const response = await request(baseURL)
        .get('/auth/login')
        .expect(404); // Method Not Allowed или Not Found

      expect(response.status).toBeGreaterThanOrEqual(400);
    });

    test('должен отклонять PUT запросы к POST endpoints', async () => {
      const response = await request(baseURL)
        .put('/auth/login')
        .send({
          username: 'testuser',
          password: 'password123'
        });

      expect(response.status).toBeGreaterThanOrEqual(400);
    });
  });
});
