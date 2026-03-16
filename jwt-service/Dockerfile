FROM node:18-alpine

# Установить OpenSSL для генерации ключей
RUN apk add --no-cache openssl

WORKDIR /app

# Копировать package files
COPY package*.json ./

# Установить зависимости (production only)
RUN npm install --omit=dev

# Копировать исходный код
COPY src ./src

# Создать директорию для ключей
RUN mkdir -p /app/keys

# Expose port
EXPOSE 3000

# Entrypoint script для генерации ключей
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["node", "src/server.js"]
