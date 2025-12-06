#include <iostream>
#include <string>

#include "minidbms.h"
#include "protocol.h"
#include "request_handler.h"

int main()
{
    MiniDBMS db("mydb");    // имя базы
    db.loadFromDisk();      // загрузка при старте

    while (true) {
        std::cout << "> ";
        std::string op;
        if (!(std::cin >> op)) {
            break; // EOF / ошибка
        }

        std::string json;
        std::getline(std::cin, json); // забираем остаток строки
        if (!json.empty() && json[0] == ' ') {
            json.erase(0, 1); // убираем ведущий пробел
        }

        Request req;
        req.database   = "mydb";
        req.operation  = op;
        req.query_json = json;

        Response resp = processRequest(req, db);

        std::cout << "[" << resp.status << "] " << resp.message << "\n";
        if (!resp.data.empty()) {
            std::cout << resp.data << "\n";
        }
    }

    return 0;
}
