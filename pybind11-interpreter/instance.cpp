#include "instance.h"


PythonInstance::PythonInstance(const Pybind11ModuleDefinitions &definitions)
{
    assert(Py_IsInitialized() == 0);
    for (auto it = definitions.begin(); it != definitions.end(); ++it) {
        PyImport_AppendInittab(it->first.c_str(), it->second);
    }
    Py_Initialize();
}
PythonInstance::~PythonInstance()
{
    Py_Finalize();
}

int PythonInstance::main(int argc, char **argv)
{
    PythonInstance instance;
    return Py_BytesMain(argc, argv);
}

int PythonInstance::main(int argc, wchar_t **argv)
{
    PythonInstance instance;
    return Py_Main(argc, argv);
}
