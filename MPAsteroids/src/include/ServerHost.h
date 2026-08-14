#pragma once

#include <string>

class ServerHost
{
public:
    ServerHost();
    ~ServerHost();

    bool Start();
    void Stop();
    bool IsRunning() const;
    std::string GetHostAddress() const;

private:
    class Impl;
    Impl* impl_;
};
