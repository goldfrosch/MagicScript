#include "MagicScript/Util/MsGlobalBuiltins.h"
#include "MagicScript/Core/MsEnvironment.h"
#include "MagicScript/Core/MsValue.h"
#include "MagicScript/Runtime/MsInterpreter.h"
#include "MagicScript/Logging/MsLoggingEnum.h"
#include "MagicScript/Subsystem/MagicScriptInterpreterSubsystem.h"

namespace MagicScript
{
	namespace MsGlobalBuiltins
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

			// SetGlobalFloat(name, value)
			RegisterNative(TEXT("SetGlobalFloat"), 64, [This](const TArray<FValue>& Args, const FScriptExecutionContext& Context) -> FValue
			{
				if (!Args.IsValidIndex(0) || Args[0].Type != EValueType::String)
				{
					return FValue::Null();
				}
				const FString VarName = Args[0].String;
				double Value = (Args.IsValidIndex(1) && Args[1].Type == EValueType::Number) ? Args[1].Number : 0.0;

				if (This)
				{
					This->AddScriptLog(EScriptLogType::Default, FString::Printf(TEXT("SetGlobalFloat %s = %f"), *VarName, Value));
				}
				return FValue::Null();
			});
		}
	}
}

