#!/bin/sh
# Генерация RSA ключей если они не существуют

PRIVATE_KEY_PATH="/app/keys/private.key"
PUBLIC_KEY_PATH="/app/keys/public.key"

if [ ! -f "$PRIVATE_KEY_PATH" ]; then
    echo "Generating RSA key pair (2048-bit)..."

    # Генерировать приватный ключ RSA 2048-bit
    openssl genrsa -out "$PRIVATE_KEY_PATH" 2048

    # Извлечь публичный ключ
    openssl rsa -in "$PRIVATE_KEY_PATH" -pubout -out "$PUBLIC_KEY_PATH"

    # Установить права доступа
    chmod 600 "$PRIVATE_KEY_PATH"
    chmod 644 "$PUBLIC_KEY_PATH"

    echo "RSA keys generated successfully:"
    echo "  Private: $PRIVATE_KEY_PATH"
    echo "  Public: $PUBLIC_KEY_PATH"
else
    echo "RSA keys already exist, skipping generation"
fi

# Показать информацию о ключах
echo "Key info:"
openssl rsa -in "$PRIVATE_KEY_PATH" -text -noout | head -n 1

# Выполнить команду
exec "$@"
