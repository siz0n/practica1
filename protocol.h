#pragma once

#include <string>
#include <cstddef> // для std::size_t

// Сетевой запрос в формате JSON:
//
// {
//   "database": "mydb",
//   "operation": "insert" | "find" | "delete",
//   "data":   [ {...}, {...} ],   // для insert (можно 0 или больше документов)
//   "query":  { ... }             // фильтр для find/delete
// }
//
// В рамках этой версии:
//  - database  и operation — ОБЯЗАТЕЛЬНЫ;
//  - data      и query     — опциональны.

struct Request
{ 
    std::string database;
    std::string operation;

    // Сырой JSON массива документов для вставки (поле "data" в запросе).
    // Может быть пустой строкой, если вставки нет.
    std::string data_json;

    // Сырой JSON-объект фильтра (поле "query" в запросе).
    // Может быть пустой строкой; тогда считаем, что "{}".
    std::string query_json;
};

struct Response
{
    // "success" или "error"
    std::string status;

    // Читаемое сообщение для пользователя
    std::string message;

    // Количество затронутых документов (найдено/удалено/вставлено)
    std::size_t count = 0;

    // Дополнительные данные.
    // В этой версии мы трактуем это как ПРОСТУЮ СТРОКУ (лог/результат),
    // а при отправке по сети кодируем её в JSON-строку.
    std::string data;
};
