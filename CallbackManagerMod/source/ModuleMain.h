#pragma once
#define ENABLEPROFILER 0
#include <YYToolkit/shared.hpp>
#include "CallbackRoutineList.h"
using namespace Aurie;
using namespace YYTK;

extern YYTKInterface* g_ModuleInterface;
extern YYRunnerInterface g_RunnerInterface;

extern std::unordered_map<int, long long> profilerMap;
extern std::unordered_map<int, const char*> codeIndexToName;
extern long long curTime;
extern long long totTime;