// tests/jwt/jwks.test.js
// Тесты для JWKS endpoint

const request = require('supertest');
const { TEST_CONFIG, setupTestDB, teardownTestDB } = require('./setup');

const baseURL = TEST_CONFIG.JWT_SERVICE_URL;

describe('GET /.well-known/jwks.json', () => {
  beforeAll(async () => {
    await setupTestDB();
  });

  afterAll(async () => {
    await teardownTestDB();
  });

  test('должен вернуть JWKS с публичными ключами', async () => {
    const response = await request(baseURL)
      .get('/.well-known/jwks.json')
      .expect('Content-Type', /json/)
      .expect(200);

    expect(response.body).toHaveProperty('keys');
    expect(Array.isArray(response.body.keys)).toBe(true);
    expect(response.body.keys.length).toBeGreaterThan(0);
  });

  test('ключ должен иметь корректную структуру JWK', async () => {
    const response = await request(baseURL)
      .get('/.well-known/jwks.json')
      .expect(200);

    const key = response.body.keys[0];

    expect(key).toHaveProperty('kty', 'RSA');
    expect(key).toHaveProperty('use', 'sig');
    expect(key).toHaveProperty('alg', 'RS256');
    expect(key).toHaveProperty('kid');
    expect(key).toHaveProperty('n'); // Modulus
    expect(key).toHaveProperty('e'); // Exponent
  });

  test('kid должен быть непустым', async () => {
    const response = await request(baseURL)
      .get('/.well-known/jwks.json')
      .expect(200);

    const key = response.body.keys[0];
    expect(key.kid).toBeTruthy();
    expect(key.kid.length).toBeGreaterThan(0);
  });

  test('модуль (n) должен быть base64url строкой', async () => {
    const response = await request(baseURL)
      .get('/.well-known/jwks.json')
      .expect(200);

    const key = response.body.keys[0];
    expect(key.n).toBeTruthy();
    expect(typeof key.n).toBe('string');
    // Base64url не должен содержать +, /, =
    expect(key.n).not.toMatch(/[+/=]/);
  });

  test('экспонент (e) должен быть AQAB (65537)', async () => {
    const response = await request(baseURL)
      .get('/.well-known/jwks.json')
      .expect(200);

    const key = response.body.keys[0];
    expect(key.e).toBe('AQAB'); // Стандартный RSA экспонент
  });

  test('должен возвращать один и тот же kid при повторных запросах', async () => {
    const response1 = await request(baseURL)
      .get('/.well-known/jwks.json')
      .expect(200);

    const response2 = await request(baseURL)
      .get('/.well-known/jwks.json')
      .expect(200);

    const kid1 = response1.body.keys[0].kid;
    const kid2 = response2.body.keys[0].kid;

    expect(kid1).toBe(kid2); // Ключ должен быть стабильным
  });

  test('токены должны использовать kid из JWKS', async () => {
    // Получить kid из JWKS
    const jwksResponse = await request(baseURL)
      .get('/.well-known/jwks.json')
      .expect(200);

    const expectedKid = jwksResponse.body.keys[0].kid;

    // Получить токен
    const loginResponse = await request(baseURL)
      .post('/auth/login')
      .send({
        username: 'testuser',
        password: 'password123'
      })
      .expect(200);

    // Декодировать header токена
    const header = JSON.parse(
      Buffer.from(loginResponse.body.access_token.split('.')[0], 'base64').toString()
    );

    expect(header).toHaveProperty('kid', expectedKid);
  });
});
