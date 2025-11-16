#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <filesystem>
namespace fs = std::filesystem;

// --- Утилита для очистки строк (удаление пробелов, табуляции, перевода строки) ---
std::string trim(const std::string &str)
{
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first)
    {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// --- 0. Самописный динамический массив строк (замена std::vector<std::string>) ---
class DynamicStringArray
{
private:
    std::string *data;
    size_t capacity;
    size_t size;

    void resize(size_t new_capacity)
    {
        if (new_capacity <= capacity)
            return;

        std::string *new_data = new std::string[new_capacity];
        for (size_t i = 0; i < size; ++i)
        {
            new_data[i] = data[i];
        }

        delete[] data;
        data = new_data;
        capacity = new_capacity;
    }

public:
    DynamicStringArray(size_t initial_capacity = 10)
        : capacity(initial_capacity), size(0)
    {
        if (capacity == 0)
            capacity = 1;
        data = new std::string[capacity];
    }

    ~DynamicStringArray()
    {
        delete[] data;
    }

    void push_back(const std::string &value)
    {
        if (size == capacity)
        {
            resize(capacity * 2);
        }
        data[size++] = value;
    }

    size_t getSize() const
    {
        return size;
    }

    std::string &operator[](size_t index)
    {
        return data[index];
    }

    // Константная версия
    const std::string &operator[](size_t index) const
    {
        return data[index];
    }
};

// --- 1. Структура для хранимых данных (имитация JSON-документа, без фиксированной схемы) ---
class Document
{
public:
    std::string _id; // всегда есть

private:
    DynamicStringArray keys;   // имена полей
    DynamicStringArray values; // значения полей как строки

public:
    Document(const std::string &id = "") : _id(id) {}

    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;

    // Добавить/перезаписать поле
    void addField(const std::string &key, const std::string &value)
    {
        // проверяем, есть ли уже такое поле
        for (size_t i = 0; i < keys.getSize(); ++i)
        {
            if (keys[i] == key)
            {
                values[i] = value;
                return;
            }
        }
        keys.push_back(key);
        values.push_back(value);
    }

    // Получить значение поля; true если поле есть
    bool getField(const std::string &key, std::string &out) const
    {
        for (size_t i = 0; i < keys.getSize(); ++i)
        {
            if (keys[i] == key)
            {
                out = values[i];
                return true;
            }
        }
        return false;
    }

    // Сериализация в простой JSON (все значения как строки)
    std::string serialize() const
    {
        std::string json = "{";

        // Всегда первым полем идёт _id
        json += "\"_id\":\"" + _id + "\"";

        // Остальные поля (кроме _id)
        for (size_t i = 0; i < keys.getSize(); ++i)
        {
            if (keys[i] == "_id")
                continue; // защита от дублирования

            json += ",\"" + keys[i] + "\":\"" + values[i] + "\"";
        }

        json += "}";
        return json;
    }

    // Примитивный парсер JSON-объекта одной строкой:
    // {"_id":"1","name":"Alice","age":25,"city":"London"}
    static Document *deserialize(const std::string &json_line)
    {
        std::string s = trim(json_line);
        if (s.size() < 2 || s.front() != '{' || s.back() != '}')
        {
            std::cerr << "ERROR: Invalid JSON object: " << json_line << std::endl;
            return nullptr;
        }

        Document *doc = new Document();
        size_t i = 1; // внутри фигурных скобок

        while (i < s.size() - 1)
        {
            // пропускаем пробелы и запятые
            while (i < s.size() - 1 && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' || s[i] == ','))
            {
                ++i;
            }
            if (i >= s.size() - 1)
                break;

            if (s[i] != '"')
            {
                std::cerr << "ERROR: Expected '\"' at position " << i << " in: " << s << std::endl;
                delete doc;
                return nullptr;
            }

            // ключ
            size_t key_start = i + 1;
            size_t key_end = s.find('"', key_start);
            if (key_end == std::string::npos)
            {
                std::cerr << "ERROR: Unterminated key string in: " << s << std::endl;
                delete doc;
                return nullptr;
            }
            std::string key = s.substr(key_start, key_end - key_start);
            i = key_end + 1;

            // двоеточие
            while (i < s.size() - 1 && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
                ++i;
            if (i >= s.size() - 1 || s[i] != ':')
            {
                std::cerr << "ERROR: Expected ':' after key in: " << s << std::endl;
                delete doc;
                return nullptr;
            }
            ++i;

            // значение
            while (i < s.size() - 1 && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
                ++i;
            if (i >= s.size() - 1)
            {
                std::cerr << "ERROR: Missing value for key in: " << s << std::endl;
                delete doc;
                return nullptr;
            }

            std::string value;
            if (s[i] == '"')
            {
                // строка
                size_t val_start = i + 1;
                size_t val_end = s.find('"', val_start);
                if (val_end == std::string::npos)
                {
                    std::cerr << "ERROR: Unterminated string value in: " << s << std::endl;
                    delete doc;
                    return nullptr;
                }
                value = s.substr(val_start, val_end - val_start);
                i = val_end + 1;
            }
            else
            {
                // число или "сырой" литерал до ',' или '}'
                size_t val_start = i;
                size_t val_end = s.find_first_of(",}", val_start);
                if (val_end == std::string::npos)
                {
                    std::cerr << "ERROR: Invalid value in: " << s << std::endl;
                    delete doc;
                    return nullptr;
                }
                value = trim(s.substr(val_start, val_end - val_start));
                i = val_end;
            }

            if (key == "_id")
            {
                // первый встретившийся _id побеждает, поле в массив не кладём
                if (doc->_id.empty())
                {
                    doc->_id = value;
                }
            }
            else
            {
                doc->addField(key, value);
            }
        }

        if (doc->_id.empty())
        {
            std::cerr << "ERROR: Document has no _id: " << s << std::endl;
            delete doc;
            return nullptr;
        }

        return doc;
    }
};

// --- 2. Структура узла для цепочки (метод цепочек) ---
struct ListNode
{
    std::string key;
    Document *value;
    ListNode *next;

    ListNode(const std::string &k, Document *v) : key(k), value(v), next(nullptr) {}
};

// --- 3. Класс CustomList (для бакетов) ---
class CustomList
{
public:
    ListNode *head;

    CustomList() : head(nullptr) {}

    // Деструктор очищает только узлы списка, не значения Document
    ~CustomList()
    {
        ListNode *current = head;
        while (current)
        {
            ListNode *next = current->next;
            // НЕ удаляем current->value, это делает CustomHashMap
            delete current;
            current = next;
        }
    }

    ListNode *find(const std::string &key) const
    {
        ListNode *current = head;
        while (current)
        {
            if (current->key == key)
            {
                return current;
            }
            current = current->next;
        }
        return nullptr;
    }

    Document *remove(const std::string &key)
    {
        ListNode *current = head;
        ListNode *prev = nullptr;

        while (current)
        {
            if (current->key == key)
            {
                if (prev)
                {
                    prev->next = current->next;
                }
                else
                {
                    head = current->next;
                }

                Document *removed_value = current->value;
                current->value = nullptr; // Узел больше не владеет документом
                delete current;
                return removed_value;
            }
            prev = current;
            current = current->next;
        }
        return nullptr;
    }
};

// --- 4. Класс CustomHashMap ---
class CustomHashMap
{
private:
    static const size_t DEFAULT_CAPACITY = 16;
    static constexpr float LOAD_FACTOR_THRESHOLD = 0.75f;

    CustomList *buckets;
    size_t capacity;
    size_t size;

    size_t _hash(const std::string &key) const
    {
        size_t hash_value = 0;
        unsigned int prime = 31;
        for (char c : key)
        {
            hash_value = hash_value * prime + (unsigned char)c;
        }
        return hash_value % capacity;
    }

    void _resize_and_rehash()
    {
        size_t old_capacity = capacity;
        CustomList *old_buckets = buckets;

        capacity *= 2;
        size = 0;
        buckets = new CustomList[capacity];

        std::cout << "--- INFO: Расширение HashMap. Новая емкость: " << capacity << " ---" << std::endl;

        for (size_t i = 0; i < old_capacity; ++i)
        {
            ListNode *current = old_buckets[i].head;
            while (current)
            {
                // Вставка без удаления старого значения, так как мы перемещаем владение
                put(current->key, current->value, false);
                current->value = nullptr; // Сброс владения
                current = current->next;
            }
        }

        delete[] old_buckets; // Деструктор CustomList очистит узлы
    }

public:
    CustomHashMap(size_t initial_capacity = DEFAULT_CAPACITY)
        : capacity(initial_capacity > 0 ? initial_capacity : DEFAULT_CAPACITY), size(0)
    {
        buckets = new CustomList[capacity];
    }

    // Деструктор очищает все документы, хранящиеся в HashMap
    ~CustomHashMap()
    {
        for (size_t i = 0; i < capacity; ++i)
        {
            ListNode *current = buckets[i].head;
            while (current)
            {
                delete current->value; // Удаляем сам документ
                current = current->next;
            }
        }
        delete[] buckets; // Деструктор CustomList очистит узлы
    }

    void put(const std::string &key, Document *value, bool delete_on_update = true)
    {
        std::string cleaned_key = trim(key);

        if (float(size) / capacity >= LOAD_FACTOR_THRESHOLD)
        {
            _resize_and_rehash();
        }

        size_t index = _hash(cleaned_key);
        ListNode *existing_node = buckets[index].find(cleaned_key);

        if (existing_node)
        {
            if (delete_on_update)
            {
                delete existing_node->value; // Удаляем старый документ
            }
            existing_node->value = value; // Присваиваем новый
        }
        else
        {
            ListNode *new_node = new ListNode(cleaned_key, value);
            new_node->next = buckets[index].head;
            buckets[index].head = new_node;
            size++;
        }
    }

    Document *get(const std::string &key) const
    {
        std::string cleaned_key = trim(key);
        size_t index = _hash(cleaned_key);
        ListNode *node = buckets[index].find(cleaned_key);
        return node ? node->value : nullptr;
    }

    Document *remove(const std::string &key)
    {
        std::string cleaned_key = trim(key);
        size_t index = _hash(cleaned_key);
        Document *removed_value = buckets[index].remove(cleaned_key);
        if (removed_value)
        {
            size--;
        }
        // Возвращаем удаленное значение, чтобы вызывающая сторона могла его очистить (handle_delete)
        return removed_value;
    }

    size_t getSize() const { return size; }
    size_t getCapacity() const { return capacity; }

    // Метод для итерации (обход всех бакетов)
    ListNode *getBucketHead(size_t index) const
    {
        if (index < capacity)
        {
            return buckets[index].head;
        }
        return nullptr;
    }
};

// --- 5. Класс MiniDBMS (Интеграция) ---
class MiniDBMS
{
private:
    std::string db_name;
    std::string db_folder;
    CustomHashMap data_store;
    long long next_id = 1;

    std::string generate_id()
    {
        return std::to_string(next_id++);
    }

    fs::path get_collection_path() const
    {
        return fs::path(db_folder) / (db_name + ".json");
    }

    /**
     * @brief Загрузка всех документов из файла коллекции в CustomHashMap.
     */
    void load()
    {
        fs::path path = get_collection_path();
        if (!fs::exists(path))
        {
            std::cout << "INFO: Collection file not found. Starting with empty database." << std::endl;
            next_id = 1;
            return;
        }

        std::ifstream file(path);
        if (!file.is_open())
        {
            std::cerr << "ERROR: Could not open collection file for reading." << std::endl;
            return;
        }

        std::string line;
        long long max_id = 0;

        while (std::getline(file, line))
        {
            if (line.empty())
                continue;

            Document *doc = Document::deserialize(line);
            if (doc)
            {
                data_store.put(doc->_id, doc);

                try
                {
                    long long current_id = std::stoll(doc->_id);
                    if (current_id > max_id)
                    {
                        max_id = current_id;
                    }
                }
                catch (const std::exception &e)
                {
                    std::cerr << "WARNING: Could not parse ID '" << doc->_id << "' to integer during load. " << e.what() << std::endl;
                }
            }
        }
        file.close();
        next_id = max_id + 1;
        std::cout << "INFO: Database loaded. Documents: " << data_store.getSize() << ". Next ID: " << next_id << std::endl;
    }

    /**
     * @brief Сохранение всех документов из CustomHashMap в файл коллекции.
     */
    void save() const
    {
        fs::path path = get_collection_path();
        fs::create_directories(db_folder);
        std::ofstream file(path);
        if (!file.is_open())
        {
            std::cerr << "ERROR: Could not open collection file for writing." << std::endl;
            return;
        }

        size_t count = 0;
        for (size_t i = 0; i < data_store.getCapacity(); ++i)
        {
            ListNode *current = data_store.getBucketHead(i);
            while (current)
            {
                file << current->value->serialize() << "\n";
                current = current->next;
                count++;
            }
        }

        file.close();
        std::cout << "INFO: Database saved successfully. Documents: " << count << ". File: " << path.string() << std::endl;
    }

    /**
     * @brief Проверяет, соответствует ли значение документа условию запроса (явный $eq или операторы $gt, $lt, $in).
     */
    bool is_integer_string(const std::string &s)
    {
        std::string t = trim(s);
        if (t.empty())
            return false;
        size_t i = 0;
        if (t[0] == '-' || t[0] == '+')
        {
            if (t.size() == 1)
                return false;
            i = 1;
        }
        for (; i < t.size(); ++i)
        {
            if (t[i] < '0' || t[i] > '9')
                return false;
        }
        return true;
    }

    // Вспомогательная рекурсивная функция для $like
    bool like_match_impl(const std::string &value,
                         const std::string &pattern,
                         size_t i,
                         size_t j)
    {
        if (j == pattern.size())
        {
            return i == value.size();
        }

        char pc = pattern[j];

        if (pc == '%')
        {
            // % – любое (возможно пустое) количество символов
            // 1) пропустить % (совпадение с пустой подстрокой)
            // 2) съесть один символ из value и снова пытаться матчить %
            return like_match_impl(value, pattern, i, j + 1) ||
                   (i < value.size() && like_match_impl(value, pattern, i + 1, j));
        }

        if (pc == '_')
        {
            // _ – ровно один любой символ
            return (i < value.size() &&
                    like_match_impl(value, pattern, i + 1, j + 1));
        }

        // Обычный символ – должен совпасть по значению
        return (i < value.size() &&
                value[i] == pc &&
                like_match_impl(value, pattern, i + 1, j + 1));
    }

    bool like_match(const std::string &value, const std::string &pattern)
    {
        return like_match_impl(value, pattern, 0, 0);
    }

    bool match_query_value(const std::string &doc_value_raw,
                           const std::string &query_value_obj)
    {
        std::string doc_value = trim(doc_value_raw);
        std::string trimmed_query = trim(query_value_obj);

        if (trimmed_query.empty())
            return false;

        // 1. Простой случай: не объект, а значение
        //    -> неявное равенство (=)
        if (trimmed_query.front() != '{')
        {
            // снять кавычки у значения в запросе, если это строка
            if (trimmed_query.length() >= 2 &&
                trimmed_query.front() == '"' &&
                trimmed_query.back() == '"')
            {
                trimmed_query = trimmed_query.substr(1, trimmed_query.length() - 2);
            }
            trimmed_query = trim(trimmed_query);

            // Если оба числа — сравниваем как int, иначе как строки
            if (is_integer_string(doc_value) && is_integer_string(trimmed_query))
            {
                try
                {
                    int lhs = std::stoi(doc_value);
                    int rhs = std::stoi(trimmed_query);
                    return lhs == rhs;
                }
                catch (...)
                {
                    return false;
                }
            }

            return doc_value == trimmed_query;
        }

        // 2. Внутри объекта – операторы $eq, $gt, $lt, $in, $like
        auto extract_operator_value = [&](const std::string &op_key) -> std::string
        {
            std::string op_search = "\"" + op_key + "\":";
            size_t pos = trimmed_query.find(op_search);
            if (pos == std::string::npos)
                return "";

            size_t start_search = pos + op_search.length();
            size_t start_val = trimmed_query.find_first_not_of(" \t\n\r", start_search);
            if (start_val == std::string::npos)
                return "";

            if (trimmed_query[start_val] == '"')
            {
                // строка
                size_t start_content = start_val + 1;
                size_t end_content = trimmed_query.find('"', start_content);
                if (end_content == std::string::npos)
                    return "";
                return trim(trimmed_query.substr(start_content,
                                                 end_content - start_content));
            }
            else
            {
                // число или сырой литерал
                size_t end_val = trimmed_query.find_first_of(",}", start_val);
                if (end_val == std::string::npos)
                    return "";
                return trim(trimmed_query.substr(start_val, end_val - start_val));
            }
        };

        bool has_any_operator = false;
        bool result = true;

        // --- $eq (явное равенство) ---
        std::string eq_val_str = extract_operator_value("$eq");
        if (!eq_val_str.empty())
        {
            has_any_operator = true;
            if (is_integer_string(doc_value) && is_integer_string(eq_val_str))
            {
                try
                {
                    int lhs = std::stoi(doc_value);
                    int rhs = std::stoi(eq_val_str);
                    result = result && (lhs == rhs);
                }
                catch (...)
                {
                    return false;
                }
            }
            else
            {
                result = result && (doc_value == eq_val_str);
            }
        }

        // --- $gt ---
        std::string gt_val_str = extract_operator_value("$gt");
        if (!gt_val_str.empty())
        {
            has_any_operator = true;
            if (is_integer_string(doc_value) && is_integer_string(gt_val_str))
            {
                try
                {
                    int lhs = std::stoi(doc_value);
                    int rhs = std::stoi(gt_val_str);
                    result = result && (lhs > rhs);
                }
                catch (...)
                {
                    return false;
                }
            }
            else
            {
                result = result && (doc_value > gt_val_str);
            }
        }

        // --- $lt ---
        std::string lt_val_str = extract_operator_value("$lt");
        if (!lt_val_str.empty())
        {
            has_any_operator = true;
            if (is_integer_string(doc_value) && is_integer_string(lt_val_str))
            {
                try
                {
                    int lhs = std::stoi(doc_value);
                    int rhs = std::stoi(lt_val_str);
                    result = result && (lhs < rhs);
                }
                catch (...)
                {
                    return false;
                }
            }
            else
            {
                result = result && (doc_value < lt_val_str);
            }
        }

        // --- $like ---
        std::string like_val_str = extract_operator_value("$like");
        if (!like_val_str.empty())
        {
            has_any_operator = true;
            result = result && like_match(doc_value, like_val_str);
        }

        // --- $in ---
        std::string in_search = "\"$in\":";
        size_t in_pos = trimmed_query.find(in_search);
        if (in_pos != std::string::npos)
        {
            has_any_operator = true;

            size_t array_start = trimmed_query.find('[', in_pos + in_search.length());
            if (array_start == std::string::npos)
                return false;
            size_t array_end = trimmed_query.find(']', array_start);
            if (array_end == std::string::npos)
                return false;

            std::string array_content =
                trimmed_query.substr(array_start + 1, array_end - array_start - 1);

            std::stringstream ss(array_content);
            std::string item;
            bool in_result = false;

            while (std::getline(ss, item, ','))
            {
                std::string trimmed_item = trim(item);

                if (trimmed_item.length() >= 2 &&
                    trimmed_item.front() == '"' &&
                    trimmed_item.back() == '"')
                {
                    trimmed_item =
                        trimmed_item.substr(1, trimmed_item.length() - 2);
                    trimmed_item = trim(trimmed_item);
                }
                else
                {
                    trimmed_item = trim(trimmed_item);
                }

                if (is_integer_string(doc_value) && is_integer_string(trimmed_item))
                {
                    try
                    {
                        int lhs = std::stoi(doc_value);
                        int rhs = std::stoi(trimmed_item);
                        if (lhs == rhs)
                        {
                            in_result = true;
                            break;
                        }
                    }
                    catch (...)
                    {
                        // игнорируем и считаем неравными
                    }
                }
                else if (doc_value == trimmed_item)
                {
                    in_result = true;
                    break;
                }
            }

            result = result && in_result;
        }

        if (!has_any_operator)
        {
            // В объекте нет ни одного из известных операторов –
            // считаем, что условие не поддерживается.
            return false;
        }

        return result;
    }

    /**
     * @brief Проверяет, соответствует ли документ всем условиям AND (неявный логический AND).
     * query_json должен быть объектом с условиями, например: {"name": "Alice", "age": 25}
     */
    bool match_and_query(const Document *doc, const std::string &query_json)
    {
        std::string query = trim(query_json);
        if (query.empty() || query == "{}")
            return true;

        // Удаление внешних фигурных скобок
        std::string content = query.substr(1, query.length() - 2);
        size_t current_pos = 0;

        while (current_pos < content.length())
        {
            // Пропуск разделителей и пробелов
            while (current_pos < content.length() &&
                   (content[current_pos] == ' ' || content[current_pos] == '\t' || content[current_pos] == ','))
            {
                current_pos++;
            }
            if (current_pos >= content.length())
                break;

            // Парсинг имени поля
            size_t start_key = content.find('"', current_pos);
            if (start_key == std::string::npos)
                break;
            size_t end_key = content.find('"', start_key + 1);
            if (end_key == std::string::npos)
                return false;
            std::string field_name = content.substr(start_key + 1, end_key - start_key - 1);

            // Поиск начала значения условия
            size_t start_val_search = content.find(':', end_key);
            if (start_val_search == std::string::npos)
                return false;
            size_t val_start_char = content.find_first_not_of(" \t", start_val_search + 1);
            if (val_start_char == std::string::npos)
                return false;

            // Определение конца значения условия
            size_t end_val = std::string::npos;
            bool is_number = false;

            char first_char = content[val_start_char];

            if (first_char == '{' || first_char == '[')
            {
                // Условие с оператором ($gt, $lt, $in) или вложенный объект/массив
                char open_char = first_char;
                char close_char = (open_char == '{') ? '}' : ']';

                int bracket_count = 0;
                end_val = val_start_char;
                while (end_val < content.length())
                {
                    if (content[end_val] == open_char)
                        bracket_count++;
                    if (content[end_val] == close_char)
                    {
                        bracket_count--;
                        if (bracket_count == 0)
                            break;
                    }
                    end_val++;
                }
                if (bracket_count != 0 || end_val >= content.length())
                    return false;
            }
            else if (first_char == '"')
            {
                // Строковое значение
                size_t temp_pos = val_start_char;
                do
                {
                    temp_pos = content.find('"', temp_pos + 1);
                    if (temp_pos == std::string::npos)
                        return false;
                    if (temp_pos == 0 || content[temp_pos - 1] != '\\')
                    {
                        end_val = temp_pos;
                        break;
                    }
                } while (temp_pos != std::string::npos);

                if (temp_pos == std::string::npos)
                    return false;
            }
            else
            {
                // Числовое значение
                size_t separator_pos = content.find_first_of(",}", val_start_char);
                size_t boundary = (separator_pos == std::string::npos) ? content.length() : separator_pos;
                end_val = content.find_last_not_of(" \t", boundary - 1);

                if (end_val == std::string::npos || end_val < val_start_char)
                    return false;

                is_number = true;
            }

            size_t length = end_val - val_start_char + 1;
            std::string condition_value = content.substr(val_start_char, length);

            // --- извлечение значения из документа ---
            std::string doc_value_raw;
            if (field_name == "_id")
            {
                doc_value_raw = doc->_id;
            }
            else
            {
                if (!doc->getField(field_name, doc_value_raw))
                {
                    // В документе нет такого поля – он не удовлетворяет условию AND
                    return false;
                }
            }

            // Проверка соответствия (логический AND)
            if (!match_query_value(doc_value_raw, condition_value))
            {
                return false;
            }

            // Обновление текущей позиции для парсинга следующего поля
            if (is_number)
            {
                current_pos = content.find_first_of(",}", end_val + 1);
                if (current_pos == std::string::npos)
                    current_pos = content.length();
            }
            else
            {
                current_pos = end_val + 1;
            }
        }

        return true;
    }

    /**
     * @brief Обрабатывает логический оператор $or.
     * Ожидаемый формат: {"$or": [ {condition1}, {condition2} ]}
     */
    /**
     * @brief Обрабатывает логический оператор $or.
     * Ожидаемый формат: {"$or": [ {condition1}, {condition2}, ... ]}
     * Документ удовлетворяет запросу, если ХОТЯ БЫ ОДНО из подусловий истинно.
     */
    bool handle_or_query(const Document *doc, const std::string &query_json)
    {
        std::string search_key = "\"$or\":";
        size_t pos = query_json.find(search_key);
        if (pos == std::string::npos)
            return false;

        // Находим начало и конец массива условий
        size_t array_start = query_json.find('[', pos + search_key.length());
        if (array_start == std::string::npos)
            return false;

        size_t array_end = query_json.find_last_of(']');
        if (array_end == std::string::npos || array_end < array_start)
            return false;

        std::string array_content =
            query_json.substr(array_start + 1, array_end - array_start - 1);

        int bracket_count = 0;
        size_t current_pos = 0;
        bool has_any_condition = false;

        while (current_pos < array_content.length())
        {
            // Ищем начало следующего объекта-условия
            size_t start_cond = array_content.find('{', current_pos);
            if (start_cond == std::string::npos)
                break;

            size_t end_cond = start_cond;
            bracket_count = 0;
            bool found_end = false;

            // Находим конец текущего объекта условия (учитывая вложенные { })
            while (end_cond < array_content.length())
            {
                if (array_content[end_cond] == '{')
                    bracket_count++;
                if (array_content[end_cond] == '}')
                {
                    bracket_count--;
                    if (bracket_count == 0)
                    {
                        found_end = true;
                        break;
                    }
                }
                end_cond++;
            }

            if (!found_end)
                return false;

            std::string sub_query =
                array_content.substr(start_cond, end_cond - start_cond + 1);
            has_any_condition = true;

            // Каждое подусловие внутри $or само по себе — неявный AND по полям
            if (match_and_query(doc, trim(sub_query)))
            {
                // Достаточно одного совпадения — $or выполняется
                return true;
            }

            current_pos = end_cond + 1;
        }

        // Если не было ни одного условия – считаем, что документ не подходит
        if (!has_any_condition)
            return false;

        // Ни одно условие не совпало
        return false;
    }

    bool handle_and_query(const Document *doc, const std::string &query_json)
    {
        std::string search_key = "\"$and\":";
        size_t pos = query_json.find(search_key);
        if (pos == std::string::npos)
            return false;

        // Находим начало и конец массива условий
        size_t array_start = query_json.find('[', pos + search_key.length());
        if (array_start == std::string::npos)
            return false;

        size_t array_end = query_json.find_last_of(']');
        if (array_end == std::string::npos || array_end < array_start)
            return false;

        std::string array_content =
            query_json.substr(array_start + 1, array_end - array_start - 1);

        int bracket_count = 0;
        size_t current_pos = 0;
        bool has_any_condition = false;

        while (current_pos < array_content.length())
        {
            size_t start_cond = array_content.find('{', current_pos);
            if (start_cond == std::string::npos)
                break;

            size_t end_cond = start_cond;
            bracket_count = 0;
            bool found_end = false;

            // Находим конец текущего объекта условия
            while (end_cond < array_content.length())
            {
                if (array_content[end_cond] == '{')
                    bracket_count++;
                if (array_content[end_cond] == '}')
                {
                    bracket_count--;
                    if (bracket_count == 0)
                    {
                        found_end = true;
                        break;
                    }
                }
                end_cond++;
            }

            if (!found_end)
                return false;

            std::string sub_query =
                array_content.substr(start_cond, end_cond - start_cond + 1);
            has_any_condition = true;

            // Каждое подусловие внутри $and - это неявный AND по своим полям
            if (!match_and_query(doc, trim(sub_query)))
            {
                // Для $and достаточно одного НЕсовпадения
                return false;
            }

            current_pos = end_cond + 1;
        }

        // Если не было ни одного условия – считаем, что документ не совпал
        return has_any_condition;
    }

    /**
     * @brief Главная функция проверки соответствия документа запросу.
     */
    /**
     * @brief Главная функция проверки соответствия документа запросу.
     */
    bool match_document(const Document *doc, const std::string &query_json)
    {
        std::string query = trim(query_json);
        if (query.empty() || query == "{}")
            return true;

        // Ожидаем объект
        if (query.front() == '{')
        {
            // Найти первый ключ top-level
            size_t first_quote = query.find('"');
            if (first_quote != std::string::npos)
            {
                size_t second_quote = query.find('"', first_quote + 1);
                if (second_quote != std::string::npos)
                {
                    std::string first_key =
                        query.substr(first_quote + 1,
                                     second_quote - first_quote - 1);

                    if (first_key == "$or")
                    {
                        return handle_or_query(doc, query);
                    }
                    if (first_key == "$and")
                    {
                        return handle_and_query(doc, query);
                    }
                }
            }
        }

        // По умолчанию – неявный AND по полям объекта
        return match_and_query(doc, query);
    }

    void handle_insert(const std::string &query_json)
    {
        std::string new_id = generate_id();

        // Обрезаем пробелы
        std::string trimmed = trim(query_json);
        if (trimmed.empty())
        {
            std::cerr << "ERROR: Empty insert JSON." << std::endl;
            return;
        }
        if (trimmed.front() != '{')
        {
            std::cerr << "ERROR: Insert JSON must be an object: " << trimmed << std::endl;
            return;
        }

        // Формируем полный JSON с _id:
        // было: {"name":"Alice","age":25}
        // станет: {"_id":"1","name":"Alice","age":25}
        std::string without_first_brace = trimmed.substr(1); // всё после '{'
        std::string full_json = "{\"_id\":\"" + new_id + "\"," + without_first_brace;

        Document *new_doc = Document::deserialize(full_json);
        if (!new_doc)
        {
            std::cerr << "ERROR: Failed to parse insert document." << std::endl;
            return;
        }

        data_store.put(new_doc->_id, new_doc);
        std::cout << "Document inserted successfully. " << new_id
                  << ". In-memory size: " << data_store.getSize() << std::endl;
    }

    void handle_find(const std::string &query_json)
    {
        size_t found_count = 0;
        std::cout << "INFO: Starting query: " << query_json << std::endl;

        // Полное сканирование коллекции (Full Table Scan)
        for (size_t i = 0; i < data_store.getCapacity(); ++i)
        {
            ListNode *current = data_store.getBucketHead(i);
            while (current)
            {
                if (match_document(current->value, query_json))
                { // Используем match_document
                    std::cout << "-> " << current->value->serialize() << std::endl;
                    found_count++;
                }
                current = current->next;
            }
        }

        std::cout << "SUCCESS: Found " << found_count << " document(s)." << std::endl;
    }

    void handle_delete(const std::string &query_json)
    {
        size_t deleted_count = 0;
        std::cout << "INFO: Starting delete query: " << query_json << std::endl;

        DynamicStringArray ids_to_delete;

        for (size_t i = 0; i < data_store.getCapacity(); ++i)
        {
            ListNode *current = data_store.getBucketHead(i);
            while (current)
            {
                if (match_document(current->value, query_json))
                { // Используем match_document
                    ids_to_delete.push_back(current->key);
                }
                current = current->next;
            }
        }

        // Выполняем удаление
        for (size_t i = 0; i < ids_to_delete.getSize(); ++i)
        {
            std::string id = ids_to_delete[i];
            Document *removed_doc = data_store.remove(id);
            if (removed_doc)
            {
                delete removed_doc; // Очищаем память удаленного документа
                deleted_count++;
            }
        }

        std::cout << "SUCCESS: Deleted " << deleted_count << " document(s)." << std::endl;
    }

public:
    MiniDBMS(const std::string &db_name, const std::string &db_folder = "mydb") : db_name(db_name), db_folder(db_folder) {}
    ~MiniDBMS() = default;

    void run(const std::string &command, const std::string &query_json)
    {
        load();

        if (command == "insert")
        {
            handle_insert(query_json);
        }
        else if (command == "find")
        {
            handle_find(query_json);
        }
        else if (command == "delete")
        {
            handle_delete(query_json);
        }
        else
        {
            std::cerr << "ERROR: Unknown command: " << command << std::endl;
        }

        save();
    }
};

// --- ОСНОВНАЯ ФУНКЦИЯ (АРГУМЕНТЫ КОМАНДНОЙ СТРОКИ) ---

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: ./no_sql_dbms <db_name> <command> '<query_json>'" << std::endl;
        std::cerr << "Commands supported:" << std::endl;
        std::cerr << "  insert: <query_json>" << std::endl;
        std::cerr << "  find: <query_json>" << std::endl;
        std::cerr << "  delete: <query_json>" << std::endl;
        std::cerr << "Example: ./no_sql_dbms my_database insert '{\"name\": \"Alice\", \"age\": 25, \"city\": \"London\"}'" << std::endl;
        std::cerr << "Example: ./no_sql_dbms my_database find '{\"age\": {\"$gt\": 20, \"$lt\": 30}}'" << std::endl;
        return 1;
    }

    std::string db_name = argv[1];
    std::string command = argv[2];
    std::string query_json = argv[3];

    // Удаление внешних кавычек (если есть)
    if (query_json.length() >= 2 && query_json.front() == '\'' && query_json.back() == '\'')
    {
        query_json = query_json.substr(1, query_json.length() - 2);
    }

    MiniDBMS dbms(db_name);
    dbms.run(command, query_json);

    return 0;
}