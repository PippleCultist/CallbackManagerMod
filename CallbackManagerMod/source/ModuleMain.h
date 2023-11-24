#pragma once
#include <YYToolkit/shared.hpp>
#include "CallbackRoutineList.h"
using namespace Aurie;
using namespace YYTK;

extern std::unordered_map<std::string, ScriptFunctionCallbackRoutineList> scriptFunctionCallbackMap;
extern YYTKInterface* g_ModuleInterface;
extern YYRunnerInterface g_RunnerInterface;
extern AurieModule* g_AurieModule;