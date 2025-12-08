#include "ProcessRunner.hpp"

#include <unistd.h> // fork, execvp
#include <signal.h> // kill
#include <cstring>
#include <cerrno>
#include <iostream>
#include <sstream>
ProcessRunner::ProcessRunner(std::vector<command> &commands)
    : commands_(commands)
{
    // nothing else
}

ProcessRunner::~ProcessRunner()
{
    // Optionally: we could attempt to kill all running processes — up to you
}
static std::vector<std::string> split_cmd(const std::string &cmdline)
{
    std::istringstream iss(cmdline);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token)
    {
        tokens.push_back(token);
    }
    return tokens;
}

pid_t ProcessRunner::start(size_t index)
{
    if (index >= commands_.size())
    {
        return -1;
    }

    const std::string &cmdline = commands_[index].Path;
    if (cmdline.empty())
    {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork failed");
        return -1;
    }

    if (pid == 0)
    {
        // CHILD

        // Optionally change working directory if Folder is set:
        if (!commands_[index].Folder.empty())
        {
            if (chdir(commands_[index].Folder.c_str()) != 0)
            {
                perror("chdir failed");
                _exit(1);
            }
        }

        if (commands_[index].Mode == 'C')
        {
            // Generic command: split by whitespace
            auto parts = split_cmd(cmdline);
            if (parts.empty())
            {
                _exit(1);
            }

            std::vector<char *> argv;
            argv.reserve(parts.size() + 1);
            for (auto &s : parts)
            {
                argv.push_back(strdup(s.c_str()));
            }
            argv.push_back(nullptr);

            execvp(argv[0], argv.data());
            perror("execvp failed (C mode)");
            _exit(1);
        }
        else if (commands_[index].Mode == 'D')
        {
            // Docker mode: run docker run <image>
            // Assuming cmdline = image name (and maybe extra args)
            std::vector<std::string> parts = split_cmd(cmdline);

            std::vector<char *> argv;
            argv.reserve(parts.size() + 2);   // docker, run, ...image + extra args
            argv.push_back(strdup("docker")); // argv[0]
            argv.push_back(strdup("start"));  // argv[1]
            for (auto &s : parts)
            {
                argv.push_back(strdup(s.c_str())); // image name, maybe extra args
            }
            argv.push_back(nullptr);

            execvp("docker", argv.data());
            perror("execvp failed (docker run)");
            _exit(1);
        }
        else
        {
            // Unknown mode
            std::cerr << "Unknown mode: " << commands_[index].Mode << "\n";
            _exit(1);
        }
    }
    else
    {
        // PARENT
        commands_[index].Pid = pid;
        commands_[index].Status = RUNNING;
        return pid;
    }
}
bool ProcessRunner::kill(size_t index, bool force) {
    if (index >= commands_.size()) {
        return false;
    }
    pid_t pid = commands_[index].Pid;
    if (pid <= 0) {
        return false;
    }

    if (commands_[index].Mode == 'C') {
        // Normal process: send signal
        int sig = force ? SIGKILL : SIGTERM;
        if (::kill(pid, sig) == 0) {
            commands_[index].Status = DEAD;
            return true;
        } else {
            perror("kill failed");
            return false;
        }
    }
    else if (commands_[index].Mode == 'D') {
        // Docker container — spawn "docker stop/kill <name_or_id>"
        const std::string &container = commands_[index].Path;
        if (container.empty()) {
            return false;
        }

        pid_t child = fork();
        if (child < 0) {
            perror("fork failed (docker kill)");
            return false;
        }
        if (child == 0) {
            // in child
            const char *cmd = "docker";
            // choose sub‑command
            const char *action = force ? "kill" : "stop";

            char *argv[] = {
                (char*)cmd,
                (char*)action,
                strdup(container.c_str()),
                nullptr
            };
            execvp(cmd, argv);
            // if execvp fails:
            perror("execvp docker stop/kill failed");
            _exit(1);
        }
        else {
            // parent — we could wait or detach; here we don't block
            commands_[index].Status = DEAD;
            return true;
        }
    }

    // unknown mode
    return false;
}

pid_t ProcessRunner::getPid(size_t index) const
{
    if (index >= commands_.size())
    {
        return -1;
    }
    return commands_[index].Pid;
}
