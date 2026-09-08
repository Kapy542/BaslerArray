#pragma once

#include <sys/types.h>

#include <string>

class OusterNode
{
public:
    OusterNode(
        const std::string& sensor);

    ~OusterNode();

    // Start ouster-cli recording.
    // Returns true if the process was successfully started.
    bool Start(const std::string& outputDirectory);

    // Stop recording and wait for ouster-cli to exit.
    void Stop();

    // Returns true if the ouster-cli process is currently running.
    bool IsRunning() const;

private:
    std::string sensor;

    pid_t processId = -1;
};
