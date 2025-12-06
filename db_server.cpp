#include "minidbms.h"
#include "protocol.h"
#include "request_handler.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

// =========================
// Структура для списка БД
// =========================

struct DbEntry
{
    std::string name;   // имя базы
    MiniDBMS* db;       // указатель на объект базы
    std::mutex mtx;     // мьютекс НА КОНКРЕТНУЮ БД
    DbEntry* next;      // односвязный список
};

static DbEntry* g_dbList = nullptr;
static std::mutex g_dbListMutex;

// =========================
// Работа с сокетом
// =========================

// чтение строки до '\n'
static bool readLine(int sock, std::string& out)
{
    out.clear();
    char ch = 0;
    while (true)
    {
        ssize_t n = ::recv(sock, &ch, 1, 0);
        if (n <= 0)
        {
            return false;
        }
        if (ch == '\n')
        {
            break;
        }
        out.push_back(ch);
    }
    return true;
}

// отправка всей строки
static bool writeAll(int sock, const std::string& data)
{
    const char* buf = data.c_str();
    std::size_t total = data.size();
    std::size_t sent = 0;

    while (sent < total)
    {
        ssize_t n = ::send(sock, buf + sent, total - sent, 0);
        if (n <= 0)
        {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

// =========================
// Вспомогательные JSON-функции
// =========================

// вытащить строковое поле: "key":"value"
static bool extractJsonStringField(const std::string& json,
                                   const std::string& key,
                                   std::string& out)
{
    std::string pattern = "\"" + key + "\"";
    std::size_t pos = json.find(pattern);
    if (pos == std::string::npos)
    {
        return false;
    }

    std::size_t colon = json.find(':', pos + pattern.size());
    if (colon == std::string::npos)
    {
        return false;
    }

    std::size_t first_quote = json.find('"', colon + 1);
    if (first_quote == std::string::npos)
    {
        return false;
    }

    std::size_t second_quote = json.find('"', first_quote + 1);
    if (second_quote == std::string::npos)
    {
        return false;
    }

    out = json.substr(first_quote + 1,
                      second_quote - first_quote - 1);
    return true;
}

// вытащить значение-подстроку (объект { ... } или массив [ ... ]) по ключу
static bool extractJsonValueField(const std::string& json,
                                  const std::string& key,
                                  std::string& out)
{
    std::string pattern = "\"" + key + "\"";
    std::size_t pos = json.find(pattern);
    if (pos == std::string::npos)
    {
        return false;
    }

    std::size_t colon = json.find(':', pos + pattern.size());
    if (colon == std::string::npos)
    {
        return false;
    }

    std::size_t start =
        json.find_first_not_of(" \t\n\r", colon + 1);
    if (start == std::string::npos)
    {
        return false;
    }

    char c = json[start];

    // объект { ... }
    if (c == '{')
    {
        int count = 0;
        std::size_t i = start;
        bool found_end = false;

        while (i < json.size())
        {
            if (json[i] == '{')
            {
                ++count;
            }
            else if (json[i] == '}')
            {
                --count;
                if (count == 0)
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
            return false;
        }

        out = json.substr(start, i - start);
        return true;
    }

    // массив [ ... ]
    if (c == '[')
    {
        int count = 0;
        std::size_t i = start;
        bool found_end = false;

        while (i < json.size())
        {
            if (json[i] == '[')
            {
                ++count;
            }
            else if (json[i] == ']')
            {
                --count;
                if (count == 0)
                {
                    ++i; // включаем ']'
                    found_end = true;
                    break;
                }
            }
            ++i;
        }

        if (!found_end)
        {
            return false;
        }

        out = json.substr(start, i - start);
        return true;
    }

    // другие случаи (число и т.п.) нам тут не нужны
    return false;
}

// разбор JSON-строки запроса в Request
static bool parseJsonRequest(const std::string& line, Request& req)
{
    req = Request{};

    // обязательные поля: database, operation (строки)
    if (!extractJsonStringField(line, "database", req.database))
    {
        return false;
    }
    if (!extractJsonStringField(line, "operation", req.operation))
    {
        return false;
    }

    // необязательные поля: data, query
    std::string data_value;
    if (extractJsonValueField(line, "data", data_value))
    {
        req.data_json = data_value;
    }

    std::string query_value;
    if (extractJsonValueField(line, "query", query_value))
    {
        req.query_json = query_value;
    }

    return true;
}

// экранировать строку для JSON
static std::string escapeJsonString(const std::string& s)
{
    std::string result;
    result.reserve(s.size() + 16);

    for (char c : s)
    {
        switch (c)
        {
        case '\\':
            result += "\\\\";
            break;
        case '\"':
            result += "\\\"";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result.push_back(c);
            break;
        }
    }

    return result;
}

// сериализация Response в JSON
static std::string serializeResponseToJson(const Response& resp)
{
    std::string json;
    json.reserve(128 + resp.data.size());

    json += "{";

    json += "\"status\":\"";
    json += escapeJsonString(resp.status);
    json += "\",";

    json += "\"message\":\"";
    json += escapeJsonString(resp.message);
    json += "\",";

    json += "\"count\":";
    json += std::to_string(resp.count);
    json += ",";

    // data — уже валидный JSON (обычно массив []), поэтому без кавычек
    json += "\"data\":";
    if (resp.data.empty())
    {
        json += "[]";
    }
    else
    {
        json += resp.data;
    }

    json += "}";
    json += "\n";

    return json;
}

// =========================
// Поиск/создание DbEntry
// =========================

static DbEntry* getOrCreateDbEntry(const std::string& dbName)
{
    std::lock_guard<std::mutex> lock(g_dbListMutex);

    // ищем уже существующую запись
    DbEntry* current = g_dbList;
    while (current != nullptr)
    {
        if (current->name == dbName)
        {
            return current;
        }
        current = current->next;
    }

    // не нашли — создаём новую базу
    MiniDBMS* db = new MiniDBMS(dbName);
    db->loadFromDisk();

    DbEntry* entry = new DbEntry;
    entry->name = dbName;
    entry->db   = db;
    entry->next = g_dbList; // вставляем в начало списка

    g_dbList = entry;

    return entry;
}

// =========================
// Обработка одного клиента
// =========================

static void handleClient(int clientSock)
{
    std::string line;

    while (readLine(clientSock, line))
    {
        if (line.empty())
        {
            continue;
        }

        Request req;
        if (!parseJsonRequest(line, req))
        {
            // Некорректный JSON-запрос → отправляем JSON-ошибку
            Response resp;
            resp.status  = "error";
            resp.message = "Invalid request JSON format";
            resp.count   = 0;
            resp.data    = "[]";

            std::string out = serializeResponseToJson(resp);
            (void)writeAll(clientSock, out);
            continue;
        }

        // Получаем (или создаём) запись для нужной базы
        DbEntry* entry = getOrCreateDbEntry(req.database);

        Response resp;
        {
            // Блокируем КОНКРЕТНУЮ БД на время операции
            std::lock_guard<std::mutex> dbLock(entry->mtx);
            resp = processRequest(req, *entry->db);
        }

        // Сериализуем ответ в JSON и отправляем
        std::string out = serializeResponseToJson(resp);
        if (!writeAll(clientSock, out))
        {
            // Ошибка отправки — выходим из цикла и закрываем сокет
            break;
        }
    }

    ::close(clientSock);
}

// =========================
// main
// =========================

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <port> <default_db_name>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string defaultDbName = argv[2];

    // заранее подгружаем дефолтную БД
    {
        DbEntry* entry = getOrCreateDbEntry(defaultDbName);
        (void)entry;
    }

    int listenSock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock < 0)
    {
        std::perror("socket");
        return 1;
    }

    int opt = 1;
    ::setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(listenSock,
               reinterpret_cast<sockaddr*>(&addr),
               sizeof(addr)) < 0)
    {
        std::perror("bind");
        ::close(listenSock);
        return 1;
    }

    if (::listen(listenSock, 16) < 0)
    {
        std::perror("listen");
        ::close(listenSock);
        return 1;
    }

    std::cout << "Server listening on port " << port << std::endl;

    while (true)
    {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientSock =
            ::accept(listenSock,
                     reinterpret_cast<sockaddr*>(&clientAddr),
                     &clientLen);

        if (clientSock < 0)
        {
            std::perror("accept");
            continue;
        }

        std::thread t(handleClient, clientSock);
        t.detach();
    }

    ::close(listenSock);
    return 0;
}
