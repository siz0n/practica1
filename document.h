#pragma once

#include <string>
#include "myarray.h"
#include "utills.h"

class Document
{
public:
    std::string _id; // id документа
private:
    myarray keys; // ключи(name, city)
    myarray values;

public:
    Document(std::string id = ""); // конструктор
    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;

    void addField(const std::string &key, const std::string &value); // добавление полей
    bool getField(const std::string &key, std::string &out) const;   // чпроверка ключа

    std::string serialize() const; // возвращаем файл строкой
    static Document *deserialize(const std::string &json_line);
};