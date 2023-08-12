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

    uint64_t frameSizeLimit1 = 0, frameSizeLimit2 = 0;
    if (::getenv("FRAMESIZELIMIT1")) frameSizeLimit1 = std::stoull(::getenv("FRAMESIZELIMIT1"));
    if (::getenv("FRAMESIZELIMIT2")) frameSizeLimit2 = std::stoull(::getenv("FRAMESIZELIMIT2"));

    // x1 is client, x2 is server
    Negentropy x1(idSize, frameSizeLimit1);
    Negentropy x2(idSize, frameSizeLimit2);

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
    uint64_t totalUp = 0;
    uint64_t totalDown = 0;

    while (1) {
        // CLIENT -> SERVER

        if (round == 0) {
            q = x1.initiate();
        } else {
            std::vector<std::string> have, need;
            q = x1.reconcile(q, have, need);

            for (auto &id : have) std::cout << "xor,HAVE," << hoytech::to_hex(id) << "\n";
            for (auto &id : need) std::cout << "xor,NEED," << hoytech::to_hex(id) << "\n";
        }

        if (q.size() == 0) break;

        std::cerr << "[" << round << "] CLIENT -> SERVER: " << q.size() << " bytes" << std::endl;
        totalUp += q.size();
        if (frameSizeLimit1 && q.size() > frameSizeLimit1) throw hoytech::error("frameSizeLimit1 exceeded");

        // SERVER -> CLIENT

        q = x2.reconcile(q);

        std::cerr << "[" << round << "] SERVER -> CLIENT: " << q.size() << " bytes" << std::endl;
        totalDown += q.size();
        if (frameSizeLimit2 && q.size() > frameSizeLimit2) throw hoytech::error("frameSizeLimit2 exceeded");


        round++;
    }

    std::cerr << "UP: " << totalUp << " bytes, DOWN: " << totalDown << " bytes" << std::endl;

    return 0;
}
