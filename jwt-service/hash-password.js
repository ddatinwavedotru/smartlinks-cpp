const bcrypt = require('bcrypt');

const password = process.argv[2];

if (!password) {
  console.error('Usage: node hash-password.js <password>');
  process.exit(1);
}

bcrypt.hash(password, 10, (err, hash) => {
  if (err) {
    console.error('Error:', err);
    process.exit(1);
  }
  console.log('Password:', password);
  console.log('Hash:', hash);
  console.log('\nMongoDB insert command:');
  console.log(`db.users.insertOne({
  username: 'username',
  password_hash: '${hash}',
  email: 'user@example.com',
  created_at: new Date(),
  active: true
})`);
});
