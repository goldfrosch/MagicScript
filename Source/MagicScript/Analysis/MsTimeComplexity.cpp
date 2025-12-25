#include "MagicScript/Analysis/MsTimeComplexity.h"
#include "HAL/PlatformTime.h"

namespace MagicScript
{
	FTimeComplexityResult FTimeComplexityAnalyzer::AnalyzeProgram(const TSharedPtr<FProgram>& Program)
	{
		FTimeComplexityResult Result;
		
		if (!Program.IsValid())
		{
			return Result;
		}

		const double StartTime = FPlatformTime::Seconds();

		// 모든 문장 분석
		for (const FStatementPtr& Stmt : Program->Statements)
		{
			if (Stmt.IsValid())
			{
				AnalyzeStatement(Stmt, 0, Result);
			}
		}

		Result.AnalysisTimeSeconds = FPlatformTime::Seconds() - StartTime;
		Result.StaticComplexityScore = Result.StatementCount + Result.MaxLoopDepth * 5 + Result.FunctionCallCount * 0.5;

		return Result;
	}

	int32 FTimeComplexityAnalyzer::AnalyzeStatement(const FStatementPtr& Stmt, int32 CurrentDepth, FTimeComplexityResult& OutResult)
	{
		if (!Stmt.IsValid())
		{
			return 0;
		}

		OutResult.StatementCount++;
		int32 Complexity = 1;

		switch (Stmt->Kind)
		{
		case EStatementKind::Block:
		{
			TSharedPtr<FBlockStatement> Block = StaticCastSharedPtr<FBlockStatement>(Stmt);
			Complexity += AnalyzeBlock(Block, CurrentDepth, OutResult);
			break;
		}

		case EStatementKind::VarDecl:
		{
			TSharedPtr<FVarDeclStatement> VarDecl = StaticCastSharedPtr<FVarDeclStatement>(Stmt);
			if (VarDecl->Initializer.IsValid())
			{
				Complexity += AnalyzeExpression(VarDecl->Initializer, OutResult);
			}
			break;
		}

		case EStatementKind::FuncDecl:
		{
			TSharedPtr<FFuncDeclStatement> FuncDecl = StaticCastSharedPtr<FFuncDeclStatement>(Stmt);
			if (FuncDecl->Body.IsValid())
			{
				Complexity += AnalyzeBlock(StaticCastSharedPtr<FBlockStatement>(FuncDecl->Body), CurrentDepth, OutResult);
			}
			break;
		}

		case EStatementKind::If:
		{
			TSharedPtr<FIfStatement> IfStmt = StaticCastSharedPtr<FIfStatement>(Stmt);
			if (IfStmt->Condition.IsValid())
			{
				Complexity += AnalyzeExpression(IfStmt->Condition, OutResult);
			}
			if (IfStmt->ThenBranch.IsValid())
			{
				Complexity += AnalyzeStatement(IfStmt->ThenBranch, CurrentDepth, OutResult);
			}
			if (IfStmt->ElseBranch.IsValid())
			{
				Complexity += AnalyzeStatement(IfStmt->ElseBranch, CurrentDepth, OutResult);
			}
			break;
		}

		case EStatementKind::For:
		{
			TSharedPtr<FForStatement> ForStmt = StaticCastSharedPtr<FForStatement>(Stmt);
			const int32 LoopDepth = CurrentDepth + 1;
			OutResult.MaxLoopDepth = FMath::Max(OutResult.MaxLoopDepth, LoopDepth);

			// For 루프는 복잡도가 높음 (반복 가능성)
			Complexity += 50;

			if (ForStmt->Init.IsValid())
			{
				Complexity += AnalyzeStatement(ForStmt->Init, CurrentDepth, OutResult);
			}
			if (ForStmt->Condition.IsValid())
			{
				Complexity += AnalyzeExpression(ForStmt->Condition, OutResult);
			}
			if (ForStmt->Increment.IsValid())
			{
				Complexity += AnalyzeExpression(ForStmt->Increment, OutResult);
			}
			if (ForStmt->Body.IsValid())
			{
				Complexity += AnalyzeStatement(ForStmt->Body, LoopDepth, OutResult);
			}
			break;
		}

		case EStatementKind::Return:
		{
			TSharedPtr<FReturnStatement> RetStmt = StaticCastSharedPtr<FReturnStatement>(Stmt);
			if (RetStmt->Value.IsValid())
			{
				Complexity += AnalyzeExpression(RetStmt->Value, OutResult);
			}
			break;
		}

		case EStatementKind::Expr:
		{
			TSharedPtr<FExpressionStatement> ExprStmt = StaticCastSharedPtr<FExpressionStatement>(Stmt);
			if (ExprStmt->Expr.IsValid())
			{
				Complexity += AnalyzeExpression(ExprStmt->Expr, OutResult);
			}
			break;
		}

		case EStatementKind::Import:
			// Import는 복잡도에 영향 없음
			break;

		default:
			break;
		}

		return Complexity;
	}

	int32 FTimeComplexityAnalyzer::AnalyzeExpression(const FExpressionPtr& Expr, FTimeComplexityResult& OutResult)
	{
		if (!Expr.IsValid())
		{
			return 0;
		}

		int32 Complexity = 1;

		switch (Expr->Kind)
		{
		case EExpressionKind::Binary:
		{
			TSharedPtr<FBinaryExpression> Bin = StaticCastSharedPtr<FBinaryExpression>(Expr);
			Complexity += AnalyzeExpression(Bin->Left, OutResult);
			Complexity += AnalyzeExpression(Bin->Right, OutResult);
			break;
		}

		case EExpressionKind::Unary:
		{
			TSharedPtr<FUnaryExpression> Un = StaticCastSharedPtr<FUnaryExpression>(Expr);
			Complexity += AnalyzeExpression(Un->Operand, OutResult);
			break;
		}

		case EExpressionKind::Call:
		{
			TSharedPtr<FCallExpression> Call = StaticCastSharedPtr<FCallExpression>(Expr);
			OutResult.FunctionCallCount++;
			Complexity += 5; // 함수 호출은 추가 복잡도

			for (const FExpressionPtr& Arg : Call->Arguments)
			{
				Complexity += AnalyzeExpression(Arg, OutResult);
			}
			break;
		}

		case EExpressionKind::Assignment:
		{
			TSharedPtr<FAssignmentExpression> Assign = StaticCastSharedPtr<FAssignmentExpression>(Expr);
			Complexity += AnalyzeExpression(Assign->Value, OutResult);
			break;
		}

		case EExpressionKind::Grouping:
		{
			TSharedPtr<FGroupingExpression> Group = StaticCastSharedPtr<FGroupingExpression>(Expr);
			Complexity += AnalyzeExpression(Group->Inner, OutResult);
			break;
		}

		case EExpressionKind::Literal:
		case EExpressionKind::Identifier:
			// 리터럴과 식별자는 기본 복잡도 1
			break;

		default:
			break;
		}

		return Complexity;
	}

	int32 FTimeComplexityAnalyzer::AnalyzeBlock(const TSharedPtr<FBlockStatement>& Block, int32 CurrentDepth, FTimeComplexityResult& OutResult)
	{
		if (!Block.IsValid())
		{
			return 0;
		}

		int32 Complexity = 0;
		for (const FStatementPtr& Stmt : Block->Statements)
		{
			if (Stmt.IsValid())
			{
				Complexity += AnalyzeStatement(Stmt, CurrentDepth, OutResult);
			}
		}

		return Complexity;
	}
}

