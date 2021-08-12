#pragma once

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include <boost/signals2/signal.hpp>

#include "instance.h"

class InterpreterContext {
    friend class Interpreter;
public:
    virtual ~InterpreterContext();

    static std::shared_ptr<InterpreterContext> create();

};

class Interpreter
{
public:
    Interpreter(const Pybind11ModuleDefinitions &definitions={});
    ~Interpreter();

    void evaluate(const std::string &statement);
    bool empty();

    static std::unique_ptr<Interpreter> create(const Pybind11ModuleDefinitions &definitions);


public:
    // slot
    void start();
    void stop();

    // signals
    boost::signals2::signal<void(bool)> stateChanged;
    boost::signals2::signal<void(const std::string &, const std::string &)> evaluated;
    boost::signals2::signal<void(const std::string &, const std::string &)> error;
    boost::signals2::signal<void()> terminated;

protected:
    std::string dequeue();

    Pybind11ModuleDefinitions definitions;

private:
    std::thread _thread;
    std::mutex _mutex;
    std::queue<std::string> _queue;
    std::condition_variable _condition;
    bool _running;

};
