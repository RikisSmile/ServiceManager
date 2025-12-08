#include <iostream>
#include "command.hpp"
#include "httplib.h"
#include "ProcessRunner.hpp"
#include <vector>
#include <fstream>
#include <thread>

#define PORT 6755

std::string Conf = "/home/raima/.sermn/cmds.conf";


// Command vector
std::vector<command> Commnads;
int N;

ProcessRunner procRunner(Commnads);

int  Init();
void ReadCmds();
void ServerSide();
void ProcessSide();

int main ( void )
{
    if (Init() != 0)
    {
        return -1;
    }

    ReadCmds();

    // Start Threads
    std::thread Server_Thread (ServerSide);
    // std::thread Process_Thread(ProcessSide);

    Server_Thread.detach();
    // Process_Thread.detach();

    // keep alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    
    return 0;
}

int Init()
{
    // Let Check if conf exist 
    if (FILE *file = fopen(Conf.c_str(), "r")) {
        fclose(file);
        std::cout << "CONF file Found !! \n";
        return 0;
    } else {
        std::cout << "CONF file WAS NOT Found !! Aborting! \n";
        return -1;
    }  


}

void ReadCmds()
{
    std::ifstream file(Conf);
    file >> N;
    file.ignore();

    for (int i = 0 ; i < N; i++)
    {
        command cmd;
        getline(file, cmd.Desc);
        file >> cmd.Mode;
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        getline(file, cmd.Path);
        getline(file, cmd.Folder);
        Commnads.push_back(cmd);
    }

    file.close();
}


void ServerSide()
{
    httplib::Server svr;

    svr.Get("/process/list", [&](const httplib::Request& /*req*/, httplib::Response& res) {
        std::string body = "[\n";

        bool first = true;
        for (auto &cmd : Commnads) {
            if (!first) {
                body += ",\n";
            }
            first = false;

            body += "  {\"desc\": \"";
            // naive escaping: replace quote characters if needed
            for (char c : cmd.Desc) {
                if (c == '\"' || c == '\\') {
                    body += '\\';
                }
                body += c;
            }
            body += "\", \"status\": \"";
            body += (cmd.Status == RUNNING ? "RUNNING" : "DEAD");
            body += "\"}";
        }
        body += "\n]\n";

        res.set_content(body, "application/json");
        res.status = 200;
    });

        svr.Post("/process/control", [&](const httplib::Request &req,
                                     httplib::Response &res) {
        // Check parameters
        if (!req.has_param("fn") || !req.has_param("id")) {
            res.status = 400;
            res.set_content("Missing fn or id parameter", "text/plain");
            return;
        }

        std::string fn = req.get_param_value("fn");
        std::string id_str = req.get_param_value("id");
        int id = -1;
        try {
            id = std::stoi(id_str);
        } catch (...) {
            res.status = 400;
            res.set_content("Bad id parameter", "text/plain");
            return;
        }

        if (id < 0 || id >= (int)Commnads.size()) {
            res.status = 404;
            res.set_content("Invalid id", "text/plain");
            return;
        }

        if (fn == "start" && Commnads[id].Status != RUNNING) {
            pid_t pid = procRunner.start(id);
            if (pid < 0) {
                res.status = 500;
                res.set_content("Failed to start process", "text/plain");
            } else {
                res.set_content("Started, pid=" + std::to_string(pid), "text/plain");
            }
        }
        else if (fn == "kill" || fn == "end" || fn == "stop") {
            bool ok = procRunner.kill(id, /*force=*/true);
            if (!ok) {
                res.status = 500;
                res.set_content("Failed to kill process", "text/plain");
            } else {
                res.set_content("Killed", "text/plain");
            }
        }
        else if (fn == "status") {
            short s = Commnads[id].Status;
            res.set_content(std::to_string(s), "text/plain");  // or human‑readable
        }
        else {
            res.status = 400;
            res.set_content("Unknown fn value", "text/plain");
        }
    });
    // start server
    svr.listen("192.168.1.89", PORT);
}