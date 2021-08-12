# pybind11-interpreter

## Requirements

* C++17 compiler
* Boost.Signals2
* python-dev
* pybind11

## Usage

```cpp
#include <iostream>

#include "pybind11-interpreter/interpreter.h"


int main(int argc, const char *argv[])
{
    Interpreter interpreter;

    bool running = false;
    std::condition_variable cond_busy;
    std::mutex busy_mutex;

    interpreter.stateChanged.connect([&](bool state){
        if ( state ) {
            running = state;
        }
    });

    interpreter.evaluated.connect([&](const std::string &stmt, const std::string &result){
        if ( result.empty() ) {
            return;
        }
        std::cout << result << std::endl;
        std::lock_guard<std::mutex> lock(busy_mutex);
        running = false;
        cond_busy.notify_all();
    });
    interpreter.error.connect([&](const std::string &stmt, const std::string &err){
        std::cout << err << std::endl;
        std::lock_guard<std::mutex> lock(busy_mutex);
        running = false;
        cond_busy.notify_all();
    });

    interpreter.start();

    std::string temp;
    while ( true ) {
        {
            std::unique_lock<std::mutex> lock(busy_mutex);
            cond_busy.wait(lock, [&]() {
                return ! running;
            });
        }
        // for timing issue
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::cout << ">>> " << std::flush;
        if ( std::getline(std::cin, temp) ) {
            if ( temp == "exit()" ) {
                interpreter.stop();
                break;
            }
            interpreter.evaluate(temp);
        }
        if ( ! std::cin ) {
            interpreter.stop();
            break;
        }
    }

    return 0;
}
```

```
$ g++ -std=c++17 interpreter_cui.cpp -I../ -I../.venv/lib/python3.8/site-packages/pybind11/include/ -I/usr/include/python3.8/ -lpybind11_interpreter -lpython3.8 -pthread
$ ./a.out
>>> a = 1
>>> a
1
>>>   a * 2
IndentationError: unexpected indent (<stdin>, line 1)

>>> a * 2
2
>>> exit()
```
