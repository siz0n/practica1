#include "request_handler.h"



using namespace std;

#include "request_handler.h"



#include "utills.h" 

using namespace std;

Response processRequest(const Request& req, MiniDBMS& db)
{
    Response resp;

    try
    {
        // -------------------------
        // INSERT
        // -------------------------
        if (req.operation == "insert")
        {
            // В JSON-запросе данные для вставки должны быть в поле "data".
            // Мы ожидаем ТАМ либо один объект { ... }, либо массив [ {...}, {...} ].
            // Для простоты первой версии:
            //  - если data_json пустой, пробуем query_json (чтобы не ломать старый клиент);
            //  - если строка начинается с '[', пока можно просто считать, что там один объект
            //    и взять первый { ... }.

            std::string docs = req.data_json.empty() ? req.query_json : req.data_json;
            std::string trimmed = trim(docs);

            if (trimmed.empty())
            {
                resp.status  = "error";
                resp.message = "Пустые данные для вставки (поле data/query пусто)";
                return resp;
            }

            // Случай: один документ { ... }
            if (trimmed.front() == '{')
            {
                db.insertQuery(trimmed);
                db.saveToDisk();

                resp.status  = "success";
                resp.message = "Документ добавлен";
                resp.count   = 1;
            }
            // Случай: массив документов [ {...}, {...}, ... ]
            else if (trimmed.front() == '[')
            {
                // Простейший парсер массива объектов, похожий на load():
                // выдёргиваем каждый { ... } и отдаём в insertQuery.

                std::size_t pos = 0;
                std::size_t inserted = 0;

                while (pos < trimmed.size())
                {
                    // ищем начало объекта
                    std::size_t start_obj = trimmed.find('{', pos);
                    if (start_obj == std::string::npos)
                    {
                        break;
                    }

                    int bracket_count = 0;
                    bool found_end = false;
                    std::size_t i = start_obj;

                    // ищем конец объекта по балансу скобок
                    while (i < trimmed.size())
                    {
                        if (trimmed[i] == '{')
                        {
                            ++bracket_count;
                        }
                        else if (trimmed[i] == '}')
                        {
                            --bracket_count;
                            if (bracket_count == 0)
                            {
                                ++i; // включаем '}'
                                found_end = true;
                                break;
                            }
                        }
                        ++i;
                    }

                    if (!found_end)
                    {
                        break;
                    }

                    std::string obj_str = trimmed.substr(start_obj, i - start_obj);
                    obj_str = trim(obj_str);

                    if (!obj_str.empty())
                    {
                        db.insertQuery(obj_str);
                        ++inserted;
                    }

                    pos = i;
                }

                db.saveToDisk();

                resp.status  = "success";
                resp.message = "Документы добавлены";
                resp.count   = inserted;
                resp.data = "[]";
            }
            else
            {
                resp.status  = "error";
                resp.message = "Неверный формат данных для insert (ожидался { } или [ ])";
            }
        }
        // -------------------------
        // FIND
        // -------------------------
        else if (req.operation == "find")
        {
            std::string query = req.query_json;
            if (query.empty())
            {
                query = "{}";
            }

            std::string json_array;
            std::size_t count = 0U;

            db.findQueryToJsonArray(query, json_array, count);

            resp.data    = json_array;                   // теперь это именно JSON-массив
            resp.count   = count;
            resp.status  = "success";
            resp.message = "Fetched " + std::to_string(count) + " documents";
        }

        // -------------------------
        // DELETE
        // -------------------------
        else if (req.operation == "delete")
        {
            std::string query = req.query_json;
            if (query.empty())
            {
                query = "{}";
            }

            std::size_t removed = db.deleteQuery(query);
            db.saveToDisk();

            resp.count   = removed;
            resp.status  = "success";
            resp.message = "Удалено " + to_string(removed);
            resp.data = "[]";
        }
        // -------------------------
        // UNKNOWN
        // -------------------------
        else
        {
            resp.status  = "error";
            resp.message = "Неизвестная команда: " + req.operation;
        }
    }
    catch (const exception& ex)
    {
        resp.status  = "error";
        resp.message = ex.what();
    }
    catch (...)
    {
        resp.status  = "error";
        resp.message = "Неизвестная ошибка";
        resp.count = 0;
        resp.data = "[]";
    }

    return resp;
}
