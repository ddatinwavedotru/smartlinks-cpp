#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace smartlinks::dsl::parser::utils {

/**
 * @brief Удалить пробелы в начале и конце строки
 */
inline std::string Trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(),
        [](unsigned char c) { return std::isspace(c); });

    auto end = std::find_if_not(str.rbegin(), str.rend(),
        [](unsigned char c) { return std::isspace(c); }).base();

    return (start < end) ? std::string(start, end) : std::string();
}

/**
 * @brief Разбить строку по разделителю с учетом скобок
 *
 * Примеры:
 * - SplitRespectingParentheses("A OR B OR C", "OR") -> ["A", "B", "C"]
 * - SplitRespectingParentheses("A OR (B OR C)", "OR") -> ["A", "(B OR C)"]
 *
 * @param str Входная строка
 * @param delimiter Разделитель (например, "OR", "AND")
 * @return Список подстрок
 */
inline std::vector<std::string> SplitRespectingParentheses(
    const std::string& str,
    const std::string& delimiter
) {
    std::vector<std::string> result;
    int parentheses_depth = 0;
    size_t last_split = 0;

    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '(') {
            parentheses_depth++;
        } else if (str[i] == ')') {
            parentheses_depth--;
        } else if (parentheses_depth == 0) {
            // Проверить, совпадает ли с delimiter
            if (str.substr(i, delimiter.length()) == delimiter) {
                // Для символьных разделителей (;, ,) не проверяем границы слов
                // Для словесных (AND, OR) проверяем, что это отдельное слово
                bool is_symbol_delimiter = (delimiter.length() == 1 && !std::isalnum(delimiter[0]));

                bool should_split = is_symbol_delimiter;

                if (!is_symbol_delimiter) {
                    // Проверить, что это отдельное слово (не часть другого слова)
                    bool is_word_boundary_before = (i == 0 || std::isspace(str[i - 1]));
                    bool is_word_boundary_after = (i + delimiter.length() >= str.length() ||
                                                   std::isspace(str[i + delimiter.length()]));
                    should_split = (is_word_boundary_before && is_word_boundary_after);
                }

                if (should_split) {
                    // Найден разделитель
                    result.push_back(Trim(str.substr(last_split, i - last_split)));
                    last_split = i + delimiter.length();
                    i = last_split - 1;  // -1 потому что цикл сделает ++i
                }
            }
        }
    }

    // Добавить последнюю часть
    if (last_split < str.length()) {
        result.push_back(Trim(str.substr(last_split)));
    }

    // Если разделитель не найден, вернуть всю строку
    if (result.empty()) {
        result.push_back(Trim(str));
    }

    return result;
}

/**
 * @brief Проверить, начинается ли строка со скобки (после trim)
 */
inline bool StartsWithParenthesis(const std::string& str) {
    auto trimmed = Trim(str);
    return !trimmed.empty() && trimmed[0] == '(';
}

/**
 * @brief Извлечь содержимое из скобок
 *
 * Примеры:
 * - "(A OR B)" -> "A OR B"
 * - "((A))" -> "(A)"
 *
 * @param str Строка со скобками
 * @return Содержимое без внешних скобок, или пустая строка если ошибка
 */
inline std::string ExtractParenthesesContent(const std::string& str) {
    auto trimmed = Trim(str);

    if (trimmed.empty() || trimmed.front() != '(' || trimmed.back() != ')') {
        return "";  // Ошибка: нет скобок
    }

    // Удалить внешние скобки
    return Trim(trimmed.substr(1, trimmed.length() - 2));
}

} // namespace smartlinks::dsl::parser::utils
