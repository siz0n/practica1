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
    Response resp;
    resp.status  = "success";
    resp.message = "Документы добавлены";
    resp.data    = "[]";
    resp.count   = 0;

    std::string docs = !req.data_json.empty() ? req.data_json : req.query_json;
    std::string trimmed = trim(docs);

    if (trimmed.empty())
    {
        resp.status  = "error";
        resp.message = "Пустой JSON-документ";
        return resp;
    }

    if (trimmed.front() == '{')
    {
        db.insertQuery(trimmed);
        resp.count = 1;
    }
    else if (trimmed.front() == '[')
    {
        size_t pos = 0;
        while (pos < trimmed.size())
        {
            size_t s = trimmed.find('{', pos);
            if (s == std::string::npos) break;

            size_t cnt = 0;
            size_t i = s;
            do
            {
                if (trimmed[i] == '{') ++cnt;
                if (trimmed[i] == '}') --cnt;
                ++i;
            } while (i < trimmed.size() && cnt > 0);

            std::string obj = trimmed.substr(s, i - s);
            db.insertQuery(obj);
            ++resp.count;

            pos = i;
        }
    }
    else
    {
        resp.status  = "error";
        resp.message = "INSERT ожидает объект {} или массив []";
        return resp;
    }

    db.saveToDisk();
    return resp;
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
