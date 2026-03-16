// tests/jwt/health.test.js
// Тесты для health check endpoint

const request = require('supertest');
const { TEST_CONFIG, setupTestDB, teardownTestDB } = require('./setup');

const baseURL = TEST_CONFIG.JWT_SERVICE_URL;

describe('GET /health', () => {
  beforeAll(async () => {
    await setupTestDB();
  });

  afterAll(async () => {
    await teardownTestDB();
  });

  test('должен вернуть статус healthy', async () => {
    const response = await request(baseURL)
      .get('/health')
      .expect('Content-Type', /json/)
      .expect(200);

    expect(response.body).toHaveProperty('status', 'healthy');
  });

  test('должен показать статус MongoDB подключения', async () => {
    const response = await request(baseURL)
      .get('/health')
      .expect(200);

    expect(response.body).toHaveProperty('mongodb');
    expect(['connected', 'disconnected']).toContain(response.body.mongodb);
  });

  test('mongodb должен быть connected', async () => {
    const response = await request(baseURL)
      .get('/health')
      .expect(200);

    expect(response.body.mongodb).toBe('connected');
  });

  test('endpoint должен быть доступен без аутентификации', async () => {
    // Health check не требует токена
    const response = await request(baseURL)
      .get('/health')
      .expect(200);

    expect(response.body.status).toBe('healthy');
  });

  test('должен быстро отвечать (< 1000ms)', async () => {
    const start = Date.now();

    await request(baseURL)
      .get('/health')
      .expect(200);

    const duration = Date.now() - start;
    expect(duration).toBeLessThan(1000);
  });
});
