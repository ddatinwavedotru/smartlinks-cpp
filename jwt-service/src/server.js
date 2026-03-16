const express = require('express');
const jwt = require('jsonwebtoken');
const fs = require('fs');
const bodyParser = require('body-parser');
const cors = require('cors');
const crypto = require('crypto');
const { MongoClient, ObjectId } = require('mongodb');
const bcrypt = require('bcrypt');

const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(bodyParser.json());

// Basic Auth middleware for admin page
function basicAuth(req, res, next) {
  const authHeader = req.headers.authorization;

  if (!authHeader || !authHeader.startsWith('Basic ')) {
    res.set('WWW-Authenticate', 'Basic realm="Admin Area"');
    return res.status(401).send('Authentication required');
  }

  const base64Credentials = authHeader.split(' ')[1];
  const credentials = Buffer.from(base64Credentials, 'base64').toString('ascii');
  const [username, password] = credentials.split(':');

  if (username === 'admin' && password === '&Admin123') {
    return next();
  }

  res.set('WWW-Authenticate', 'Basic realm="Admin Area"');
  return res.status(401).send('Invalid credentials');
}

// Конфигурация
const MONGO_URI = process.env.MONGO_URI || 'mongodb://root:password@mongodb:27017';
const MONGO_DB = process.env.MONGO_DB || 'Links';
const ACCESS_TOKEN_TTL = process.env.ACCESS_TOKEN_TTL || '10m';   // 10 минут
const REFRESH_TOKEN_TTL = process.env.REFRESH_TOKEN_TTL || '14d'; // 2 недели
const ISSUER = process.env.JWT_ISSUER || 'smartlinks-jwt-service';
const AUDIENCE = process.env.JWT_AUDIENCE || 'smartlinks-api';

// Пути к ключам
const PRIVATE_KEY_PATH = '/app/keys/private.key';
const PUBLIC_KEY_PATH = '/app/keys/public.key';

// Загрузка RSA ключей
let PRIVATE_KEY, PUBLIC_KEY;

try {
  PRIVATE_KEY = fs.readFileSync(PRIVATE_KEY_PATH, 'utf8');
  PUBLIC_KEY = fs.readFileSync(PUBLIC_KEY_PATH, 'utf8');
  console.log('RSA keys loaded successfully');
} catch (err) {
  console.error('Error loading RSA keys:', err.message);
  console.error('Make sure keys are generated in /app/keys/');
  process.exit(1);
}

// MongoDB клиент
let db;
MongoClient.connect(MONGO_URI)
  .then(client => {
    db = client.db(MONGO_DB);
    console.log('Connected to MongoDB');
  })
  .catch(err => {
    console.error('MongoDB connection error:', err);
    process.exit(1);
  });

// Helper: вычислить kid (key ID) для JWKS
function getKeyId() {
  const publicKeyDer = crypto.createPublicKey(PUBLIC_KEY).export({
    type: 'spki',
    format: 'der'
  });
  return crypto.createHash('sha256').update(publicKeyDer).digest('hex').substring(0, 16);
}

// Helper: конвертировать PEM в JWK формат
function pemToJwk(publicKeyPem) {
  const publicKey = crypto.createPublicKey(publicKeyPem);
  const jwk = publicKey.export({ format: 'jwk' });

  return {
    kty: 'RSA',
    use: 'sig',
    alg: 'RS256',
    kid: getKeyId(),
    n: jwk.n,
    e: jwk.e
  };
}

// GET /.well-known/jwks.json - публичные ключи (JWKS)
app.get('/.well-known/jwks.json', (req, res) => {
  try {
    const jwk = pemToJwk(PUBLIC_KEY);
    res.json({ keys: [jwk] });
  } catch (err) {
    console.error('JWKS error:', err);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// POST /auth/login - выдача access + refresh токенов
app.post('/auth/login', async (req, res) => {
  const { username, password } = req.body;

  if (!username || !password) {
    return res.status(400).json({ error: 'username and password required' });
  }

  try {
    // Проверить credentials в MongoDB
    const usersCollection = db.collection('users');
    const user = await usersCollection.findOne({ username });

    if (!user) {
      return res.status(401).json({ error: 'Invalid username or password' });
    }

    // Проверить активность пользователя
    if (!user.active) {
      return res.status(403).json({ error: 'User account is disabled' });
    }

    // Проверить пароль (bcrypt)
    const passwordMatch = await bcrypt.compare(password, user.password_hash);

    if (!passwordMatch) {
      return res.status(401).json({ error: 'Invalid username or password' });
    }

    const now = Math.floor(Date.now() / 1000);
    const userId = user._id.toString();

    // Генерация access token
    const accessToken = jwt.sign(
      {
        sub: userId,
        username: user.username,
        email: user.email || '',
        type: 'access'
      },
      PRIVATE_KEY,
      {
        algorithm: 'RS256',
        expiresIn: ACCESS_TOKEN_TTL,
        issuer: ISSUER,
        audience: AUDIENCE,
        keyid: getKeyId()
      }
    );

    // Генерация refresh token
    const refreshTokenId = crypto.randomUUID();
    const refreshToken = jwt.sign(
      {
        sub: userId,
        jti: refreshTokenId,
        type: 'refresh'
      },
      PRIVATE_KEY,
      {
        algorithm: 'RS256',
        expiresIn: REFRESH_TOKEN_TTL,
        issuer: ISSUER,
        audience: AUDIENCE,
        keyid: getKeyId()
      }
    );

    // Сохранить refresh token в MongoDB
    const refreshTokensCollection = db.collection('refresh_tokens');
    const decoded = jwt.decode(refreshToken);

    await refreshTokensCollection.insertOne({
      jti: refreshTokenId,
      user_id: new ObjectId(userId),
      username: user.username,
      token: refreshToken,
      expires_at: new Date(decoded.exp * 1000),
      created_at: new Date(),
      revoked: false
    });

    console.log(`Login successful for user: ${username}`);

    res.json({
      access_token: accessToken,
      refresh_token: refreshToken,
      token_type: 'Bearer',
      expires_in: 600  // 10 минут в секундах
    });

  } catch (err) {
    console.error('Login error:', err);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// POST /auth/refresh - обновление access токена
app.post('/auth/refresh', async (req, res) => {
  const { refresh_token } = req.body;

  if (!refresh_token) {
    return res.status(400).json({ error: 'refresh_token required' });
  }

  try {
    // Проверить refresh token
    const decoded = jwt.verify(refresh_token, PUBLIC_KEY, {
      algorithms: ['RS256'],
      issuer: ISSUER,
      audience: AUDIENCE
    });

    // Проверить что это refresh token
    if (decoded.type !== 'refresh') {
      return res.status(401).json({ error: 'Invalid token type' });
    }

    // Проверить что токен не отозван
    const refreshTokensCollection = db.collection('refresh_tokens');
    const storedToken = await refreshTokensCollection.findOne({
      jti: decoded.jti,
      revoked: false
    });

    if (!storedToken) {
      return res.status(401).json({ error: 'Token revoked or not found' });
    }

    // Получить данные пользователя
    const usersCollection = db.collection('users');
    const user = await usersCollection.findOne({
      _id: new ObjectId(decoded.sub)
    });

    if (!user || !user.active) {
      return res.status(401).json({ error: 'User not found or disabled' });
    }

    // Выдать новый access token
    const accessToken = jwt.sign(
      {
        sub: decoded.sub,
        username: user.username,
        email: user.email || '',
        type: 'access'
      },
      PRIVATE_KEY,
      {
        algorithm: 'RS256',
        expiresIn: ACCESS_TOKEN_TTL,
        issuer: ISSUER,
        audience: AUDIENCE,
        keyid: getKeyId()
      }
    );

    console.log(`Token refreshed for user: ${user.username}`);

    res.json({
      access_token: accessToken,
      token_type: 'Bearer',
      expires_in: 600
    });

  } catch (err) {
    if (err.name === 'JsonWebTokenError' || err.name === 'TokenExpiredError') {
      return res.status(401).json({ error: 'Invalid or expired refresh token' });
    }
    console.error('Refresh error:', err);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// POST /auth/revoke - отзыв refresh токена (logout)
app.post('/auth/revoke', async (req, res) => {
  const { refresh_token } = req.body;

  if (!refresh_token) {
    return res.status(400).json({ error: 'refresh_token required' });
  }

  try {
    const decoded = jwt.decode(refresh_token);

    if (!decoded || !decoded.jti) {
      return res.status(400).json({ error: 'Invalid token' });
    }

    // Отозвать токен
    const refreshTokensCollection = db.collection('refresh_tokens');
    await refreshTokensCollection.updateOne(
      { jti: decoded.jti },
      { $set: { revoked: true, revoked_at: new Date() } }
    );

    console.log(`Token revoked: ${decoded.jti}`);

    res.json({ success: true });

  } catch (err) {
    console.error('Revoke error:', err);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// POST /auth/introspect - проверка токена (опционально)
app.post('/auth/introspect', (req, res) => {
  const { token } = req.body;

  if (!token) {
    return res.status(400).json({ error: 'token required' });
  }

  try {
    const decoded = jwt.verify(token, PUBLIC_KEY, {
      algorithms: ['RS256'],
      issuer: ISSUER,
      audience: AUDIENCE
    });

    res.json({
      active: true,
      sub: decoded.sub,
      username: decoded.username,
      type: decoded.type,
      exp: decoded.exp,
      iat: decoded.iat,
      iss: decoded.iss,
      aud: decoded.aud
    });

  } catch (err) {
    res.json({ active: false });
  }
});

// GET /auth/manipulate/ - Admin page
app.get('/auth/manipulate/', basicAuth, (req, res) => {
  const html = `
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>JWT Service - Admin Panel</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 12px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            padding: 40px;
        }
        h1 {
            color: #333;
            margin-bottom: 10px;
            font-size: 28px;
        }
        .subtitle {
            color: #666;
            margin-bottom: 40px;
            font-size: 14px;
        }
        .forms-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(500px, 1fr));
            gap: 30px;
        }
        .form-card {
            border: 1px solid #e0e0e0;
            border-radius: 8px;
            padding: 25px;
            background: #fafafa;
        }
        .form-card h2 {
            color: #667eea;
            font-size: 18px;
            margin-bottom: 20px;
            display: flex;
            align-items: center;
        }
        .form-card h2::before {
            content: '';
            width: 4px;
            height: 20px;
            background: #667eea;
            margin-right: 10px;
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #555;
            font-size: 14px;
            font-weight: 500;
        }
        input[type="text"],
        input[type="email"],
        input[type="password"],
        select,
        textarea {
            width: 100%;
            padding: 10px 12px;
            border: 1px solid #ddd;
            border-radius: 6px;
            font-size: 14px;
            font-family: inherit;
            transition: border-color 0.3s;
        }
        input:focus, select:focus, textarea:focus {
            outline: none;
            border-color: #667eea;
        }
        textarea {
            resize: vertical;
            min-height: 100px;
            font-family: monospace;
            font-size: 12px;
        }
        button {
            background: #667eea;
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 6px;
            font-size: 14px;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.3s;
            width: 100%;
        }
        button:hover {
            background: #5568d3;
        }
        button:disabled {
            background: #ccc;
            cursor: not-allowed;
        }
        .result {
            margin-top: 15px;
            padding: 12px;
            border-radius: 6px;
            font-size: 13px;
            display: none;
        }
        .result.success {
            background: #d4edda;
            border: 1px solid #c3e6cb;
            color: #155724;
            display: block;
        }
        .result.error {
            background: #f8d7da;
            border: 1px solid #f5c6cb;
            color: #721c24;
            display: block;
        }
        .token-display {
            background: #f8f9fa;
            border: 1px solid #dee2e6;
            border-radius: 4px;
            padding: 10px;
            margin-top: 10px;
            word-break: break-all;
            font-family: monospace;
            font-size: 11px;
            max-height: 200px;
            overflow-y: auto;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>JWT Service - Admin Panel</h1>
        <p class="subtitle">Manage users and tokens</p>

        <div class="forms-grid">
            <!-- Form 1: Register/Update User -->
            <div class="form-card">
                <h2>Register/Update User</h2>
                <form id="registerForm">
                    <div class="form-group">
                        <label for="reg_username">Username *</label>
                        <input type="text" id="reg_username" required>
                    </div>
                    <div class="form-group">
                        <label for="reg_password">Password *</label>
                        <input type="password" id="reg_password" required>
                    </div>
                    <div class="form-group">
                        <label for="reg_email">Email *</label>
                        <input type="email" id="reg_email" required>
                    </div>
                    <button type="submit">Create/Update User</button>
                    <div class="result" id="registerResult"></div>
                </form>
            </div>

            <!-- Form 2: Login to Get Tokens -->
            <div class="form-card">
                <h2>Login & Get Tokens</h2>
                <form id="loginForm">
                    <div class="form-group">
                        <label for="login_username">Username *</label>
                        <input type="text" id="login_username" required>
                    </div>
                    <div class="form-group">
                        <label for="login_password">Password *</label>
                        <input type="password" id="login_password" required>
                    </div>
                    <button type="submit">Login & Get Tokens</button>
                    <div class="result" id="loginResult"></div>
                    <div id="loginTokensContainer" style="display:none;">
                        <div style="margin-top: 10px;">
                            <label style="font-weight: 600; color: #667eea;">Access Token:</label>
                            <div class="token-display" id="loginAccessTokenDisplay"></div>
                        </div>
                        <div style="margin-top: 10px;">
                            <label style="font-weight: 600; color: #667eea;">Refresh Token:</label>
                            <div class="token-display" id="loginRefreshTokenDisplay"></div>
                        </div>
                    </div>
                </form>
            </div>

            <!-- Form 3: Get Access Token from Refresh Token -->
            <div class="form-card">
                <h2>Get Access Token</h2>
                <form id="accessTokenForm">
                    <div class="form-group">
                        <label for="at_refresh_token">Refresh Token *</label>
                        <textarea id="at_refresh_token" required placeholder="Paste refresh token here..."></textarea>
                    </div>
                    <button type="submit">Generate Access Token</button>
                    <div class="result" id="accessTokenResult"></div>
                    <div class="token-display" id="accessTokenDisplay" style="display:none;"></div>
                </form>
            </div>

            <!-- Form 4: Check Token Validity -->
            <div class="form-card">
                <h2>Check Token Validity</h2>
                <form id="introspectForm">
                    <div class="form-group">
                        <label for="it_user">Select User (optional)</label>
                        <select id="it_user">
                            <option value="">-- Select User --</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label for="it_token">Token *</label>
                        <textarea id="it_token" required placeholder="Paste access or refresh token here..."></textarea>
                    </div>
                    <button type="submit">Verify Token</button>
                    <div class="result" id="introspectResult"></div>
                    <div class="token-display" id="introspectDisplay" style="display:none;"></div>
                </form>
            </div>
        </div>
    </div>

    <script>
        // Load users into dropdown (Form 4)
        async function loadUsers() {
            try {
                const response = await fetch('/auth/admin/users', {
                    credentials: 'same-origin'
                });
                if (!response.ok) throw new Error('Failed to load users');

                const users = await response.json();
                const select = document.getElementById('it_user');

                // Keep first option, remove others
                while (select.options.length > 1) {
                    select.remove(1);
                }

                // Add user options
                users.forEach(user => {
                    const option = document.createElement('option');
                    option.value = user.username;
                    option.textContent = \`\${user.username} (\${user.email || 'no email'})\`;
                    select.appendChild(option);
                });
            } catch (err) {
                console.error('Error loading users:', err);
            }
        }

        // Form 1: Register/Update User
        document.getElementById('registerForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const resultDiv = document.getElementById('registerResult');
            const submitBtn = e.target.querySelector('button[type="submit"]');

            submitBtn.disabled = true;
            resultDiv.className = 'result';
            resultDiv.textContent = 'Processing...';
            resultDiv.style.display = 'block';

            try {
                const response = await fetch('/auth/admin/register', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    credentials: 'same-origin',
                    body: JSON.stringify({
                        username: document.getElementById('reg_username').value,
                        password: document.getElementById('reg_password').value,
                        email: document.getElementById('reg_email').value
                    })
                });

                const data = await response.json();

                if (response.ok) {
                    resultDiv.className = 'result success';
                    if (data.created) {
                        resultDiv.textContent = \`User '\${data.username}' created successfully!\`;
                    } else if (data.updated) {
                        resultDiv.textContent = \`User '\${data.username}' password updated successfully!\`;
                    }
                    e.target.reset();
                    loadUsers(); // Refresh user lists
                } else {
                    resultDiv.className = 'result error';
                    resultDiv.textContent = data.error || 'Failed to create/update user';
                }
            } catch (err) {
                resultDiv.className = 'result error';
                resultDiv.textContent = 'Network error: ' + err.message;
            } finally {
                submitBtn.disabled = false;
            }
        });

        // Form 2: Login & Get Tokens
        document.getElementById('loginForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const resultDiv = document.getElementById('loginResult');
            const tokensContainer = document.getElementById('loginTokensContainer');
            const accessTokenDiv = document.getElementById('loginAccessTokenDisplay');
            const refreshTokenDiv = document.getElementById('loginRefreshTokenDisplay');
            const submitBtn = e.target.querySelector('button[type="submit"]');

            submitBtn.disabled = true;
            resultDiv.className = 'result';
            resultDiv.textContent = 'Logging in...';
            resultDiv.style.display = 'block';
            tokensContainer.style.display = 'none';

            try {
                const response = await fetch('/auth/login', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        username: document.getElementById('login_username').value,
                        password: document.getElementById('login_password').value
                    })
                });

                const data = await response.json();

                if (response.ok) {
                    resultDiv.className = 'result success';
                    resultDiv.textContent = 'Login successful! Tokens generated.';
                    accessTokenDiv.textContent = data.access_token;
                    refreshTokenDiv.textContent = data.refresh_token;
                    tokensContainer.style.display = 'block';
                } else {
                    resultDiv.className = 'result error';
                    resultDiv.textContent = data.error || 'Login failed';
                }
            } catch (err) {
                resultDiv.className = 'result error';
                resultDiv.textContent = 'Network error: ' + err.message;
            } finally {
                submitBtn.disabled = false;
            }
        });

        // Form 3: Get Access Token
        document.getElementById('accessTokenForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const resultDiv = document.getElementById('accessTokenResult');
            const tokenDiv = document.getElementById('accessTokenDisplay');
            const submitBtn = e.target.querySelector('button[type="submit"]');

            submitBtn.disabled = true;
            resultDiv.className = 'result';
            resultDiv.textContent = 'Generating token...';
            resultDiv.style.display = 'block';
            tokenDiv.style.display = 'none';

            try {
                const response = await fetch('/auth/refresh', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        refresh_token: document.getElementById('at_refresh_token').value.trim()
                    })
                });

                const data = await response.json();

                if (response.ok) {
                    resultDiv.className = 'result success';
                    resultDiv.textContent = 'Access token generated successfully!';
                    tokenDiv.textContent = data.access_token;
                    tokenDiv.style.display = 'block';
                } else {
                    resultDiv.className = 'result error';
                    resultDiv.textContent = data.error || 'Failed to generate token';
                }
            } catch (err) {
                resultDiv.className = 'result error';
                resultDiv.textContent = 'Network error: ' + err.message;
            } finally {
                submitBtn.disabled = false;
            }
        });

        // Form 4: Check Token Validity
        document.getElementById('introspectForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const resultDiv = document.getElementById('introspectResult');
            const tokenDiv = document.getElementById('introspectDisplay');
            const submitBtn = e.target.querySelector('button[type="submit"]');

            submitBtn.disabled = true;
            resultDiv.className = 'result';
            resultDiv.textContent = 'Verifying token...';
            resultDiv.style.display = 'block';
            tokenDiv.style.display = 'none';

            try {
                const response = await fetch('/auth/introspect', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        token: document.getElementById('it_token').value.trim()
                    })
                });

                const data = await response.json();

                if (data.active) {
                    resultDiv.className = 'result success';
                    resultDiv.textContent = 'Token is VALID';
                    tokenDiv.textContent = JSON.stringify(data, null, 2);
                    tokenDiv.style.display = 'block';
                } else {
                    resultDiv.className = 'result error';
                    resultDiv.textContent = 'Token is INVALID or EXPIRED';
                }
            } catch (err) {
                resultDiv.className = 'result error';
                resultDiv.textContent = 'Network error: ' + err.message;
            } finally {
                submitBtn.disabled = false;
            }
        });

        // Load users on page load
        loadUsers();
    </script>
</body>
</html>
  `;

  res.send(html);
});

// GET /auth/admin/users - List all active users (admin only)
app.get('/auth/admin/users', basicAuth, async (req, res) => {
  try {
    const usersCollection = db.collection('users');
    const users = await usersCollection
      .find({ active: true }, { projection: { username: 1, email: 1, _id: 1 } })
      .toArray();

    res.json(users.map(u => ({
      id: u._id.toString(),
      username: u.username,
      email: u.email
    })));
  } catch (err) {
    console.error('List users error:', err);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// POST /auth/admin/register - Register new user or update existing user password (admin only)
app.post('/auth/admin/register', basicAuth, async (req, res) => {
  const { username, password, email } = req.body;

  if (!username || !password || !email) {
    return res.status(400).json({ error: 'username, password, and email are required' });
  }

  try {
    const usersCollection = db.collection('users');

    // Check if username already exists
    const existingUser = await usersCollection.findOne({ username });

    // Hash password
    const passwordHash = await bcrypt.hash(password, 10);

    if (existingUser) {
      // Update existing user's password and email
      await usersCollection.updateOne(
        { username },
        {
          $set: {
            password_hash: passwordHash,
            email,
            updated_at: new Date(),
            updated_by: 'admin'
          }
        }
      );

      console.log(`User password updated by admin: ${username}`);

      res.json({
        success: true,
        username,
        email,
        updated: true,
        id: existingUser._id.toString()
      });
    } else {
      // Create new user
      const newUser = {
        username,
        password_hash: passwordHash,
        email,
        active: true,
        created_at: new Date(),
        created_by: 'admin'
      };

      const result = await usersCollection.insertOne(newUser);

      console.log(`User created by admin: ${username}`);

      res.status(201).json({
        success: true,
        username,
        email,
        created: true,
        id: result.insertedId.toString()
      });
    }

  } catch (err) {
    console.error('Register user error:', err);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// GET /health - health check
app.get('/health', (req, res) => {
  res.json({
    status: 'healthy',
    mongodb: db ? 'connected' : 'disconnected'
  });
});

// Cleanup истекших refresh токенов (каждый час)
setInterval(async () => {
  try {
    const refreshTokensCollection = db.collection('refresh_tokens');
    const result = await refreshTokensCollection.deleteMany({
      expires_at: { $lt: new Date() }
    });
    if (result.deletedCount > 0) {
      console.log(`Cleaned up ${result.deletedCount} expired refresh tokens`);
    }
  } catch (err) {
    console.error('Cleanup error:', err);
  }
}, 3600000); // 1 час

app.listen(PORT, () => {
  console.log(`JWT service listening on port ${PORT}`);
  console.log(`JWKS endpoint: http://localhost:${PORT}/.well-known/jwks.json`);
  console.log(`Config: ACCESS_TOKEN_TTL=${ACCESS_TOKEN_TTL}, REFRESH_TOKEN_TTL=${REFRESH_TOKEN_TTL}`);
});
