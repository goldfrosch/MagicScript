#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MagicScript/Runtime/MsInterpreter.h"
#include "MagicScriptInterpreterSubsystem.generated.h"

struct FScriptLog;
enum class EScriptLogType : uint8;

namespace MagicScript
{
	class FLexer;
	class FParser;
	class FEnvironment;
	struct FProgram;
	struct FTimeComplexityResult;
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScriptLogAdded, const FScriptLog&, ScriptLog);

UCLASS()
class MAGICSCRIPT_API UMagicScriptInterpreterSubsystem
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	static UMagicScriptInterpreterSubsystem* Get(const UObject* WorldContextObject);

	FOnScriptLogAdded OnScriptLogAdded;

	const TArray<FScriptLog>& GetScriptLogs() const { return ScriptLogs; }

	void AddScriptLog(const EScriptLogType ScriptLogType, const FString& ScriptLogMessage);

	void ClearScriptLogs() { ScriptLogs.Empty(); }

	bool RunScriptFile(const FString& RelativePath,
		const FString& FuncName = TEXT("main"),
		const MagicScript::FScriptExecutionContext& ExecutionContext = MagicScript::FScriptExecutionContext());

	void ClearScriptCache(const FString& RelativePath);

	void TickEventLoops();

	bool CheckScriptByPath(const FString& ScriptPath, FString& Source);
	bool SaveScriptCache(const FString& ScriptPath, const FString& Source);
	
	double GetTimeComplexityCache(const FString& RelativePath) const;
	int64 GetSpaceComplexityCache(const FString& RelativePath) const;

protected:
	virtual void OnRegisterBuiltins(const TSharedPtr<MagicScript::FEnvironment> Env);
	
private:
	// 스크립트 실행 중 발생하는 모든 로그를 저장하는 배열
	UPROPERTY()
	TArray<FScriptLog> ScriptLogs;

	TMap<FString, FString> ScriptCache;
	
	// 인터프리터 캐시 (파일 경로별)
	TMap<FString, TSharedPtr<MagicScript::FInterpreter>> InterpreterCache;

	// 파일 경로 기반 프로그램 정보 캐싱
	TMap<FString, TSharedPtr<MagicScript::FProgram>> ProgramCache;

	// 시간 복잡도 캐시
	TMap<FString, TSharedPtr<MagicScript::FTimeComplexityResult>> PrevTimeComplexityCache;

	// 공간 복잡도 캐시
	TMap<FString, int64> PrevSpaceComplexityCache;

	bool CheckCache_Internal(const FString& RelativePath, const FString& FuncName, MagicScript::FScriptExecutionContext& ExecutionContext);
	bool Lexer_Internal(MagicScript::FLexer& Lexer, TArray<MagicScript::FToken>& Tokens, const FString& ScriptPath);
	bool Parsing_Internal(MagicScript::FParser& Parser, const FString& RelativePath);
	bool Import_Internal(const TSharedPtr<MagicScript::FProgram>& Program, const MagicScript::FScriptExecutionContext& ExecutionContext);
	void RunScript_Internal(MagicScript::FTimeComplexityResult& TimeComplexityResult,
		const FString& RelativePath, const FString& FuncName,
		MagicScript::FScriptExecutionContext& ExecutionContext);
	
	void RegisterBuiltins_Internal(const FString& RelativePath);
};
