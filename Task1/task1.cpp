#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <thread>
#include <mutex>


// Основная структура для работы с адресами IPv6
// Будем хранить так для экономии места, т.к если хранить в строковом варианте, 
// то получим 39 байт на каждый адрес. (8 частей по 4 символа + 7 символов-разделителей)
// В таком варианте 1 запись занимает 16 байт
// храним по частям - первые 64 бита и последние 64 бита
// также реализованы два оператора: == и < для возможности сравнения
struct IPv6Addr {
    uint64_t hi;
    uint64_t lo;

    bool operator==(const IPv6Addr& other) const {
        return hi == other.hi && lo == other.lo;
    }

    bool operator<(const IPv6Addr& other) const {
        if (hi != other.hi)
            return hi < other.hi;
        return lo < other.lo;
    }
};

// Специализированная хэш-функция для структуры адреса
struct IPv6Addr_hash {
    std::size_t operator()(const IPv6Addr& addr) const {
        std::size_t h1 = std::hash<uint64_t>{}(addr.hi);
        std::size_t h2 = std::hash<uint64_t>{}(addr.lo);
        return h1 ^ (h2 << 1); 
    }
};

// Функция, приводящая строку с адресом к единому формату для дальнейшего сравнения
// адреса с остальными

// Как она работает: 
// 1. Всю полученную строку приводим к нижнему регистру
// 2. Ищем сокращение ('::').
//  a. Если есть, то делим полученную строку на левую и правую половинки, чтобы обработать каждую по отдельности.
//      Каждую половинку разбираем на части по символам ':'.
//      Далее собираем из всех частей канонизированный результат (с ведущими нулями и 
//          восстановленными пропусками, правда для простоты опустим добавление символов ':)
//  б. Если нет, то также разбиваем строку целиком на части по символу ':'.
//      И также из полученных частей собираем канонизированный результат
IPv6Addr parse_to_canon(const std::string& row) {
    std::array<uint16_t, 8> parts{};
    parts.fill(0);

    std::string s = row;

    // сначала приводим все имеющиеся символы к нижнему регистру
    std::transform(s.begin(), s.end(), s.begin(), 
    [](unsigned char c) {return std::tolower(c);});

    // далее получаем позицию ::
    size_t reduced = s.find("::");

    // если наш адрес был сокращен (в адресе есть ::),
    // то делим адрес на 2 половинки и обрабатываем каждую часть по отдельности
    if (reduced != std::string::npos) {
        std::string left_part = s.substr(0, reduced);
        std::string right_part = s.substr(reduced + 2);

        std::vector<std::string> left_parts;
        std::vector<std::string> right_parts;

        // разбираем на куски левую и правую половинки, чтобы потом собрать все по порядку,
        // добавить сокращенные нули и дописать ведущие, где потребуется
        if (!left_part.empty()) {
            std::stringstream ss(left_part);
            std::string part;

            while (std::getline(ss, part, ':')) {
                left_parts.push_back(part);
            }
        }

        if (!right_part.empty()) {
            std::stringstream ss(right_part);
            std::string part;

            while (std::getline(ss, part, ':')) {
                right_parts.push_back(part);
            }
        }

        // чтобы понять, сколько частей сократили в ::, надо
        // посчитать, сколько частей осталось в левой и правой половинках

        size_t num_left_parts = left_parts.size();
        size_t num_right_parts = right_parts.size();

        size_t num_zero = 8 - num_left_parts - num_right_parts;
        size_t idx = 0;

        for (const auto& part : left_parts) {
            parts[idx++] = static_cast<uint16_t>(std::stoul(part, nullptr, 16));
        }

        idx += num_zero;

        for (const auto& part : right_parts) {
            parts[idx++] = static_cast<uint16_t>(std::stoul(part, nullptr, 16));
        }

    } else {
        // если сокращения не было, то просто делим строку на части целиком
        std::stringstream ss(s);
        std::string part;

        size_t idx = 0;

        while (std::getline(ss, part, ':')) {
            parts[idx++] = static_cast<uint16_t>(std::stoul(part, nullptr, 16));
        }
    }

    // Собираем теперь из всех частей стандартизированный вариант адреса
    IPv6Addr res{};

    res.hi = 0;
    res.lo = 0;
    int i;

    for (i = 0; i < 4; ++i) {
        res.hi = (res.hi << 16) | parts[i];
    }

    for (i = 4; i < 8; ++i) {
        res.lo = (res.lo << 16) | parts[i];
    }

    return res;
};

// Базовый алгоритм, который использует хэш-таблицу. 
// Работает точно, но на больших файлах можем вылезти за ограничения памяти и скорости
size_t base_algorythm(const std::string& file_path) {
    std::ifstream input_file(file_path);

    std::unordered_set<IPv6Addr, IPv6Addr_hash> addresses{};

    if (input_file.is_open()) {
        std::string line;

        while (std::getline(input_file, line)) {
            addresses.insert(parse_to_canon(line));
        }
        
        size_t res = addresses.size();

        input_file.close();

        return res;
    }

    return 0;
}


// Оптимизированный алгоритм

// Функция распределения адресов по разным файлам, чтобы не читать весь большой файл в память
// Гарантируется, что одинаковые адреса в разные файлы не попадут, поскольку тут мы распределяем
// уже канонизированные адреса
void split_into_buckets(const std::string& file_path, int N) {
    std::vector<std::ofstream> buckets(N);

    for(int i = 0; i < N; ++i)
        buckets[i].open("temp_" + std::to_string(i) + ".bin", std::ios::binary);

    std::ifstream input_file(file_path);

    if (input_file.is_open()) {
        std::string line;

        while(std::getline(input_file, line)) {
            IPv6Addr addr = parse_to_canon(line);
            size_t idx = IPv6Addr_hash{}(addr) % N;

            buckets[idx].write(reinterpret_cast<char*>(&addr), sizeof(IPv6Addr));
        }

        input_file.close();
    }
}

// Подсчет уникальных адресов в одном bucket-файле
// Адреса уже канонизированы и хранятся в бинарном формате (по 16 байт каждый)
size_t count_bucket_unique(const std::string& file_path) {
    std::ifstream input_file(file_path, std::ios::binary);
    std::unordered_set<IPv6Addr, IPv6Addr_hash> addresses;
    IPv6Addr addr;

    if (input_file.is_open()) {
        while (input_file.read(reinterpret_cast<char*>(&addr), sizeof(IPv6Addr))) {
            addresses.insert(addr);
        }

        input_file.close();
    }

    return addresses.size();
}

// сам оптимизированный алгоритм, который сначала разбивает основной файл на N файлов меньше
// а затем параллельно считает число уникальный адресов в каждом из файлов
size_t optimized_algorythm(const std::string& file_path, int N) {
    std::vector<std::thread> threads;
    std::vector<size_t> results(N);

    split_into_buckets(file_path, N);

    for(int i = 0; i < N; ++i) {
        threads.emplace_back([i, &results]() {
            results[i] = count_bucket_unique("temp_" + std::to_string(i) + ".bin");
        });
    }

    for(auto& t : threads) t.join();

    for(int i = 0; i < N; ++i) {
        std::filesystem::remove("temp_" + std::to_string(i) + ".bin");
    }

    size_t total = 0;
    for(auto r : results) total += r;

    return total;
}


int main(int argc, char* argv[]) {
    std::string input_file_path = argv[1];
    std::string output_file_path = argv[2];
    size_t res;

    std::uintmax_t size = std::filesystem::file_size(input_file_path) / (1024 * 1024);
    
    // при размере файла <= 50 Мб используем базовый алгоритм, в ином случае оптимизированный
    if (size <= 50) {
        res = base_algorythm(input_file_path);
    } else {
        // эта формула выбрана для того, чтобы у нас даже большие файлы точно прошли по памяти
        int N = size / 350 + 1;
        res = optimized_algorythm(input_file_path, N);
    }

    std::ofstream output_file(output_file_path);
    if (output_file.is_open()) {
        output_file << res;
    }

    return 0;
}
