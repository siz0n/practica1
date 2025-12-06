#include <iostream>
#include <string>
#include <cstdlib>
#include <cctype>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "utills.h"


// to-lower для команд
static std::string toLower(const std::string& s)
{
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(),
                   [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    return res;
}

// экранирование строки для JSON (" \ \n ...)
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

// отправка всей строки
bool writeAll(int sock, const std::string& data)
{
    const char* buf = data.c_str();
    std::size_t total = data.size();
    std::size_t sent = 0;

    while (sent < total)
    {
        ssize_t n = send(sock, buf + sent, total - sent, 0);
        if (n <= 0)
        {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

// чтение одной строки до '\n'
bool readLine(int sock, std::string& out)
{
    out.clear();
    char ch = 0;
    while (true)
    {
        ssize_t n = recv(sock, &ch, 1, 0);
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

// ----------------------------
// Парсинг команд пользователя
// ----------------------------
//
// Форматы (вводишь в клиенте):
//   INSERT { ...json объекта... }
//   FIND   { ...json условия... }
//   DELETE { ...json условия... }
//
// database берётся из параметра --database
//
// На выходе: готовая JSON-строка запроса для сервера.
//
// Примеры:
//
//   INSERT {"name":"Alice","age":25}
//
//  -> {"database":"mydb","operation":"insert",
//      "data":[{"name":"Alice","age":25}],
//      "query":{}}
//
//   FIND {"age":{"$gt":20}}
//
//  -> {"database":"mydb","operation":"find",
//      "data":[],
//      "query":{"age":{"$gt":20}}}
//
static bool buildJsonRequestFromCommand(const std::string& line,
                                        const std::string& database,
                                        std::string& outJson)
{
    std::string trimmed = trim(line);
    if (trimmed.empty())
    {
        return false;
    }

    // первый токен — команда (insert/find/delete)
    std::size_t spacePos = trimmed.find(' ');
    std::string cmd = (spacePos == std::string::npos)
                          ? trimmed
                          : trimmed.substr(0, spacePos);
    std::string rest =
        (spacePos == std::string::npos)
            ? std::string()
            : trim(trimmed.substr(spacePos + 1));

    std::string op = toLower(cmd); // insert/find/delete

    if (op != "insert" && op != "find" && op != "delete")
    {
        std::cerr << "Unknown command: " << cmd
                  << " (use INSERT, FIND, DELETE)\n";
        return false;
    }

    // Для find/delete, если условия нет — считаем "{}"
    std::string queryJson = "{}";
    if (op == "find" || op == "delete")
    {
        if (!rest.empty())
        {
            queryJson = rest; // предполагается, что это валидный JSON-объект
        }
    }

    // Для insert:
    //  rest должен быть либо { ... } (один документ), либо [ {...}, {...} ] (массив).
    //  Мы всё равно оборачиваем один объект в массив.
    std::string dataJson = "[]";
    if (op == "insert")
    {
        if (rest.empty())
        {
            std::cerr << "INSERT требует JSON-документ после команды.\n";
            return false;
        }

        if (!rest.empty() && rest.front() == '[')
        {
            // уже массив
            dataJson = rest;
        }
        else
        {
            // один объект, оборачиваем в массив
            dataJson = "[" + rest + "]";
        }

        queryJson = "{}"; // для insert фильтр не нужен
    }

    // Собираем JSON-запрос:
    // {
    //   "database": "<db>",
    //   "operation": "insert"|"find"|"delete",
    //   "data": [...],
    //   "query": {...}
    // }
    std::string json;
    json.reserve(256 + dataJson.size() + queryJson.size());

    json += "{";
    json += "\"database\":\"";
    json += escapeJsonString(database);
    json += "\",";

    json += "\"operation\":\"";
    json += escapeJsonString(op);
    json += "\",";

    json += "\"data\":";
    json += dataJson;
    json += ",";

    json += "\"query\":";
    json += queryJson;

    json += "}";
    json += "\n"; // сервер ждёт строку, заканчивающуюся \n

    outJson = json;
    return true;
}

int main(int argc, char* argv[])
{
    // значения по умолчанию
    std::string host = "127.0.0.1";
    int port = 5000;
    std::string database = "mydb";
    std::string onceCommand;
    bool onceMode = false;

    // простой парсер аргументов:
    //   --host <host>
    //   --port <port>
    //   --database <name>
    //   --once "<command>"
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--host" && i + 1 < argc)
        {
            host = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            port = std::atoi(argv[++i]);
        }
        else if (arg == "--database" && i + 1 < argc)
        {
            database = argv[++i];
        }
        else if (arg == "--once" && i + 1 < argc)
        {
            onceMode = true;
            onceCommand = argv[++i];
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: " << argv[0]
                      << " --host <host> --port <port> --database <name>"
                      << " [--once \"COMMAND\"]\n\n"
                      << "Examples (interactive):\n"
                      << "  INSERT {\"name\":\"Alice\",\"age\":25}\n"
                      << "  FIND   {\"age\":{\"$gt\":20}}\n"
                      << "  DELETE {\"name\":\"Alice\"}\n\n"
                      << "One-shot example:\n"
                      << "  " << argv[0]
                      << " --host 127.0.0.1 --port 5555 --database mydb \\\n"
                      << "     --once \"FIND {\\\"age\\\":{\\\"$gt\\\":20}}\"\n";
            return 0;
        }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            std::cerr << "Use --help for usage.\n";
            return 1;
        }
    }

    // создаём сокет
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    // настраиваем адрес сервера
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    // допускаем и IP, и "localhost"
    if (host == "localhost")
    {
        host = "127.0.0.1";
    }

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid host/IP address: " << host << "\n";
        close(sock);
        return 1;
    }

    // подключаемся
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        perror("connect");
        close(sock);
        return 1;
    }

    std::cout << "Connected to " << host << ":" << port
              << " (database: " << database << ")\n";

    // ----------------------------
    // РЕЖИМ ОДНОГО ЗАПРОСА
    // ----------------------------
    if (onceMode)
    {
        std::string reqJson;
        if (!buildJsonRequestFromCommand(onceCommand, database, reqJson))
        {
            std::cerr << "Failed to build request from --once command.\n";
            close(sock);
            return 1;
        }

        if (!writeAll(sock, reqJson))
        {
            std::cerr << "Send error\n";
            close(sock);
            return 1;
        }

        std::string respLine;
        if (!readLine(sock, respLine))
        {
            std::cerr << "Disconnected from server\n";
            close(sock);
            return 1;
        }

        std::cout << respLine << "\n";
        close(sock);
        return 0;
    }

    // ----------------------------
    // ИНТЕРАКТИВНЫЙ РЕЖИМ
    // ----------------------------
    while (true)
    {
        std::cout << "[" << database << "] > ";
        std::string line;
        if (!std::getline(std::cin, line))
        {
            break; // EOF / Ctrl+D
        }

        std::string trimmed = trim(line);
        std::string lowered = toLower(trimmed);

        if (lowered == "exit" || lowered == "quit")
        {
            break;
        }

        if (trimmed.empty())
        {
            continue;
        }

        std::string reqJson;
        if (!buildJsonRequestFromCommand(line, database, reqJson))
        {
            // ошибка уже вывели в stderr, просто продолжаем
            continue;
        }

        if (!writeAll(sock, reqJson))
        {
            std::cerr << "Send error\n";
            break;
        }

        std::string respLine;
        if (!readLine(sock, respLine))
        {
            std::cerr << "Disconnected from server\n";
            close(sock);
            return 1;
        }

        // Пока просто печатаем JSON-ответ как есть
        std::cout << respLine << "\n";
    }

    close(sock);
    return 0;
}
