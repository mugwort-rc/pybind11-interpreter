#include "interpreter.h"

#include <chrono>

#include <boost/algorithm/string.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/eval.h>


class InterpreterContextImpl : public InterpreterContext {
    friend class Interpreter;
public:
    InterpreterContextImpl()
        : global(pybind11::dict(pybind11::module::import("__main__").attr("__dict__")))
        , local()
    {}
    ~InterpreterContextImpl()
    {}

protected:
    pybind11::dict global;
    pybind11::dict local;
};


InterpreterContext::~InterpreterContext()
{}

std::shared_ptr<InterpreterContext> InterpreterContext::create() {
    return std::make_shared<InterpreterContextImpl>();
}


Interpreter::Interpreter(const Pybind11ModuleDefinitions &definitions)
    : stateChanged()
    , evaluated()
    , error()
    , terminated()
    , definitions(definitions)
    , _thread()
    , _mutex()
    , _queue()
    , _condition()
    , _running(false)
{
}

Interpreter::~Interpreter()
{
    stop();
}

void Interpreter::evaluate(const std::string &statement)
{
    auto trimmed = boost::algorithm::trim_copy(statement);
    if ( trimmed.empty() ) {
        return;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    _queue.push(statement);
    _condition.notify_all();
}

bool Interpreter::empty()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _queue.empty();
}

std::unique_ptr<Interpreter> Interpreter::create(const Pybind11ModuleDefinitions &definitions)
{
    return std::make_unique<Interpreter>(definitions);
}

std::string Interpreter::dequeue()
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto value = _queue.front();
    _queue.pop();
    return value;
}

// see: https://stackoverflow.com/a/18063284

void Interpreter::start()
{
    _running = true;
    _thread = std::thread([&]() {
        auto instance = std::make_unique<PythonInstance>(definitions);
        std::shared_ptr<InterpreterContext> context;
        while ( _running ) {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _condition.wait(lock);
            }
            while ( _running && ! empty() ) {
                auto statement = dequeue();
                if ( ! context ) {
                    context = InterpreterContext::create();
                }
                auto _rawContext = reinterpret_cast<InterpreterContextImpl *>(context.get());
                stateChanged(true);
                try {
                    PyObject *compiled = nullptr;
                    // compile as expr
                    compiled = Py_CompileString(statement.c_str(), "<stdin>", Py_eval_input);
                    if ( ! compiled ) {
                        // reset error
                        PyErr_Clear();
                        // compile as stmt
                        compiled = Py_CompileString(statement.c_str(), "<stdin>", Py_file_input);
                    }
                    if ( compiled == nullptr ) {
                        // compile error
                        throw pybind11::error_already_set();
                        continue;
                    }
                    assert(compiled != nullptr);
                    auto ptr = PyEval_EvalCode(compiled, _rawContext->global.ptr(), _rawContext->local.ptr());
                    auto result = pybind11::handle(ptr);

                    if ( result.is_none() ) {
                        evaluated(statement, std::string());
                    } else {
                        evaluated(statement, pybind11::repr(result).cast<std::string>());
                    }
                } catch (pybind11::error_already_set &err) {
                    // serialize Python error
                    std::string error_message;

                    auto trace_ = err.trace();
                    if ( trace_ ) {
                        PyTracebackObject *trace = reinterpret_cast<PyTracebackObject *>(trace_.ptr());

                        /* Get the deepest trace possible */
                        while (trace->tb_next)
                            trace = trace->tb_next;

                        PyFrameObject *frame = trace->tb_frame;
                        error_message+= "Traceback (most recent call last):\n";
                        while (frame) {
                            int lineno = PyFrame_GetLineNumber(frame);
                            error_message +=
                                "  File \"" + pybind11::handle(frame->f_code->co_filename).cast<std::string>() +
                                "\", line " + std::to_string(lineno) + ", in " +
                                pybind11::handle(frame->f_code->co_name).cast<std::string>() + "\n";
                            frame = frame->f_back;
                        }
                    }
                    auto type = err.type();
                    if ( type ) {
                        error_message += type.attr("__name__").cast<std::string>();
                        error_message += ": ";
                    }
                    auto value = err.value();
                    if ( value ) {
                        error_message += pybind11::str(value).cast<std::string>() + "\n";
                    }
                    error(statement, error_message.c_str());
                    err.restore();
                    PyErr_Clear();
                }

                stateChanged(false);
            }
        }
    });

    terminated();
}

void Interpreter::stop() {
    if ( _running ) {
        std::lock_guard<std::mutex> lock(_mutex);
        _running = false;
        _condition.notify_all();
    }
    if ( _thread.joinable() ) {
        _thread.join();
    }
}
