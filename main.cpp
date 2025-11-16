#include <iostream>
#include <string>
#include "minidbms.h"

using namespace std;

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        cerr << "Usage: " << argv[0] << " <db_name> <command> <query_json>" << endl;
        cerr << "Commands supported:" << endl;
        cerr << "  insert: <query_json>" << endl;
        cerr << "  find:   <query_json>" << endl;
        cerr << "  delete: <query_json>" << endl;
        cerr << "Example: " << argv[0]
             << " mydb insert '{\"name\":\"Alice\",\"age\":25,\"city\":\"London\"}'"
             << endl;
        cerr << "Example: " << argv[0]
             << " mydb find '{\"age\":{\"$gt\":20,\"$lt\":30}}'"
             << endl;
        return 1;
    }

    string db_name = argv[1];
    string command = argv[2];
    string query_json = argv[3];

    // Если JSON обёрнут в одинарные кавычки '...'
    if (query_json.length() >= 2 &&
        query_json.front() == '\'' &&
        query_json.back() == '\'')
    {
        query_json = query_json.substr(1, query_json.length() - 2);
    }

    MiniDBMS dbms(db_name);
    dbms.run(command, query_json);

    return 0;
}
