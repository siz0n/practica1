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
        if (req.operation == "insert")
        {
            string docs = req.data_json.empty() ? req.query_json : req.data_json;
            string trimmed = trim(docs);

            if (trimmed.empty())
            {
                resp.status = "error";
                resp.message = "Пустые данные для вставки (поле data/query пусто)";
                return resp;
            }

            if (trimmed.front() == '{')
            {
                db.insertQuery(trimmed);
                db.saveToDisk();

                resp.status = "success";
                resp.message = "Документ добавлен";
                resp.count = 1;
            }

            else if (trimmed.front() == '[')
            {
                size_t pos = 0;
                size_t inserted = 0;

                while (pos < trimmed.size())
                {
                    // ищем начало объекта
                    size_t start_obj = trimmed.find('{', pos);
                    if (start_obj == string::npos)
                    {
                        break;
                    }

                    int bracket_count = 0;
                    bool found_end = false;
                    size_t i = start_obj;

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

                    string obj_str = trimmed.substr(start_obj, i - start_obj);
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

        else if (req.operation == "find")
        {
            string query = req.query_json;
            if (query.empty())
            {
                query = "{}";
            }

            string json_array;
            size_t count = 0U;

            db.findQueryToJsonArray(query, json_array, count);

            resp.data = json_array;
            resp.count = count;
            resp.status = "success";
            resp.message = "Fetched " + to_string(count) + " documents";
        }

        // -------------------------
        // DELETE
        // -------------------------
        else if (req.operation == "delete")
        {
            string query = req.query_json;
            if (query.empty())
            {
                query = "{}";
            }

            size_t removed = db.deleteQuery(query);
            db.saveToDisk();

            resp.count   = removed;
            resp.status  = "success";
            resp.message = "Удалено " + to_string(removed);
            resp.data = "[]";
        }

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
