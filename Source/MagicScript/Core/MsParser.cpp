#include "MagicScript/Core/MsParser.h"

namespace MagicScript
{
	FParser::FParser(const TArray<FToken>& InTokens)
		: Tokens(InTokens)
	{
	}

	const FToken& FParser::Peek() const
	{
		// 주석 토큰 스킵
		int32 Index = Current;
		while (Index < Tokens.Num() && Tokens[Index].Type == ETokenType::Comment)
		{
			Index++;
		}
		// 범위 체크
		if (Index >= Tokens.Num())
		{
			return Tokens.Last(); // EndOfFile 토큰 반환
		}
		return Tokens[Index];
	}

	const FToken& FParser::Previous() const
	{
		return Tokens[Current - 1];
	}

	bool FParser::IsAtEnd() const
	{
		return Peek().Type == ETokenType::EndOfFile;
	}

	bool FParser::Check(ETokenType Type) const
	{
		return !IsAtEnd() && Peek().Type == Type;
	}

	bool FParser::Match(const TArray<ETokenType>& Types)
	{
		for (ETokenType Type : Types)
		{
			if (Check(Type))
			{
				Advance();
				return true;
			}
		}
		return false;
	}

	const FToken& FParser::Advance()
	{
		if (!IsAtEnd())
		{
			++Current;
			// 주석 토큰 스킵
			while (Current < Tokens.Num() && Tokens[Current].Type == ETokenType::Comment)
			{
				++Current;
			}
		}
		return Previous();
	}

	const FToken& FParser::Consume(ETokenType Type, const FString& ErrorMessage)
	{
		if (Check(Type))
		{
			return Advance();
		}

		// 기대한 토큰이 아니면 에러 기록
		ReportError(Peek(), ErrorMessage);
		return Tokens.Last();
	}

	void FParser::ReportError(const FToken& AtToken, const FString& Message)
	{
		bHadError = true;

		FString TokenDisplay = AtToken.Lexeme;
		if (TokenDisplay.Len() > 50)
		{
			TokenDisplay = TokenDisplay.Left(50) + TEXT("...");
		}

		const FString FullMsg = FString::Printf(
			TEXT("[Syntax Error] Line %d, Column %d: %s\n"
			     "  Token: '%s' (Type: %d)\n"
			     "  This is a syntax error in your script. Please check the syntax at this location."),
			AtToken.Location.Line,
			AtToken.Location.Column,
			*Message,
			*TokenDisplay,
			static_cast<int32>(AtToken.Type)
		);

		ErrorMessages.Add(FullMsg);
		AddScriptLog(EScriptLogType::Error, FullMsg);
		UE_LOG(LogTemp, Error, TEXT("%s"), *FullMsg);

		// 에러가 발생했을 때 현재 토큰 위치를 앞으로 진행시켜
		// 같은 위치에서 무한히 ParseStatement/ParseExpression을 반복하지 않도록 한다.
		Synchronize();
	}

	void FParser::Synchronize()
	{
		// 최소 한 토큰은 소비
		Advance();

		while (!IsAtEnd())
		{
			// 직전 토큰이 세미콜론이면 문장 단위로 동기화
			if (Previous().Type == ETokenType::Semicolon)
			{
				return;
			}

			// 다음 토큰이 문장 시작 키워드면 여기서 멈추고 상위에서 처리
			switch (Peek().Type)
			{
			case ETokenType::Let:
			case ETokenType::Const:
			case ETokenType::Function:
			case ETokenType::Import:
			case ETokenType::If:
			case ETokenType::Switch:
			case ETokenType::While:
			case ETokenType::For:
			case ETokenType::Return:
			case ETokenType::LBrace:
				return;
			default:
				break;
			}

			Advance();
		}
	}

	TSharedPtr<FProgram> FParser::ParseProgram()
	{
		TSharedPtr<FProgram> Program = MakeShared<FProgram>();

		while (!IsAtEnd())
		{
			FStatementPtr Stmt = ParseStatement();

			// 파싱 과정에서 에러가 발생했다면 더 이상 진행하지 않고 중단
			if (bHadError)
			{
				break;
			}

			if (Stmt.IsValid())
			{
				Program->Statements.Add(Stmt);
			}
			else
			{
				// 에러 시 토큰 하나 소비하고 계속
				Advance();
			}
		}

		return Program;
	}

	FStatementPtr FParser::ParseStatement()
	{
		if (Match({ ETokenType::Import }))
		{
			return ParseImportStatement();
		}
		if (Match({ ETokenType::Let }))
		{
			return ParseVariableDeclaration(false);
		}
		if (Match({ ETokenType::Const }))
		{
			return ParseVariableDeclaration(true);
		}
		if (Match({ ETokenType::Function }))
		{
			return ParseFunctionDeclaration();
		}
		if (Match({ ETokenType::If }))
		{
			return ParseIfStatement();
		}
		if (Match({ ETokenType::Switch }))
		{
			return ParseSwitchStatement();
		}
		if (Match({ ETokenType::While }))
		{
			return ParseWhileStatement();
		}
		if (Match({ ETokenType::For }))
		{
			return ParseForStatement();
		}
		if (Match({ ETokenType::Return }))
		{
			return ParseReturnStatement();
		}
		if (Match({ ETokenType::LBrace }))
		{
			return ParseBlockStatement();
		}

		return ParseExpressionStatement();
	}

	FStatementPtr FParser::ParseWhileStatement()
	{
		// while (condition) statement
		Consume(ETokenType::LParen, TEXT("Expected '(' after 'while'."));
		FExpressionPtr Condition = ParseExpression();
		Consume(ETokenType::RParen, TEXT("Expected ')' after while condition."));

		FStatementPtr Body = ParseStatement();

		TSharedPtr<FWhileStatement> WhileStmt = MakeShared<FWhileStatement>();
		WhileStmt->Condition = Condition;
		WhileStmt->Body = Body;
		return WhileStmt;
	}

	FStatementPtr FParser::ParseImportStatement()
	{
		// import "Scripts/Util.ms";
		const FToken& PathTok = Consume(ETokenType::String, TEXT("Expected string literal after 'import'."));
		Consume(ETokenType::Semicolon, TEXT("Expected ';' after import statement."));

		TSharedPtr<FImportStatement> ImportStmt = MakeShared<FImportStatement>();
		ImportStmt->Path = PathTok.Lexeme;
		return ImportStmt;
	}

	FStatementPtr FParser::ParseVariableDeclaration(bool bIsConst)
	{
		const FToken& NameTok = Consume(ETokenType::Identifier, TEXT("Expected variable name."));

		TSharedPtr<FVarDeclStatement> Decl = MakeShared<FVarDeclStatement>();
		Decl->bIsConst = bIsConst;
		Decl->Name = NameTok.Lexeme;

		if (Match({ ETokenType::Equal }))
		{
			Decl->Initializer = ParseExpression();
		}

		Consume(ETokenType::Semicolon, TEXT("Expected ';' after variable declaration."));
		return Decl;
	}

	FStatementPtr FParser::ParseFunctionDeclaration()
	{
		const FToken& NameTok = Consume(ETokenType::Identifier, TEXT("Expected function name."));
		Consume(ETokenType::LParen, TEXT("Expected '(' after function name."));

		TSharedPtr<FFuncDeclStatement> Func = MakeShared<FFuncDeclStatement>();
		Func->Name = NameTok.Lexeme;

		if (!Check(ETokenType::RParen))
		{
			do
			{
				const FToken& ParamTok = Consume(ETokenType::Identifier, TEXT("Expected parameter name."));
				Func->Parameters.Add(ParamTok.Lexeme);
			}
			while (Match({ ETokenType::Comma }));
		}

		Consume(ETokenType::RParen, TEXT("Expected ')' after parameters."));
		Consume(ETokenType::LBrace, TEXT("Expected '{' before function body."));

		Func->Body = StaticCastSharedPtr<FBlockStatement>(ParseBlockStatement());
		return Func;
	}

	FStatementPtr FParser::ParseIfStatement()
	{
		Consume(ETokenType::LParen, TEXT("Expected '(' after 'if'."));
		FExpressionPtr Condition = ParseExpression();
		Consume(ETokenType::RParen, TEXT("Expected ')' after if condition."));

		FStatementPtr ThenBranch = ParseStatement();
		FStatementPtr ElseBranch;

		if (Match({ ETokenType::Else }))
		{
			ElseBranch = ParseStatement();
		}

		TSharedPtr<FIfStatement> IfStmt = MakeShared<FIfStatement>();
		IfStmt->Condition = Condition;
		IfStmt->ThenBranch = ThenBranch;
		IfStmt->ElseBranch = ElseBranch;
		return IfStmt;
	}

	FStatementPtr FParser::ParseSwitchStatement()
	{
		// switch (expression) { case value: statements ... default: statements }
		Consume(ETokenType::LParen, TEXT("Expected '(' after 'switch'."));
		FExpressionPtr Expression = ParseExpression();
		Consume(ETokenType::RParen, TEXT("Expected ')' after switch expression."));
		Consume(ETokenType::LBrace, TEXT("Expected '{' after switch expression."));

		TSharedPtr<FSwitchStatement> SwitchStmt = MakeShared<FSwitchStatement>();
		SwitchStmt->Expression = Expression;

		bool bFoundDefault = false;

		while (!Check(ETokenType::RBrace) && !IsAtEnd())
		{
			if (Match({ ETokenType::Case }))
			{
				FSwitchCase Case;
				Case.Value = ParseExpression();
				Consume(ETokenType::Colon, TEXT("Expected ':' after case value."));

				// case 다음의 문장들을 읽어서 break나 다음 case/default까지 수집
				while (!Check(ETokenType::Case) && !Check(ETokenType::Default) && !Check(ETokenType::RBrace) && !IsAtEnd())
				{
					FStatementPtr Stmt = ParseStatement();
					if (Stmt.IsValid())
					{
						Case.Statements.Add(Stmt);
					}
				}

				SwitchStmt->Cases.Add(Case);
			}
			else if (Match({ ETokenType::Default }))
			{
				if (bFoundDefault)
				{
					ReportError(Peek(), TEXT("Multiple 'default' cases in switch statement."));
				}
				bFoundDefault = true;

				FSwitchCase DefaultCase;
				DefaultCase.Value = nullptr;  // default는 값이 없음
				Consume(ETokenType::Colon, TEXT("Expected ':' after 'default'."));

				// default 다음의 문장들을 읽어서 } 까지 수집
				while (!Check(ETokenType::RBrace) && !IsAtEnd())
				{
					FStatementPtr Stmt = ParseStatement();
					if (Stmt.IsValid())
					{
						DefaultCase.Statements.Add(Stmt);
					}
				}

				SwitchStmt->Cases.Add(DefaultCase);
			}
			else
			{
				ReportError(Peek(), TEXT("Expected 'case' or 'default' in switch statement."));
				break;
			}
		}

		Consume(ETokenType::RBrace, TEXT("Expected '}' after switch statement."));
		return SwitchStmt;
	}

	FStatementPtr FParser::ParseForStatement()
	{
		Consume(ETokenType::LParen, TEXT("Expected '(' after 'for'."));

		FStatementPtr Init;
		if (!Check(ETokenType::Semicolon))
		{
			if (Match({ ETokenType::Let }))
			{
				Init = ParseVariableDeclaration(false);
			}
			else if (Match({ ETokenType::Const }))
			{
				Init = ParseVariableDeclaration(true);
			}
			else
			{
				Init = ParseExpressionStatement();
			}
		}
		else
		{
			Consume(ETokenType::Semicolon, TEXT("Expected ';' after for initializer."));
		}

		FExpressionPtr Condition;
		if (!Check(ETokenType::Semicolon))
		{
			Condition = ParseExpression();
		}
		Consume(ETokenType::Semicolon, TEXT("Expected ';' after for condition."));

		FExpressionPtr Increment;
		if (!Check(ETokenType::RParen))
		{
			Increment = ParseExpression();
		}
		Consume(ETokenType::RParen, TEXT("Expected ')' after for clauses."));

		FStatementPtr Body = ParseStatement();

		TSharedPtr<FForStatement> ForStmt = MakeShared<FForStatement>();
		ForStmt->Init = Init;
		ForStmt->Condition = Condition;
		ForStmt->Increment = Increment;
		ForStmt->Body = Body;
		return ForStmt;
	}

	FStatementPtr FParser::ParseReturnStatement()
	{
		TSharedPtr<FReturnStatement> Ret = MakeShared<FReturnStatement>();

		if (!Check(ETokenType::Semicolon))
		{
			Ret->Value = ParseExpression();
		}

		Consume(ETokenType::Semicolon, TEXT("Expected ';' after return value."));
		return Ret;
	}

	TSharedPtr<FBlockStatement> FParser::ParseBlockStatement()
	{
		TSharedPtr<FBlockStatement> Block = MakeShared<FBlockStatement>();

		while (!Check(ETokenType::RBrace) && !IsAtEnd())
		{
			FStatementPtr Stmt = ParseStatement();
			if (Stmt.IsValid())
			{
				Block->Statements.Add(Stmt);
			}
			else
			{
				Advance();
			}
		}

		Consume(ETokenType::RBrace, TEXT("Expected '}' after block."));
		return Block;
	}

	FStatementPtr FParser::ParseExpressionStatement()
	{
		FExpressionPtr Expr = ParseExpression();
		Consume(ETokenType::Semicolon, TEXT("Expected ';' after expression."));

		TSharedPtr<FExpressionStatement> Stmt = MakeShared<FExpressionStatement>();
		Stmt->Expr = Expr;
		return Stmt;
	}

	// === 표현식 파싱 ===

	FExpressionPtr FParser::ParseExpression()
	{
		return ParseAssignment();
	}

	FExpressionPtr FParser::ParseAssignment()
	{
		// 좌변은 Identifier 로 제한 (초기 버전)
		FExpressionPtr Left = ParseLogicalOr();

		// 복합 할당 연산자 확인
		EAssignmentOp AssignOp = EAssignmentOp::Assign;
		if (Match({ ETokenType::Equal }))
		{
			AssignOp = EAssignmentOp::Assign;
		}
		else if (Match({ ETokenType::PlusEqual }))
		{
			AssignOp = EAssignmentOp::AddAssign;
		}
		else if (Match({ ETokenType::MinusEqual }))
		{
			AssignOp = EAssignmentOp::SubAssign;
		}
		else if (Match({ ETokenType::StarEqual }))
		{
			AssignOp = EAssignmentOp::MulAssign;
		}
		else if (Match({ ETokenType::SlashEqual }))
		{
			AssignOp = EAssignmentOp::DivAssign;
		}
		else if (Match({ ETokenType::PercentEqual }))
		{
			AssignOp = EAssignmentOp::ModAssign;
		}
		else
		{
			return Left;
		}

		FExpressionPtr Value = ParseAssignment();

		if (Left.IsValid() && Left->Kind == EExpressionKind::Identifier)
		{
			TSharedPtr<FIdentifierExpression> Ident = StaticCastSharedPtr<FIdentifierExpression>(Left);
			TSharedPtr<FAssignmentExpression> Assign = MakeShared<FAssignmentExpression>();
			Assign->Op = AssignOp;
			Assign->TargetName = Ident->Name;
			Assign->Value = Value;
			return Assign;
		}
		else if (Left.IsValid() && Left->Kind == EExpressionKind::MemberAccess)
		{
			// 객체 멤버 할당: obj.property = value
			TSharedPtr<FMemberAccessExpression> MemberAccess = StaticCastSharedPtr<FMemberAccessExpression>(Left);
			
			if (MemberAccess->Target->Kind == EExpressionKind::Identifier)
			{
				TSharedPtr<FIdentifierExpression> Ident = StaticCastSharedPtr<FIdentifierExpression>(MemberAccess->Target);
				TSharedPtr<FAssignmentExpression> Assign = MakeShared<FAssignmentExpression>();
				Assign->Op = AssignOp;
				Assign->TargetName = Ident->Name;
				Assign->MemberName = MemberAccess->MemberName;
				Assign->Value = Value;
				
				// 복합 할당은 나중에 지원
				if (AssignOp != EAssignmentOp::Assign)
				{
					ReportError(Previous(), TEXT("Compound assignment to object member is not supported yet."));
					return Left;
				}
				
				return Assign;
			}
			else
			{
				ReportError(Previous(), TEXT("Invalid assignment target for member access."));
				return Left;
			}
		}
		else if (Left.IsValid() && Left->Kind == EExpressionKind::Index)
		{
			// 배열 인덱싱 할당: arr[0] = value
			TSharedPtr<FIndexExpression> IndexExpr = StaticCastSharedPtr<FIndexExpression>(Left);
			TSharedPtr<FAssignmentExpression> Assign = MakeShared<FAssignmentExpression>();
			Assign->Op = AssignOp;
			
			if (IndexExpr->Target->Kind == EExpressionKind::Identifier)
			{
				TSharedPtr<FIdentifierExpression> Ident = StaticCastSharedPtr<FIdentifierExpression>(IndexExpr->Target);
				Assign->TargetName = Ident->Name;
				Assign->Index = IndexExpr->Index;
				Assign->Value = Value;
				
				// 복합 할당은 나중에 지원
				if (AssignOp != EAssignmentOp::Assign)
				{
					ReportError(Previous(), TEXT("Compound assignment to array index not yet supported."));
					return Left;
				}
			}
			else
			{
				ReportError(Previous(), TEXT("Invalid assignment target for indexed expression."));
				return Left;
			}
			return Assign;
		}

		// 잘못된 대입 대상
		ReportError(Previous(), TEXT("Invalid assignment target."));
		return Left;
	}

	FExpressionPtr FParser::ParseLogicalOr()
	{
		FExpressionPtr Expr = ParseLogicalAnd();

		while (Match({ ETokenType::OrOr }))
		{
			TSharedPtr<FBinaryExpression> Bin = MakeShared<FBinaryExpression>();
			Bin->Op = EBinaryOp::Or;
			Bin->Left = Expr;
			Bin->Right = ParseLogicalAnd();
			Expr = Bin;
		}

		return Expr;
	}

	FExpressionPtr FParser::ParseLogicalAnd()
	{
		FExpressionPtr Expr = ParseEquality();

		while (Match({ ETokenType::AndAnd }))
		{
			TSharedPtr<FBinaryExpression> Bin = MakeShared<FBinaryExpression>();
			Bin->Op = EBinaryOp::And;
			Bin->Left = Expr;
			Bin->Right = ParseEquality();
			Expr = Bin;
		}

		return Expr;
	}

	FExpressionPtr FParser::ParseEquality()
	{
		FExpressionPtr Expr = ParseRelational();

		while (Match({ ETokenType::EqualEqual, ETokenType::BangEqual }))
		{
			const FToken& OpTok = Previous();
			TSharedPtr<FBinaryExpression> Bin = MakeShared<FBinaryExpression>();
			Bin->Op = (OpTok.Type == ETokenType::EqualEqual) ? EBinaryOp::Equal : EBinaryOp::NotEqual;
			Bin->Left = Expr;
			Bin->Right = ParseRelational();
			Expr = Bin;
		}

		return Expr;
	}

	FExpressionPtr FParser::ParseRelational()
	{
		FExpressionPtr Expr = ParseAdditive();

		while (Match({ ETokenType::Less, ETokenType::LessEqual, ETokenType::Greater, ETokenType::GreaterEqual }))
		{
			const FToken& OpTok = Previous();
			TSharedPtr<FBinaryExpression> Bin = MakeShared<FBinaryExpression>();

			switch (OpTok.Type)
			{
			case ETokenType::Less:          Bin->Op = EBinaryOp::Less; break;
			case ETokenType::LessEqual:     Bin->Op = EBinaryOp::LessEqual; break;
			case ETokenType::Greater:       Bin->Op = EBinaryOp::Greater; break;
			case ETokenType::GreaterEqual:  Bin->Op = EBinaryOp::GreaterEqual; break;
			default:                        Bin->Op = EBinaryOp::Less; break;
			}

			Bin->Left = Expr;
			Bin->Right = ParseAdditive();
			Expr = Bin;
		}

		return Expr;
	}

	FExpressionPtr FParser::ParseAdditive()
	{
		FExpressionPtr Expr = ParseMultiplicative();

		while (Match({ ETokenType::Plus, ETokenType::Minus }))
		{
			const FToken& OpTok = Previous();
			TSharedPtr<FBinaryExpression> Bin = MakeShared<FBinaryExpression>();
			Bin->Op = (OpTok.Type == ETokenType::Plus) ? EBinaryOp::Add : EBinaryOp::Sub;
			Bin->Left = Expr;
			Bin->Right = ParseMultiplicative();
			Expr = Bin;
		}

		return Expr;
	}

	FExpressionPtr FParser::ParseMultiplicative()
	{
		FExpressionPtr Expr = ParseUnary();

		while (Match({ ETokenType::Star, ETokenType::Slash, ETokenType::Percent }))
		{
			const FToken& OpTok = Previous();
			TSharedPtr<FBinaryExpression> Bin = MakeShared<FBinaryExpression>();

			switch (OpTok.Type)
			{
			case ETokenType::Star:    Bin->Op = EBinaryOp::Mul; break;
			case ETokenType::Slash:   Bin->Op = EBinaryOp::Div; break;
			case ETokenType::Percent: Bin->Op = EBinaryOp::Mod; break;
			default:                  Bin->Op = EBinaryOp::Mul; break;
			}

			Bin->Left = Expr;
			Bin->Right = ParseUnary();
			Expr = Bin;
		}

		return Expr;
	}

	FExpressionPtr FParser::ParseUnary()
	{
		if (Match({ ETokenType::Bang }))
		{
			TSharedPtr<FUnaryExpression> Expr = MakeShared<FUnaryExpression>();
			Expr->Op = EUnaryOp::Not;
			Expr->Operand = ParseUnary();
			return Expr;
		}
		if (Match({ ETokenType::Minus }))
		{
			TSharedPtr<FUnaryExpression> Expr = MakeShared<FUnaryExpression>();
			Expr->Op = EUnaryOp::Negate;
			Expr->Operand = ParseUnary();
			return Expr;
		}
		if (Match({ ETokenType::PlusPlus }))
		{
			TSharedPtr<FUnaryExpression> Expr = MakeShared<FUnaryExpression>();
			Expr->Op = EUnaryOp::PreIncrement;
			Expr->Operand = ParseUnary();
			return Expr;
		}
		if (Match({ ETokenType::MinusMinus }))
		{
			TSharedPtr<FUnaryExpression> Expr = MakeShared<FUnaryExpression>();
			Expr->Op = EUnaryOp::PreDecrement;
			Expr->Operand = ParseUnary();
			return Expr;
		}

		return ParseCallOrIdentifier();
	}

	FExpressionPtr FParser::ParseCallOrIdentifier()
	{
		// 객체 리터럴
		if (Match({ ETokenType::LBrace }))
		{
			TSharedPtr<FObjectLiteralExpression> ObjectLit = MakeShared<FObjectLiteralExpression>();

			// 빈 객체 체크
			if (Check(ETokenType::RBrace))
			{
				Advance(); // } 소비
				return ObjectLit;
			}

			// 속성 파싱
			while (true)
			{
				FObjectProperty Prop;
				
				// 키는 식별자 또는 문자열 리터럴
				if (Match({ ETokenType::Identifier }))
				{
					Prop.Key = Previous().Lexeme;
				}
				else if (Match({ ETokenType::String }))
				{
					Prop.Key = Previous().Lexeme;
					// 따옴표 제거
					if (Prop.Key.Len() >= 2 && Prop.Key[0] == TEXT('"') && Prop.Key[Prop.Key.Len() - 1] == TEXT('"'))
					{
						Prop.Key = Prop.Key.Mid(1, Prop.Key.Len() - 2);
					}
				}
				else
				{
					// trailing comma 허용: 마지막 쉼표 후 바로 }가 오는 경우
					if (Check(ETokenType::RBrace))
					{
						break;
					}
					ReportError(Peek(), TEXT("Expected property name (identifier or string) in object literal."));
					break;
				}

				// 콜론(:) 기대
				Consume(ETokenType::Colon, TEXT("Expected ':' after property name in object literal."));
				
				// 값 표현식 파싱
				Prop.Value = ParseExpression();
				
				ObjectLit->Properties.Add(Prop);
				
				// trailing comma 체크: 쉼표 후 바로 }가 오는 경우 허용
				if (Match({ ETokenType::Comma }))
				{
					if (Check(ETokenType::RBrace))
					{
						break;
					}
					// 쉼표가 있고 다음 속성이 있으면 계속
				}
				else
				{
					// 쉼표가 없으면 종료
					break;
				}
			}

			Consume(ETokenType::RBrace, TEXT("Expected '}' after object properties."));
			return ObjectLit;
		}

		// 배열 리터럴
		if (Match({ ETokenType::LBracket }))
		{
			TSharedPtr<FArrayLiteralExpression> ArrayLit = MakeShared<FArrayLiteralExpression>();

			if (!Check(ETokenType::RBracket))
			{
				do
				{
					ArrayLit->Elements.Add(ParseExpression());
				}
				while (Match({ ETokenType::Comma }));
			}

			Consume(ETokenType::RBracket, TEXT("Expected ']' after array elements."));
			return ArrayLit;
		}

		// 기본 단위
		if (Match({ ETokenType::LParen }))
		{
			// Arrow 함수인지 확인: () => ... 또는 (param) => ... 또는 (param1, param2) => ...
			int32 SavedCurrent = Current;
			TArray<FString> Params;
			
			// 파라미터 목록 파싱 시도
			if (Check(ETokenType::Identifier))
			{
				// 파라미터가 있는 경우
				do
				{
					const FToken& ParamTok = Advance();
					if (ParamTok.Type == ETokenType::Identifier)
					{
						Params.Add(ParamTok.Lexeme);
					}
					else
					{
						break;
					}
				}
				while (Match({ ETokenType::Comma }));
			}
			
			// ) 다음에 => 가 오는지 확인 (파라미터가 있든 없든)
			// 파라미터가 없는 경우 () => 형태도 처리
			if (Check(ETokenType::RParen))
			{
				Advance(); // ) 소비
				if (Check(ETokenType::Arrow))
				{
					Advance(); // => 소비
					// Arrow 함수 파싱
					return ParseArrowFunction(Params);
				}
				else
				{
					// ) 다음에 => 가 없으면 Arrow 함수가 아님
					// 원래대로 그룹 표현식으로 파싱
					Current = SavedCurrent;
					FExpressionPtr Inner = ParseExpression();
					Consume(ETokenType::RParen, TEXT("Expected ')' after expression."));

					TSharedPtr<FGroupingExpression> Group = MakeShared<FGroupingExpression>();
					Group->Inner = Inner;
					return Group;
				}
			}
			else
			{
				// ) 가 없으면 Arrow 함수가 아님
				// 원래대로 그룹 표현식으로 파싱
				Current = SavedCurrent;
				FExpressionPtr Inner = ParseExpression();
				Consume(ETokenType::RParen, TEXT("Expected ')' after expression."));

				TSharedPtr<FGroupingExpression> Group = MakeShared<FGroupingExpression>();
				Group->Inner = Inner;
				return Group;
			}
		}

		if (Match({ ETokenType::Number, ETokenType::String, ETokenType::True, ETokenType::False, ETokenType::Null }))
		{
			return ParseLiteral(Previous());
		}

		if (Match({ ETokenType::Identifier }))
		{
			const FToken& NameTok = Previous();

			// 함수 호출인지 확인
			if (Match({ ETokenType::LParen }))
			{
				TSharedPtr<FCallExpression> Call = MakeShared<FCallExpression>();
				Call->CalleeName = NameTok.Lexeme;

				if (!Check(ETokenType::RParen))
				{
					do
					{
						Call->Arguments.Add(ParseExpression());
					}
					while (Match({ ETokenType::Comma }));
				}

				Consume(ETokenType::RParen, TEXT("Expected ')' after arguments."));
				return ParsePostfix(Call);
			}
			else
			{
				TSharedPtr<FIdentifierExpression> Ident = MakeShared<FIdentifierExpression>();
				Ident->Name = NameTok.Lexeme;
				return ParsePostfix(Ident);
			}
		}

		// 예상치 못한 토큰
		ReportError(Peek(), TEXT("Unexpected token in expression."));
		return nullptr;
	}

	FExpressionPtr FParser::ParsePostfix(FExpressionPtr Left)
	{
		// 인덱싱 및 멤버 접근 처리
		while (true)
		{
			if (Match({ ETokenType::LBracket }))
			{
				FExpressionPtr Index = ParseExpression();
				Consume(ETokenType::RBracket, TEXT("Expected ']' after index."));

				TSharedPtr<FIndexExpression> IndexExpr = MakeShared<FIndexExpression>();
				IndexExpr->Target = Left;
				IndexExpr->Index = Index;
				Left = IndexExpr;
			}
			else if (Match({ ETokenType::Dot }))
			{
				// 멤버 접근: arr.push_back 또는 console.log
				const FToken& MemberTok = Consume(ETokenType::Identifier, TEXT("Expected member name after '.'"));
				
				TSharedPtr<FMemberAccessExpression> MemberAccess = MakeShared<FMemberAccessExpression>();
				MemberAccess->Target = Left;
				MemberAccess->MemberName = MemberTok.Lexeme;
				Left = MemberAccess;
				
				// 멤버 접근 후 함수 호출이 오는 경우: arr.push_back(value) 또는 console.log(value)
				if (Match({ ETokenType::LParen }))
				{
					TSharedPtr<FCallExpression> Call = MakeShared<FCallExpression>();
					
					// Target이 Identifier인 경우 objectName.memberName 형태로 저장
					// 인터프리터에서 ThisValue 타입에 따라 Array.memberName으로 변환할지 결정
					FString CalleeName;
					if (MemberAccess->Target->Kind == EExpressionKind::Identifier)
					{
						TSharedPtr<FIdentifierExpression> Ident = StaticCastSharedPtr<FIdentifierExpression>(MemberAccess->Target);
						CalleeName = FString::Printf(TEXT("%s.%s"), *Ident->Name, *MemberAccess->MemberName);
					}
					else
					{
						// Identifier가 아닌 경우 (배열 리터럴 등)는 Array.memberName으로 가정
						CalleeName = FString::Printf(TEXT("Array.%s"), *MemberAccess->MemberName);
					}
					
					Call->CalleeName = CalleeName;
					Call->ThisValue = MemberAccess->Target;  // 객체 표현식 저장
					
					if (!Check(ETokenType::RParen))
					{
						do
						{
							Call->Arguments.Add(ParseExpression());
						}
						while (Match({ ETokenType::Comma }));
					}
					
					Consume(ETokenType::RParen, TEXT("Expected ')' after arguments."));
					Left = Call;
				}
			}
			else if (Match({ ETokenType::PlusPlus }))
			{
				// 후위 증가: x++
				TSharedPtr<FPostfixExpression> Postfix = MakeShared<FPostfixExpression>();
				Postfix->Kind = EExpressionKind::PostfixIncrement;
				Postfix->Operand = Left;
				Postfix->bIsIncrement = true;
				Left = Postfix;
			}
			else if (Match({ ETokenType::MinusMinus }))
			{
				// 후위 감소: x--
				TSharedPtr<FPostfixExpression> Postfix = MakeShared<FPostfixExpression>();
				Postfix->Kind = EExpressionKind::PostfixDecrement;
				Postfix->Operand = Left;
				Postfix->bIsIncrement = false;
				Left = Postfix;
			}
			else
			{
				break;
			}
		}

		return Left;
	}

	FExpressionPtr FParser::ParsePrimary()
	{
		// 사용하지 않지만, 필요 시 확장용으로 남겨둠
		return ParseCallOrIdentifier();
	}

	FExpressionPtr FParser::ParseLiteral(const FToken& Token)
	{
		TSharedPtr<FLiteralExpression> Lit = MakeShared<FLiteralExpression>();
		Lit->LiteralToken = Token;
		return Lit;
	}

	FExpressionPtr FParser::ParseArrowFunction(const TArray<FString>& Parameters)
	{
		TSharedPtr<FArrowFunctionExpression> ArrowFunc = MakeShared<FArrowFunctionExpression>();
		ArrowFunc->Parameters = Parameters;

		// 단일 표현식인지 블록인지 확인
		if (Match({ ETokenType::LBrace }))
		{
			// 블록: (x) => { return x + 1; }
			ArrowFunc->BodyBlock = ParseBlockStatement();
		}
		else
		{
			// 단일 표현식: (x) => x + 1
			ArrowFunc->Body = ParseExpression();
		}

		return ArrowFunc;
	}
}

