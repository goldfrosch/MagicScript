#pragma once

#include "MsLoggingEnum.generated.h"

UENUM(BlueprintType)
enum class EScriptLogType : uint8
{
	Default,
	Warning,
	Error,
};

USTRUCT(BlueprintType)
struct MAGICSCRIPT_API FScriptLog
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	EScriptLogType LogType;

	UPROPERTY(BlueprintReadWrite)
	FString LogMessage;

	FScriptLog()
		: LogType(EScriptLogType::Default)
	{
	}
};