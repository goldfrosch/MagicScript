#pragma once

#include "CoreMinimal.h"
#include "MagicScript/Core/MsAst.h"

namespace MagicScript
{
	// 스크립트의 시간 복잡도 분석 결과
	struct FTimeComplexityResult
	{
		// 정적 분석 점수 (AST 기반 예상 복잡도)
		int32 StaticComplexityScore = 0;

		// 동적 분석 점수 (실제 실행 횟수)
		int64 DynamicExecutionCount = 0;

		// 문장 수
		int32 StatementCount = 0;

		// 루프 깊이 (최대 중첩)
		int32 MaxLoopDepth = 0;

		// 함수 호출 수
		int32 FunctionCallCount = 0;

		// 표현식 평가 횟수
		int64 ExpressionEvaluationCount = 0;

		// 분석 시간 (초)
		double AnalysisTimeSeconds = 0.0;

		// 실행 시간 (초)
		double ExecutionTimeSeconds = 0.0;

		FString ToString() const
		{
			return FString::Printf(
				TEXT("TimeComplexity[Static:%d, Dynamic:%lld, Statements:%d, MaxLoopDepth:%d, FuncCalls:%d, ExprEvals:%lld, AnalysisTime:%.3fs, ExecTime:%.3fs]"),
				StaticComplexityScore,
				DynamicExecutionCount,
				StatementCount,
				MaxLoopDepth,
				FunctionCallCount,
				ExpressionEvaluationCount,
				AnalysisTimeSeconds,
				ExecutionTimeSeconds
			);
		}
	};

	// AST를 분석하여 시간 복잡도를 계산하는 정적 분석기
	class FTimeComplexityAnalyzer
	{
	public:
		// 프로그램의 시간 복잡도 분석
		static FTimeComplexityResult AnalyzeProgram(const TSharedPtr<FProgram>& Program);

	private:
		// 문장의 복잡도 분석
		static int32 AnalyzeStatement(const FStatementPtr& Stmt, int32 CurrentDepth, FTimeComplexityResult& OutResult);

		// 표현식의 복잡도 분석
		static int32 AnalyzeExpression(const FExpressionPtr& Expr, FTimeComplexityResult& OutResult);

		// 블록의 복잡도 분석
		static int32 AnalyzeBlock(const TSharedPtr<FBlockStatement>& Block, int32 CurrentDepth, FTimeComplexityResult& OutResult);
	};
}

