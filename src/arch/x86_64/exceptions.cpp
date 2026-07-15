#include "LTOS/panic.hpp"

extern "C" void divide_error() { panic::halt("Divide by zero"); }
