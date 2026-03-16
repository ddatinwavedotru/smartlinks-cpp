#pragma once

namespace smartlinks::domain {

/**
 * @brief Интерфейс для проверки поддержки HTTP метода
 *
 * Аналог ISupportedHttpRequest из C# версии.
 * Проверяет, является ли HTTP метод допустимым для SmartLink.
 */
class ISupportedHttpRequest {
public:
    virtual ~ISupportedHttpRequest() = default;

    /**
     * @brief Проверить, поддерживается ли HTTP метод запроса
     * @return true если метод GET или HEAD, false иначе
     *
     * SmartLink поддерживает только GET и HEAD запросы.
     * Для всех остальных методов должен возвращаться 405 Method Not Allowed.
     */
    virtual bool MethodIsSupported() = 0;
};

} // namespace smartlinks::domain
