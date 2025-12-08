#include <string>
#pragma once
enum STATUS
{
    DEAD = 0,
    RUNNING = 1
};

typedef struct command
{
    std::string Desc;
    std::string Path;
    char Mode;
    std::string Folder = ".";
    short       Status = DEAD; // FOR DEFAULT
    int         Pid;   
};
