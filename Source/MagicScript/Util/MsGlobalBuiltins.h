#pragma once

#include "CoreMinimal.h"

class UMagicScriptInterpreterSubsystem;

namespace MagicScript
{
	struct FValue;
	class FEnvironment;
	
	MAGICSCRIPT_API bool GetObjectParamBool(const TSharedPtr<TMap<FString, FValue>>& Params, const FString& Key);
	MAGICSCRIPT_API float GetObjectParamFloat(const TSharedPtr<TMap<FString, FValue>>& Params, const FString& Key);
	MAGICSCRIPT_API FVector GetObjectParamVector(const TSharedPtr<TMap<FString, FValue>>& Params, const FString& Key);
	MAGICSCRIPT_API void SetObjectParamToVector(const TSharedPtr<TMap<FString, FValue>>& Params, const FVector& Value);
}

