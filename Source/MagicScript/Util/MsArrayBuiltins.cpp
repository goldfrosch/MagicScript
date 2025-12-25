#include "MagicScript/Util/MsArrayBuiltins.h"
#include "MagicScript/Core/MsEnvironment.h"
#include "MagicScript/Core/MsValue.h"
#include "MagicScript/Runtime/MsInterpreter.h"
#include "MagicScript/Logging/MsLoggingEnum.h"
#include "MagicScript/Subsystem/MagicScriptInterpreterSubsystem.h"

namespace MagicScript
{
	namespace MsArrayBuiltins
	{
		void Register(TSharedPtr<FEnvironment> Env, UMagicScriptInterpreterSubsystem* Subsystem)
		{
			if (!Env.IsValid())
			{
				return;
			}

			UMagicScriptInterpreterSubsystem* This = Subsystem;
			auto RegisterNative = [&Env, This](const FString& Name, int32 SpaceBytes, TFunction<FValue(const TArray<FValue>&, const FScriptExecutionContext&)> Impl)
			{
				TSharedPtr<FFunctionValue> Func = MakeShared<FFunctionValue>();
				Func->Name = Name;
				Func->bIsNative = true;
				Func->NativeImpl = Impl;
				Func->SpaceCostBytes = SpaceBytes;
				Env->Define(Name, FValue::FromFunction(Func), true);
			};

			// Array.push_back(array, value)
			RegisterNative(TEXT("Array.push_back"), 0, [This](const TArray<FValue>& Args, const FScriptExecutionContext& Context) -> FValue
			{
				if (!Args.IsValidIndex(0) || Args[0].Type != EValueType::Array || !Args[0].Array.IsValid())
				{
					if (This) This->AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array.push_back requires array as first argument"));
					return FValue::Null();
				}
				
				if (!Args.IsValidIndex(1))
				{
					if (This) This->AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array.push_back requires value argument"));
					return FValue::Null();
				}
				
				Args[0].Array->Add(Args[1]);
				return FValue::Null();
			});

			// Array.push_front(array, value)
			RegisterNative(TEXT("Array.push_front"), 0, [This](const TArray<FValue>& Args, const FScriptExecutionContext& Context) -> FValue
			{
				if (!Args.IsValidIndex(0) || Args[0].Type != EValueType::Array || !Args[0].Array.IsValid())
				{
					if (This) This->AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array.push_front requires array as first argument"));
					return FValue::Null();
				}
				
				if (!Args.IsValidIndex(1))
				{
					if (This) This->AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array.push_front requires value argument"));
					return FValue::Null();
				}
				
				Args[0].Array->Insert(Args[1], 0);
				return FValue::Null();
			});

			// Array.pop_back(array)
			RegisterNative(TEXT("Array.pop_back"), 0, [This](const TArray<FValue>& Args, const FScriptExecutionContext& Context) -> FValue
			{
				if (!Args.IsValidIndex(0) || Args[0].Type != EValueType::Array || !Args[0].Array.IsValid())
				{
					if (This) This->AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array.pop_back requires array as first argument"));
					return FValue::Null();
				}
				
				if (Args[0].Array->Num() == 0)
				{
					if (This) This->AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array.pop_back called on empty array"));
					return FValue::Null();
				}
				
				FValue Result = Args[0].Array->Last();
				Args[0].Array->RemoveAt(Args[0].Array->Num() - 1);
				return Result;
			});

			// Array.pop_front(array)
			RegisterNative(TEXT("Array.pop_front"), 0, [This](const TArray<FValue>& Args, const FScriptExecutionContext& Context) -> FValue
			{
				if (!Args.IsValidIndex(0) || Args[0].Type != EValueType::Array || !Args[0].Array.IsValid())
				{
					if (This) This->AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array.pop_front requires array as first argument"));
					return FValue::Null();
				}
				
				if (Args[0].Array->Num() == 0)
				{
					if (This) This->AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array.pop_front called on empty array"));
					return FValue::Null();
				}
				
				FValue Result = Args[0].Array->operator[](0);
				Args[0].Array->RemoveAt(0);
				return Result;
			});

			// Array.length(array)
			RegisterNative(TEXT("Array.length"), 0, [This](const TArray<FValue>& Args, const FScriptExecutionContext& Context) -> FValue
			{
				if (!Args.IsValidIndex(0) || Args[0].Type != EValueType::Array || !Args[0].Array.IsValid())
				{
					if (This) This->AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array.length requires array as first argument"));
					return FValue::Null();
				}
				
				if (Args[0].Array->Num() == 0)
				{
					if (This) This->AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array.length called on empty array"));
					return FValue::Null();
				}
				
				return FValue::FromNumber(Args[0].Array->Num());
			});
		}
	}
}

