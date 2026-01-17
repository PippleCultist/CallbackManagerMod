#pragma once
#include <YYToolkit/YYTK_Shared.hpp>
#include <vector>

template <typename RoutineType>
struct CallbackRoutine
{
	RoutineType beforeRoutine;
	RoutineType afterRoutine;
	std::string modName;
	bool callOriginalFunctionFlag;
	bool cancelOriginalFunctionFlag;
	CallbackRoutine(RoutineType beforeRoutine, RoutineType afterRoutine, std::string modName) : beforeRoutine(beforeRoutine), afterRoutine(afterRoutine), modName(modName),
		callOriginalFunctionFlag(false), cancelOriginalFunctionFlag(false)
	{
	}
};

template <typename RoutineType>
struct CallbackRoutineList
{
	std::vector<CallbackRoutine<RoutineType>> routineList;
	RoutineType callbackRoutine;
	RoutineType originalFunction;
	std::string name;
	PFUNC_YYGMLScript funcPointer = nullptr;
	int index;

	CallbackRoutineList() : callbackRoutine(nullptr), originalFunction(nullptr), index(0)
	{
	}

	CallbackRoutineList(std::string name, int index) : callbackRoutine(nullptr), originalFunction(nullptr), name(name), index(index)
	{
	}

	CallbackRoutineList(PFUNC_YYGMLScript funcPointer, int index) : callbackRoutine(nullptr), originalFunction(nullptr), funcPointer(funcPointer), index(index)
	{
	}
};