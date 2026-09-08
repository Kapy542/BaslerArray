#include "OusterNode.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;


OusterNode::OusterNode(
    const string& sensor)
    : sensor(sensor)
{
}


OusterNode::~OusterNode()
{
    if (IsRunning())
    {
        Stop();
    }
}


bool OusterNode::Start(const std::string& outputPath)
{
    if (IsRunning())
    {
        cerr << "[Ouster] Recorder is already running." << endl;
        return false;
    }

    /*
     * ------------------------------------------------------------
     * Output file
     * ------------------------------------------------------------
     */

    const string outputFile = outputPath + ".pcap";
    std::cout << "Starting Ouster Rec in: " << outputFile << std::endl;

    /*
     * ------------------------------------------------------------
     * Create child process
     * ------------------------------------------------------------
     */

    processId = fork();

    if (processId < 0)
    {
        cerr << "[Ouster] Failed to fork: "
            << strerror(errno)
            << endl;

        processId = -1;
        return false;
    }


    /*
     * ------------------------------------------------------------
     * CHILD PROCESS
     * ------------------------------------------------------------
     */

    if (processId == 0)
    {
        std::string ousterCliPath = "../../../.venv/bin/ouster-cli";
        //std::string ousterCliPath = "ouster-cli";
        execl(
            ousterCliPath.c_str(),
            "ouster-cli",

            "source",
            sensor.c_str(),

            "save_raw",
            outputFile.c_str(),

            nullptr
        );


        /*
         * If execlp() returns, execution failed.
         */

        cerr << "[Ouster] Failed to start ouster-cli: "
            << strerror(errno)
            << endl;

        _exit(127);
    }


    /*
     * ------------------------------------------------------------
     * PARENT PROCESS
     * ------------------------------------------------------------
     */

    cout << "[Ouster] Started ouster-cli"
        << " (PID " << processId << ")"
        << endl;

    return true;
}


void OusterNode::Stop()
{
    if (!IsRunning())
    {
        processId = -1;
        return;
    }


    cout << "[Ouster] Stopping recording..." << endl;


    /*
     * SIGINT allows ouster-cli to shut down normally and
     * finish writing the PCAP/metadata.
     */

    if (kill(processId, SIGINT) != 0)
    {
        if (errno == ESRCH)
        {
            // Process already exited.
            processId = -1;
            return;
        }

        cerr << "[Ouster] Failed to send SIGINT: "
            << strerror(errno)
            << endl;
    }


    /*
     * Wait until ouster-cli has actually terminated.
     *
     * This is important because otherwise the recorder could
     * continue while the PCAP file is still being finalized.
     */

    int status = 0;

    pid_t result;

    do
    {
        result = waitpid(processId, &status, 0);
    } while (result == -1 && errno == EINTR);


    if (result == -1)
    {
        cerr << "[Ouster] waitpid() failed: "
            << strerror(errno)
            << endl;

        processId = -1;
        return;
    }


    /*
     * ------------------------------------------------------------
     * Check exit status
     * ------------------------------------------------------------
     */

    if (WIFEXITED(status))
    {
        const int exitCode = WEXITSTATUS(status);

        if (exitCode == 0)
        {
            cout << "[Ouster] Recording stopped successfully."
                << endl;
        }
        else
        {
            cerr << "[Ouster] ouster-cli exited with code "
                << exitCode
                << endl;
        }
    }
    else if (WIFSIGNALED(status))
    {
        cerr << "[Ouster] ouster-cli terminated by signal "
            << WTERMSIG(status)
            << endl;
    }


    processId = -1;
}


bool OusterNode::IsRunning() const
{
    if (processId <= 0)
        return false;

    /*
     * kill(pid, 0) doesn't send a signal. It only checks
     * whether the process exists and whether we have permission
     * to signal it.
     */

    if (kill(processId, 0) == 0)
        return true;

    if (errno == EPERM)
        return true;

    return false;
}
