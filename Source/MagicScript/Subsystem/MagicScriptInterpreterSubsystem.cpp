#include "MagicScriptInterpreterSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformTime.h"

#include "MagicScript/Analysis/MsTimeComplexity.h"
#include "MagicScript/Core/MsLexer.h"
#include "MagicScript/Core/MsParser.h"
#include "MagicScript/Core/MsEnvironment.h"
#include "MagicScript/Logging/MsLoggingEnum.h"
#include "MagicScript/Runtime/MsInterpreter.h"
#include "MagicScript/Util/MsMathBuiltins.h"
#include "MagicScript/Util/MsConsoleBuiltins.h"
#include "MagicScript/Util/MsArrayBuiltins.h"

using namespace MagicScript;

void UMagicScriptInterpreterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UMagicScriptInterpreterSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

UMagicScriptInterpreterSubsystem* UMagicScriptInterpreterSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UMagicScriptInterpreterSubsystem>();
		}
	}
	return nullptr;
}

void UMagicScriptInterpreterSubsystem::AddScriptLog(const EScriptLogType ScriptLogType, const FString& ScriptLogMessage)
{
	FScriptLog Log;
	Log.LogType = ScriptLogType;
	Log.LogMessage = ScriptLogMessage;
	ScriptLogs.Add(Log);
	
	// Delegate를 통해 바인딩된 모든 함수 호출
	OnScriptLogAdded.Broadcast(Log);
}

bool UMagicScriptInterpreterSubsystem::RunScriptFile(const FString& RelativePath, const FString& FuncName, const FScriptExecutionContext& ExecutionContext)
{
	const double StartTime = FPlatformTime::Seconds();
	FString Source;
	
	if (!CheckScriptByPath(RelativePath, Source))
	{
		return false;
	}
	
	// 0) 캐시 확인 (상대 경로를 키로 사용)
	if (CheckCache_Internal(RelativePath, FuncName,
		const_cast<FScriptExecutionContext&>(ExecutionContext)))
	{
		return true;  // 캐시에서 실행 성공
	}

	// 1) 렉싱
	FLexer Lexer(Source);
	TArray<FToken> Tokens;
	if (!Lexer_Internal(Lexer, Tokens, RelativePath))
	{
		return false;
	}

	// 2) 파싱 (상대 경로를 키로 사용)
	FParser Parser(Tokens);
	if (!Parsing_Internal(Parser, RelativePath))
	{
		return false;
	}

	// 3) 정적 분석 (AST 기반 시간 복잡도 계산)
	TSharedPtr<FProgram> Program = ProgramCache[RelativePath];
	FTimeComplexityResult TimeComplexity = FTimeComplexityAnalyzer::AnalyzeProgram(Program);

	// 4) 인터프리터 생성 + 네이티브 함수 등록 (상대 경로를 키로 사용)
	RegisterBuiltins_Internal(RelativePath);

	// --- import 처리 ---
	if (!Import_Internal(Program, ExecutionContext))
	{
		return false;
	}

	// 전역 코드 실행
	InterpreterCache[RelativePath]->ExecuteProgram(Program, ExecutionContext);
	
	// 5) 함수 실행 및 동적 분석 (상대 경로를 키로 사용)
	RunScript_Internal(TimeComplexity, RelativePath, FuncName,
		const_cast<FScriptExecutionContext&>(ExecutionContext));

	const double EndTime = FPlatformTime::Seconds();

	UE_LOG(LogTemp, Display, TEXT("%s Script %s Function GeneratedTime : %f"), *RelativePath, *FuncName, EndTime - StartTime);
	
	return true;
}

void UMagicScriptInterpreterSubsystem::ClearScriptCache(const FString& RelativePath)
{
	ProgramCache.Remove(RelativePath);
	InterpreterCache.Remove(RelativePath);
	PrevTimeComplexityCache.Remove(RelativePath);
	PrevSpaceComplexityCache.Remove(RelativePath);
}

double UMagicScriptInterpreterSubsystem::GetTimeComplexityCache(
	const FString& RelativePath) const
{
	if (!PrevTimeComplexityCache.FindRef(RelativePath))
	{
		return 0;
	}

	return PrevTimeComplexityCache[RelativePath].Get()->StaticComplexityScore;
}

int64 UMagicScriptInterpreterSubsystem::GetSpaceComplexityCache(
	const FString& RelativePath) const
{
	if (!PrevSpaceComplexityCache.FindRef(RelativePath))
	{
		return 0;
	}

	return PrevSpaceComplexityCache[RelativePath];
}

void UMagicScriptInterpreterSubsystem::TickEventLoops()
{
	// 모든 인터프리터의 이벤트 루프 업데이트
	for (auto& Pair : InterpreterCache)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->GetEventLoop().Tick(Pair.Value.Get());
		}
	}
}

// ============================================================================
// Internal Helper Functions
// ============================================================================

bool UMagicScriptInterpreterSubsystem::CheckScriptByPath(const FString& ScriptPath, FString& Source)
{
	FString ScriptFilePath = FPaths::ProjectSavedDir() / ScriptPath;
	
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ScriptFilePath))
	{
		AddScriptLog(EScriptLogType::Warning, FString::Printf(TEXT("MagicScript: File not found: %s"), *ScriptFilePath));
		return false;
	}

	if (ScriptCache.Find(ScriptPath))
	{
		Source = ScriptCache[ScriptPath];
		return true;
	}
	
	if (!FFileHelper::LoadFileToString(Source, *ScriptFilePath))
	{
		AddScriptLog(EScriptLogType::Warning, FString::Printf(TEXT("MagicScript: Failed to load file: %s"), *ScriptPath));
		return false;
	}

	SaveScriptCache(ScriptPath, Source);
	return true;
}

bool UMagicScriptInterpreterSubsystem::SaveScriptCache(const FString& ScriptPath
	, const FString& Source)
{
	if (!ScriptCache.Find(ScriptPath))
	{
		ScriptCache.Add(ScriptPath, Source);
	} else
	{
		ScriptCache[ScriptPath] = Source;
	}

	ClearScriptCache(ScriptPath);
	
	FString ScriptFilePath = FPaths::ProjectSavedDir() / ScriptPath;
	return FFileHelper::SaveStringToFile(Source, *ScriptFilePath);
}

bool UMagicScriptInterpreterSubsystem::CheckCache_Internal(const FString& RelativePath, const FString& FuncName, FScriptExecutionContext& ExecutionContext)
{
	const TSharedPtr<FProgram>* CachedProgramPtr = ProgramCache.Find(RelativePath);
	if (!CachedProgramPtr)
	{
		return false;
	}

	const TSharedPtr<FProgram> Program = *CachedProgramPtr;
	if (!Program.IsValid())
	{
		return false;
	}

	const TSharedPtr<FInterpreter>* CachedInterpreterPtr = InterpreterCache.Find(RelativePath);
	if (!CachedInterpreterPtr)
	{
		return false;
	}

	const TSharedPtr<FInterpreter> Interpreter = *CachedInterpreterPtr;
	if (!Interpreter.IsValid())
	{
		return false;
	}

	// 시간 복잡도 캐시 확인
	TSharedPtr<FTimeComplexityResult>* CachedTimeComplexity = PrevTimeComplexityCache.Find(RelativePath);
	if (!CachedTimeComplexity)
	{
		PrevTimeComplexityCache.Add(RelativePath, MakeShared<FTimeComplexityResult>());
		CachedTimeComplexity = PrevTimeComplexityCache.Find(RelativePath);
	}

	// 기존 시간, 공간 복잡도 캐싱을 기반으로 사전 점검.
	// 어차피 인 게임 안에서 스크립트를 수정하는 경우는 거의 없고, 있더라도 캐시가 날라가기에 큰 문제는 없다.
	// 라이브에서 수정하는 경우는 캐싱 로직은 타지 않는다는 것이 핵심

	// 기존에 캐싱해둔 프로그램 재실행
	ExecutionContext.Interpreter = Interpreter;
	Interpreter->ExecuteProgram(Program, ExecutionContext);
	
	const double ExecStartTime = FPlatformTime::Seconds();
	const FValue CallFunction = Interpreter->CallFunctionByName(FuncName, {}, ExecutionContext);
	const double ExecEndTime = FPlatformTime::Seconds();

	const TSharedPtr<FTimeComplexityResult> TimeComplexity = *CachedTimeComplexity;
	TimeComplexity.Get()->ExecutionTimeSeconds = ExecEndTime - ExecStartTime;
	TimeComplexity.Get()->DynamicExecutionCount = Interpreter->GetExecutionCount();
	TimeComplexity.Get()->ExpressionEvaluationCount = Interpreter->GetExpressionEvaluationCount();
	TimeComplexity.Get()->FunctionCallCount = Interpreter->GetFunctionCallCount();

	const int64 PeakBytes = Interpreter->GetPeakSpaceBytes();
	PrevSpaceComplexityCache.Add(RelativePath, PeakBytes);
	Interpreter->ResetSpaceTracking();

	if (ExecutionContext.Mode == EExecutionMode::PreAnalysis)
	{
		UE_LOG(LogTemp, Display, TEXT("%s"), *FString::Printf(TEXT(
			"MagicScript PreAnalysis (cached): %s() finished. Return: %s, PeakSpace: %lld bytes, Complexity: %s"),
			*FuncName, *CallFunction.ToDebugString(), PeakBytes, *TimeComplexity.Get()->ToString()));
		return true;
	}
	
	UE_LOG(LogTemp, Display, TEXT("%s"), *FString::Printf(TEXT(
		"MagicScript (cached): %s() finished. Return: %s, PeakSpace: %lld bytes, Complexity: %s"),
		*FuncName, *CallFunction.ToDebugString(), PeakBytes, *TimeComplexity.Get()->ToString()));
	
	return true;
}

bool UMagicScriptInterpreterSubsystem::Lexer_Internal(FLexer& Lexer, TArray<FToken>& Tokens, const FString& ScriptPath)
{
	Tokens = Lexer.Tokenize();

	for (const FToken& Tok : Tokens)
	{
		if (Tok.Type == ETokenType::Error)
		{
			AddScriptLog(EScriptLogType::Error,
				FString::Printf(TEXT("MagicScript Lex Error %s(%d:%d): %s"),
				*ScriptPath, Tok.Location.Line, Tok.Location.Column, *Tok.Lexeme));
			return false;
		}
	}
	
	return true;
}

bool UMagicScriptInterpreterSubsystem::Parsing_Internal(FParser& Parser, const FString& RelativePath)
{
	TSharedPtr<FProgram> Program = Parser.ParseProgram();
	
	if (!Program.IsValid() || Parser.HasError())
	{
		AddScriptLog(EScriptLogType::Error,
			FString::Printf(TEXT("MagicScript: Failed to parse script: %s"), *RelativePath));
		return false;
	}

	if (ProgramCache.Find(RelativePath))
	{
		ProgramCache[RelativePath] = Program;
	}
	else
	{
		ProgramCache.Add(RelativePath, Program);
	}
	return true;
}

bool UMagicScriptInterpreterSubsystem::Import_Internal(const TSharedPtr<FProgram>& Program, const FScriptExecutionContext& ExecutionContext)
{
	TFunction<bool(const TSharedPtr<FProgram>&, TSet<FString>&)> ProcessImports;
	ProcessImports = [this, &ProcessImports, &ExecutionContext](const TSharedPtr<FProgram>& InProgram, TSet<FString>& Visiting) -> bool
	{
		for (const FStatementPtr& Stmt : InProgram->Statements)
		{
			if (!Stmt.IsValid() || Stmt->Kind != EStatementKind::Import)
			{
				continue;
			}

			TSharedPtr<FImportStatement> ImportStmt = StaticCastSharedPtr<FImportStatement>(Stmt);
			const FString ImportRelPath = ImportStmt->Path;

			if (Visiting.Contains(ImportRelPath))
			{
				AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript: Cyclic import detected: %s"), *ImportRelPath));
				return false;
			}

			Visiting.Add(ImportRelPath);

			TSharedPtr<FProgram>* CachedProgramPtr = ProgramCache.Find(ImportRelPath);
			if (!CachedProgramPtr)
			{
				FString ModSource;
				if (!CheckScriptByPath(ImportRelPath, ModSource))
				{
					return false;
				}

				FLexer ModLexer(ModSource);
				TArray<FToken> ModTokens;
				if (!Lexer_Internal(ModLexer, ModTokens, ImportRelPath))
				{
					return false;
				}

				FParser ModParser(ModTokens);
				if (!Parsing_Internal(ModParser, ImportRelPath))
				{
					return false;
				}
				
				CachedProgramPtr = ProgramCache.Find(ImportRelPath);
			}

			RegisterBuiltins_Internal(ImportRelPath);

			if (!CachedProgramPtr || !CachedProgramPtr->IsValid())
			{
				return false;
			}

			if (!ProcessImports(*CachedProgramPtr, Visiting))
			{
				return false;
			}

			InterpreterCache[ImportRelPath]->ExecuteProgram(*CachedProgramPtr, ExecutionContext);
		}

		return true;
	};
	
	TSet<FString> Visited;
	return ProcessImports(Program, Visited);
}

void UMagicScriptInterpreterSubsystem::RunScript_Internal(FTimeComplexityResult& TimeComplexityResult,
	const FString& RelativePath, const FString& FuncName, FScriptExecutionContext& ExecutionContext)
{
	ExecutionContext.Interpreter = InterpreterCache[RelativePath];
	
	const double ExecStartTime = FPlatformTime::Seconds();
	FValue Ret = InterpreterCache[RelativePath]->CallFunctionByName(FuncName, {}, ExecutionContext);
	const double ExecEndTime = FPlatformTime::Seconds();

	// 스코어 점수에, Native 함수 호출 점수 반영
	TimeComplexityResult.StaticComplexityScore += InterpreterCache[RelativePath]->GetAccumulatedTimeComplexityScore();
	TimeComplexityResult.DynamicExecutionCount = InterpreterCache[RelativePath]->GetExecutionCount();
	TimeComplexityResult.ExpressionEvaluationCount = InterpreterCache[RelativePath]->GetExpressionEvaluationCount();
	TimeComplexityResult.FunctionCallCount = InterpreterCache[RelativePath]->GetFunctionCallCount();
	TimeComplexityResult.ExecutionTimeSeconds = ExecEndTime - ExecStartTime;

	const TSharedPtr<FTimeComplexityResult> CacheTimeComplexity = MakeShared<FTimeComplexityResult>(TimeComplexityResult);
	const int64 PeakBytes = InterpreterCache[RelativePath]->GetPeakSpaceBytes();
	
	PrevTimeComplexityCache.Add(RelativePath, CacheTimeComplexity);
	PrevSpaceComplexityCache.Add(RelativePath, PeakBytes);

	if (ExecutionContext.Mode == EExecutionMode::PreAnalysis)
	{
		UE_LOG(LogTemp, Display, TEXT("%s"),
			*FString::Printf(TEXT("MagicScript PreAnalysis: %s() finished. Return: %s, PeakSpace: %lld bytes, Complexity: %s"),
		*FuncName, *Ret.ToDebugString(), PeakBytes, *TimeComplexityResult.ToString()));
		return;
	}
	
	UE_LOG(LogTemp, Display, TEXT("%s"),
		*FString::Printf(TEXT("MagicScript: %s() finished. Return: %s, PeakSpace: %lld bytes, Complexity: %s"),
		*FuncName, *Ret.ToDebugString(), PeakBytes, *TimeComplexityResult.ToString()));
}

void UMagicScriptInterpreterSubsystem::OnRegisterBuiltins(const TSharedPtr<FEnvironment> Env)
{
	MsMathBuiltins::Register(Env, this);
	MsConsoleBuiltins::Register(Env, this);
	MsArrayBuiltins::Register(Env, this);
}

void UMagicScriptInterpreterSubsystem::RegisterBuiltins_Internal(const FString& RelativePath)
{
	TSharedPtr<FInterpreter> Interpreter = InterpreterCache.FindRef(RelativePath);
	if (Interpreter.IsValid())
	{
		return;
	}
	
	InterpreterCache.Add(RelativePath, MakeShared<FInterpreter>());
	const TSharedPtr<FInterpreter> InterpreterPtr = InterpreterCache[RelativePath];
	const TSharedPtr<FEnvironment> Env = InterpreterPtr->GetGlobalEnv();
	if (!Env.IsValid())
	{
		return;
	}
	
	OnRegisterBuiltins(Env);
}
