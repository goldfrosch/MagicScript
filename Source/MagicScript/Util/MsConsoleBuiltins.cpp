#include "MagicScript/Util/MsConsoleBuiltins.h"
#include "MagicScript/Core/MsEnvironment.h"
#include "MagicScript/Core/MsValue.h"
#include "MagicScript/Runtime/MsInterpreter.h"
#include "MagicScript/Logging/MsLoggingEnum.h"
#include "MagicScript/Subsystem/MagicScriptInterpreterSubsystem.h"

namespace MagicScript
{
	namespace MsConsoleBuiltins
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

			// console.log(...args)
			RegisterNative(TEXT("console.log"), 0, [This](const TArray<FValue>& Args, const FScriptExecutionContext& Context) -> FValue
			{
				if (!Args.IsValidIndex(0))
				{
					return FValue::Null();
				}

				FString LogResult;
				for (int32 i = 0; i < Args.Num(); i++)
				{
					if (i > 0)
					{
						LogResult += TEXT(", ");
					}
					LogResult += Args[i].ToDebugString();
				}
				
				if (This)
				{
					This->AddScriptLog(EScriptLogType::Default, LogResult);
				}
				return FValue::Null();
			});

			// console.warn(...args)
			RegisterNative(TEXT("console.warn"), 0, [This](const TArray<FValue>& Args, const FScriptExecutionContext& Context) -> FValue
			{
				if (!Args.IsValidIndex(0))
				{
					return FValue::Null();
				}

				FString LogResult;
				for (int32 i = 0; i < Args.Num(); i++)
				{
					if (i > 0)
					{
						LogResult += TEXT(", ");
					}
					LogResult += Args[i].ToDebugString();
				}
				
				if (This)
				{
					This->AddScriptLog(EScriptLogType::Warning, LogResult);
				}
				return FValue::Null();
			});

			// console.error(...args)
			RegisterNative(TEXT("console.error"), 0, [This](const TArray<FValue>& Args, const FScriptExecutionContext& Context) -> FValue
			{
				if (!Args.IsValidIndex(0))
				{
					return FValue::Null();
				}

				FString LogResult;
				for (int32 i = 0; i < Args.Num(); i++)
				{
					if (i > 0)
					{
						LogResult += TEXT(", ");
					}
					LogResult += Args[i].ToDebugString();
				}
				
				if (This)
				{
					This->AddScriptLog(EScriptLogType::Error, LogResult);
				}
				return FValue::Null();
			});
		}
	}
}

