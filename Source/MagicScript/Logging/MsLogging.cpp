#include "MagicScript/Logging/MsLogging.h"
#include "MagicScript/Logging/MsLoggingEnum.h"
#include "MagicScript/Subsystem/MagicScriptInterpreterSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

namespace MagicScript
{
	TArray<FScriptLog>& GetScriptLogs()
	{
		// GEngine을 통해 현재 World를 찾고 서브시스템에서 로그 배열 가져오기
		if (GEngine)
		{
			// GEngine의 모든 WorldContext를 확인
			for (int32 i = 0; i < GEngine->GetWorldContexts().Num(); ++i)
			{
				const FWorldContext& Context = GEngine->GetWorldContexts()[i];
				if (UWorld* World = Context.World())
				{
					if (UGameInstance* GameInstance = World->GetGameInstance())
					{
						if (UMagicScriptInterpreterSubsystem* Subsystem = GameInstance->GetSubsystem<UMagicScriptInterpreterSubsystem>())
						{
							// const 메서드가 아니므로 const_cast 필요
							return const_cast<TArray<FScriptLog>&>(Subsystem->GetScriptLogs());
						}
					}
				}
			}
		}
		
		// World를 찾을 수 없는 경우 정적 배열 반환 (임시)
		static TArray<FScriptLog> EmptyLogs;
		return EmptyLogs;
	}

	void AddScriptLog(const EScriptLogType ScriptLogType, const FString& ScriptLogMessage)
	{
		// GEngine을 통해 현재 World를 찾고 서브시스템의 AddScriptLog 호출
		if (GEngine)
		{
			// GEngine의 모든 WorldContext를 확인
			for (int32 i = 0; i < GEngine->GetWorldContexts().Num(); ++i)
			{
				const FWorldContext& Context = GEngine->GetWorldContexts()[i];
				if (UWorld* World = Context.World())
				{
					if (UGameInstance* GameInstance = World->GetGameInstance())
					{
						if (UMagicScriptInterpreterSubsystem* Subsystem = GameInstance->GetSubsystem<UMagicScriptInterpreterSubsystem>())
						{
							Subsystem->AddScriptLog(ScriptLogType, ScriptLogMessage);
							return;
						}
					}
				}
			}
		}
		
		// World를 찾을 수 없는 경우 UE_LOG로 대체
		UE_LOG(LogTemp, Warning, TEXT("MagicScript: Failed to add script log (subsystem not available): %s"), *ScriptLogMessage);
	}

	void ClearScriptLogs()
	{
		// GEngine을 통해 현재 World를 찾고 서브시스템의 ClearScriptLogs 호출
		if (GEngine)
		{
			// GEngine의 모든 WorldContext를 확인
			for (int32 i = 0; i < GEngine->GetWorldContexts().Num(); ++i)
			{
				const FWorldContext& Context = GEngine->GetWorldContexts()[i];
				if (UWorld* World = Context.World())
				{
					if (UGameInstance* GameInstance = World->GetGameInstance())
					{
						if (UMagicScriptInterpreterSubsystem* Subsystem = GameInstance->GetSubsystem<UMagicScriptInterpreterSubsystem>())
						{
							Subsystem->ClearScriptLogs();
							return;
						}
					}
				}
			}
		}
	}
}
