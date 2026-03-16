FROM node:18-alpine

WORKDIR /app/tests

# Копировать package files
COPY package*.json ./

# Установить зависимости
RUN npm install

# Копировать тестовые файлы
COPY *.js ./

# Дефолтная команда
CMD ["npm", "test"]
