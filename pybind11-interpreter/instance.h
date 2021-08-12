#pragma once

#include <map>
#include <string>

#include <pybind11/pybind11.h>


typedef std::map<std::string, PyObject *(*)()> Pybind11ModuleDefinitions;

class PythonInstance {
public:
    PythonInstance(const Pybind11ModuleDefinitions &definitions={});
    ~PythonInstance();

    static int main(int argc, char **argv);
    static int main(int argc, wchar_t **argv);
};
