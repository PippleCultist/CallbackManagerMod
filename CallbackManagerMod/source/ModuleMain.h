#pragma once
#define ENABLEPROFILER 0
#include <YYToolkit/YYTK_Shared.hpp>
#include "CallbackRoutineList.h"
using namespace Aurie;
using namespace YYTK;

#define VERSION_NUM "v1.1.0"
#define MODNAME "Callback Manager Mod " VERSION_NUM

extern YYTKInterface* g_ModuleInterface;
extern YYRunnerInterface g_RunnerInterface;

extern std::unordered_map<int, long long> profilerMap;
extern std::unordered_map<int, const char*> codeIndexToName;
extern long long curTime;
extern long long totTime;