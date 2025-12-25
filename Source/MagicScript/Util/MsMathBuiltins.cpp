#include "MagicScript/Util/MsMathBuiltins.h"
#include "MagicScript/Core/MsEnvironment.h"
#include "MagicScript/Core/MsValue.h"
#include "MagicScript/Runtime/MsInterpreter.h"
#include "MagicScript/Subsystem/MagicScriptInterpreterSubsystem.h"
#include "Math/UnrealMathUtility.h"

namespace MagicScript
{
	namespace MsMathBuiltins
	{
		void Register(TSharedPtr<FEnvironment> Env, UMagicScriptInterpreterSubsystem* Subsystem)
		{
			if (!Env.IsValid())
			{
				return;
			}

			auto RegisterNative = [&Env](const FString& Name, int32 SpaceBytes, TFunction<FValue(const TArray<FValue>&, const FScriptExecutionContext&)> Impl)
			{
				TSharedPtr<FFunctionValue> Func = MakeShared<FFunctionValue>();
				Func->Name = Name;
				Func->bIsNative = true;
				Func->NativeImpl = Impl;
				Func->SpaceCostBytes = SpaceBytes;
				Env->Define(Name, FValue::FromFunction(Func), true);
			};

			// math.pow(base, exp)
			RegisterNative(TEXT("math.pow"), 0, [](const TArray<FValue>& Args, const FScriptExecutionContext& Context) -> FValue
			{
				double Base = (Args.IsValidIndex(0) && Args[0].Type == EValueType::Number) ? Args[0].Number : 0.0;
				double Exp  = (Args.IsValidIndex(1) && Args[1].Type == EValueType::Number) ? Args[1].Number : 0.0;
				return FValue::FromNumber(FMath::Pow(Base, Exp));
			});
		}
	}
}

