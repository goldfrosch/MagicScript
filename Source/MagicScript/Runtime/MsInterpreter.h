#pragma once

#include "CoreMinimal.h"
#include "MagicScript/Core/MsAst.h"
#include "MagicScript/Core/MsValue.h"
#include "MagicScript/Core/MsEnvironment.h"
#include "MagicScript/Runtime/MsEventLoop.h"

namespace MagicScript
{
	// 실행 모드 
	enum class EExecutionMode : uint8
	{
		Normal,      // 정상 실행
		PreAnalysis  // 사전 계산 모드
	};

	// 실행 컨텍스트
	struct MAGICSCRIPT_API FScriptExecutionContext
	{
		EExecutionMode Mode = EExecutionMode::Normal;
		
		// PreAnalysis 모드에서 롤백을 위한 Environment 스냅샷
		TSharedPtr<FEnvironment> Snapshot;
		
		// 네이티브 함수에서 스크립트 함수를 호출하기 위한 Interpreter 포인터
		TSharedPtr<FInterpreter> Interpreter = nullptr;
		
		FScriptExecutionContext() = default;
		explicit FScriptExecutionContext(const EExecutionMode InMode) : Mode(InMode) {}
	};

	/**
	 * AST 인터프리터
	 * - Program 실행
	 * - 전역 환경에서 함수 등록/호출
	 */
	
	class MAGICSCRIPT_API FInterpreter
	{
	public:
		FInterpreter() = default;

		// 프로그램 전체 실행 (전역 코드 + 함수 정의 등)
		void ExecuteProgram(const TSharedPtr<FProgram>& Program, const FScriptExecutionContext& Context = FScriptExecutionContext());

		// 전역 환경에 접근
		TSharedPtr<FEnvironment> GetGlobalEnv() const { return GlobalEnv; }

		// 전역에서 이름으로 함수를 찾아 호출
		FValue CallFunctionByName(const FString& Name, const TArray<FValue>& Args, const FScriptExecutionContext& Context = FScriptExecutionContext());

		// 실행 중 사용했던 최대 메모리(추정치, byte 단위)
		int64 GetPeakSpaceBytes() const { return PeakSpaceBytes; }

		// 메모리 사용 통계 리셋
		void ResetSpaceTracking();

		// 동적 실행 카운트 가져오기
		int64 GetExecutionCount() const { return ExecutionCount; }
		int64 GetExpressionEvaluationCount() const { return ExpressionEvaluationCount; }
		int32 GetFunctionCallCount() const { return FunctionCallCount; }
		int32 GetAccumulatedTimeComplexityScore() const { return AccumulatedTimeComplexityScore; }

		// 이벤트 루프 접근
		FEventLoop& GetEventLoop() { return EventLoop; }

		// 내부 함수 호출
		FValue CallFunction(const TSharedPtr<FFunctionValue>& FuncValue, const TArray<FValue>& Args, const FScriptExecutionContext& Context = FScriptExecutionContext());

	private:
		TSharedPtr<FEnvironment> GlobalEnv = MakeShared<FEnvironment>();

		// 메모리 사용 추적용
		int64 CurrentSpaceBytes = 0;
		int64 PeakSpaceBytes = 0;

		// 시간 복잡도 추적용 (동적 분석)
		int64 ExecutionCount = 0;              // 문장 실행 횟수
		int64 ExpressionEvaluationCount = 0;  // 표현식 평가 횟수
		int32 FunctionCallCount = 0;           // 함수 호출 횟수
		int32 AccumulatedTimeComplexityScore = 0;  // Native 함수 호출로 누적된 시간 복잡도 점수

		// 함수 호출 스택 깊이 제한 (무한 재귀 방지, 최대 64번)
		static constexpr int32 MAX_CALL_STACK_DEPTH = 64;
		int32 CallStackDepth = 0;

		// 런타임 에러 발생 시 이후 실행 중단 
		bool bAbortExecution = false;

		void AddSpaceBytes(int64 Delta);
		static int32 EstimateValueSizeBytes(const FValue& V);
		void SignalRuntimeError();

		// 제어 흐름 전달용
		struct FExecResult
		{
			bool   bHasReturn = false;
			FValue ReturnValue = FValue::Null();
		};

		// 문장 실행
		FExecResult ExecuteStatement(const FStatementPtr& Stmt, const TSharedPtr<FEnvironment>& Env, const FScriptExecutionContext& Context);
		FExecResult ExecuteBlock(const TSharedPtr<FBlockStatement>& Block, const TSharedPtr<FEnvironment>& Env, const FScriptExecutionContext& Context);

		// 표현식 평가
		FValue EvaluateExpression(const FExpressionPtr& Expr, const TSharedPtr<FEnvironment>& Env, const FScriptExecutionContext& Context);

		// 이벤트 루프
		FEventLoop EventLoop;
	};
}

