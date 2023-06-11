#include <iostream>
#include <sstream>

#include <hoytech/error.h>
#include <hoytech/hex.h>

#include "Negentropy.h"



std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss (s);
    std::string item;

    while (getline (ss, item, delim)) {
        result.push_back (item);
    }

    return result;
}



int main() {
    const uint64_t idSize = 16;

    // x1 is client, x2 is server
    Negentropy x1(idSize);
    Negentropy x2(idSize);

    std::string line;
    while (std::cin) {
        std::getline(std::cin, line);
        if (!line.size()) continue;

        auto items = split(line, ',');
        if (items.size() != 3) throw hoytech::error("too few items");

        int mode = std::stoi(items[0]);
        uint64_t created = std::stoull(items[1]);
        auto id = hoytech::from_hex(items[2]);
        if (id.size() != idSize) throw hoytech::error("unexpected id size");

        if (mode == 1) {
            x1.addItem(created, id);
        } else if (mode == 2) {
            x2.addItem(created, id);
        } else if (mode == 3) {
            x1.addItem(created, id);
            x2.addItem(created, id);
        } else {
            throw hoytech::error("unexpected mode");
        }
    }

    x1.seal();
    x2.seal();

    std::string q;
    uint64_t round = 0;

    while (1) {
        // CLIENT -> SERVER

        if (round == 0) {
            uint64_t frameSizeLimit = 0;
            if (::getenv("FRAMESIZELIMIT")) frameSizeLimit = std::stoull(::getenv("FRAMESIZELIMIT"));
            q = x1.initiate(frameSizeLimit);
        } else {
            std::vector<std::string> have, need;
            q = x1.reconcile(q, have, need);

            for (auto &id : have) std::cout << "xor,HAVE," << hoytech::to_hex(id) << "\n";
            for (auto &id : need) std::cout << "xor,NEED," << hoytech::to_hex(id) << "\n";
        }

        if (q.size() == 0) break;

        std::cerr << "[" << round << "] CLIENT -> SERVER: " << q.size() << " bytes" << std::endl;

        // SERVER -> CLIENT

        q = x2.reconcile(q);

        std::cerr << "[" << round << "] SERVER -> CLIENT: " << q.size() << " bytes" << std::endl;


        round++;
    }

    return 0;
}
