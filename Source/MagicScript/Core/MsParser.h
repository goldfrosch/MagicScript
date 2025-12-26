#pragma once

#include "CoreMinimal.h"
#include "MagicScript/Core/MsToken.h"
#include "MagicScript/Core/MsAst.h"

namespace MagicScript
{
	/**
	 * 재귀 하향 파서 뼈대
	 * - EBNF 스펙에 맞는 AST 생성 책임
	 * - 현재는 인터페이스와 주요 진입점만 정의
	 */
	class MAGICSCRIPT_API FParser
	{
	public:
		FParser(const TArray<FToken>& InTokens);

		// 프로그램 전체 파싱
		TSharedPtr<FProgram> ParseProgram();

		// 파싱 도중 에러가 발생했는지 여부
		bool HasError() const { return bHadError; }

		// 에러 메시지 목록 (간단 텍스트)
		const TArray<FString>& GetErrors() const { return ErrorMessages; }

	private:
		const TArray<FToken>& Tokens;
		int32 Current = 0;

		bool bHadError = false;
		TArray<FString> ErrorMessages;

		const FToken& Peek() const;
		const FToken& Previous() const;
		bool IsAtEnd() const;
		bool Check(ETokenType Type) const;
		bool Match(const TArray<ETokenType>& Types);
		const FToken& Advance();
		const FToken& Consume(ETokenType Type, const FString& ErrorMessage);

		void ReportError(const FToken& AtToken, const FString& Message);
		void Synchronize();

		// 문장
		FStatementPtr ParseStatement();
		FStatementPtr ParseVariableDeclaration(bool bIsConst);
		FStatementPtr ParseFunctionDeclaration();
		FStatementPtr ParseImportStatement();
		FStatementPtr ParseIfStatement();
		FStatementPtr ParseSwitchStatement();
		FStatementPtr ParseWhileStatement();
		FStatementPtr ParseForStatement();
		FStatementPtr ParseReturnStatement();
		TSharedPtr<FBlockStatement> ParseBlockStatement();
		FStatementPtr ParseExpressionStatement();

		// 표현식
		FExpressionPtr ParseExpression();
		FExpressionPtr ParseAssignment();
		FExpressionPtr ParseLogicalOr();
		FExpressionPtr ParseLogicalAnd();
		FExpressionPtr ParseEquality();
		FExpressionPtr ParseRelational();
		FExpressionPtr ParseAdditive();
		FExpressionPtr ParseMultiplicative();
		FExpressionPtr ParseUnary();
		FExpressionPtr ParsePrimary();

		FExpressionPtr ParseCallOrIdentifier();
		FExpressionPtr ParsePostfix(FExpressionPtr Left);
		FExpressionPtr ParseLiteral(const FToken& Token);
		FExpressionPtr ParseArrowFunction(const TArray<FString>& Parameters);
	};
}

