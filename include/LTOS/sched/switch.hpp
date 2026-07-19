#pragma once

#include "LTOS/sched/task.hpp"

namespace sched {

extern "C" void switch_context(CPUContext **old);

}
