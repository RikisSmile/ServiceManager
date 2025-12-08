
// client.cpp
#include "httplib.h"
#include <iostream>
#include <string>
#include <vector>

struct CmdInfo {
    int    id;
    std::string desc;
    std::string status;
};

std::vector<CmdInfo> parse_cmd_list(const std::string &body) {
    std::vector<CmdInfo> result;
    size_t pos = 0;
    while (true) {
        pos = body.find('{', pos);
        if (pos == std::string::npos) break;

        CmdInfo cmd;

        auto parse_field = [&](const std::string &field) -> std::string {
            auto p = body.find('\"' + field + "\"", pos);
            if (p == std::string::npos) return "";
            auto colon = body.find(':', p);
            if (colon == std::string::npos) return "";
            auto quote1 = body.find('\"', colon);
            if (quote1 == std::string::npos) return "";
            auto quote2 = body.find('\"', quote1 + 1);
            if (quote2 == std::string::npos) return "";
            return body.substr(quote1 + 1, quote2 - quote1 - 1);
        };

        cmd.desc   = parse_field("desc");
        cmd.status = parse_field("status");

        result.push_back(cmd);

        pos += 1;
    }
    return result;
}


void PrintList()
{
    httplib::Client cli("192.168.1.89", 6755);

    auto res = cli.Get("/process/list");
    if (!res || res->status != 200) {
        std::cerr << "Failed to fetch command list\n";
        return;
    }

    std::vector<CmdInfo> cmds = parse_cmd_list(res->body);
    std::cout << "Available commands:\n";
    int i = 0;
    for (auto &c : cmds) {
        std::cout <<"[" << i  << "]" << " :"<< " \"" << c.desc << "\" — " << c.status << "\n";
        i++;
    }
}

int main() {
    httplib::Client cli("192.168.1.89", 6755);
    // Replace with your server host and port
    // 1) Fetch command list
    PrintList();


    // 2) Ask user to start/kill a command
    std::cout << "\nEnter command: p to print,  s <id> to start, k <id> to kill, q to quit\n> ";
    char op;
    int id;
    while (std::cin >> op) {
        if (op == 'q') break;
        if (op == 'p') PrintList();
        if ((op == 's' || op == 'k') && std::cin >> id) {
            httplib::Params params;
            params.emplace("fn", (op == 's' ? "start" : "kill"));
            params.emplace("id", std::to_string(id));
            auto r2 = cli.Post("/process/control", params);
            if (r2 && r2->status == 200) {
                std::cout << "Success: " << r2->body << "\n";
            } else {
                std::cout << "Failed to send control request\n";
            }
        }
        std::cout << "> ";
    }

    return 0;
}
