#include "MagicScript/Runtime/MsInterpreter.h"
#include "MagicScript/Logging/MsLogging.h"
#include "MagicScript/Logging/MsLoggingEnum.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogMagicScript, Log, All);

namespace MagicScript
{
	void FInterpreter::ExecuteProgram(const TSharedPtr<FProgram>& Program, const FScriptExecutionContext& Context)
	{
		if (!Program.IsValid())
		{
			return;
		}

		// PreAnalysis 모드: 스냅샷 생성
		TSharedPtr<FEnvironment> Snapshot = nullptr;
		if (Context.Mode == EExecutionMode::PreAnalysis && GlobalEnv.IsValid())
		{
			Snapshot = GlobalEnv->Clone();
		}

		// 프로그램 실행 전 메모리 통계 초기화
		ResetSpaceTracking();
		bAbortExecution = false;

		for (const FStatementPtr& Stmt : Program->Statements)
		{
			if (bAbortExecution)
			{
				break;
			}
			FExecResult Result = ExecuteStatement(Stmt, GlobalEnv, Context);
			if (Result.bHasReturn)
			{
				// 전역 레벨의 return 은 무시하거나 추후 로그로 처리 가능
				break;
			}
		}

		// PreAnalysis 모드: 스냅샷으로 복원
		if (Context.Mode == EExecutionMode::PreAnalysis && Snapshot.IsValid())
		{
			GlobalEnv = Snapshot;
		}
	}

	FInterpreter::FExecResult FInterpreter::ExecuteStatement(const FStatementPtr& Stmt, const TSharedPtr<FEnvironment>& Env, const FScriptExecutionContext& Context)
	{
		if (bAbortExecution)
		{
			return FExecResult();
		}

		if (!Stmt.IsValid())
		{
			return FExecResult();
		}

		ExecutionCount++;
		FExecResult Result;

		switch (Stmt->Kind)
		{
		case EStatementKind::Import:
			// import 문은 호스트 레벨(MagicScriptRunner)에서 처리하므로 여기서는 no-op
			break;
		case EStatementKind::Block:
			Result = ExecuteBlock(StaticCastSharedPtr<FBlockStatement>(Stmt), MakeShared<FEnvironment>(Env), Context);
			break;

		case EStatementKind::VarDecl:
		{
			TSharedPtr<FVarDeclStatement> Var = StaticCastSharedPtr<FVarDeclStatement>(Stmt);
			FValue InitValue = FValue::Null();
			if (Var->Initializer.IsValid())
			{
				InitValue = EvaluateExpression(Var->Initializer, Env, Context);
			}
			Env->Define(Var->Name, InitValue, Var->bIsConst);
			AddSpaceBytes(EstimateValueSizeBytes(InitValue));
			break;
		}

		case EStatementKind::FuncDecl:
		{
			TSharedPtr<FFuncDeclStatement> FuncDecl = StaticCastSharedPtr<FFuncDeclStatement>(Stmt);

			TSharedPtr<FFunctionValue> FuncVal = MakeShared<FFunctionValue>();
			FuncVal->Name = FuncDecl->Name;
			FuncVal->Parameters = FuncDecl->Parameters;
			FuncVal->Body = FuncDecl->Body;
			FuncVal->Closure = Env;

			Env->Define(FuncDecl->Name, FValue::FromFunction(FuncVal), true);
			break;
		}

		case EStatementKind::If:
		{
			TSharedPtr<FIfStatement> IfStmt = StaticCastSharedPtr<FIfStatement>(Stmt);
			FValue CondVal = EvaluateExpression(IfStmt->Condition, Env, Context);
			const bool bCond = (CondVal.Type == EValueType::Bool) ? CondVal.Bool : false;

			if (bCond)
			{
				Result = ExecuteStatement(IfStmt->ThenBranch, Env, Context);
			}
			else if (IfStmt->ElseBranch.IsValid())
			{
				Result = ExecuteStatement(IfStmt->ElseBranch, Env, Context);
			}
			break;
		}

		case EStatementKind::Switch:
		{
			TSharedPtr<FSwitchStatement> SwitchStmt = StaticCastSharedPtr<FSwitchStatement>(Stmt);
			FValue SwitchValue = EvaluateExpression(SwitchStmt->Expression, Env, Context);
			if (bAbortExecution)
			{
				break;
			}

			bool bMatched = false;
			for (const FSwitchCase& Case : SwitchStmt->Cases)
			{
				// default case 처리
				if (!Case.Value.IsValid())
				{
					if (!bMatched)
					{
						// default case 실행
						for (const FStatementPtr& CaseStmt : Case.Statements)
						{
							Result = ExecuteStatement(CaseStmt, Env, Context);
							if (Result.bHasReturn || bAbortExecution)
							{
								return Result;
							}
						}
					}
					break;
				}

				// case 값 비교
				FValue CaseValue = EvaluateExpression(Case.Value, Env, Context);
				if (bAbortExecution)
				{
					break;
				}

				// 값 비교 (타입과 값이 모두 같아야 함)
				bool bEqual = false;
				if (SwitchValue.Type == CaseValue.Type)
				{
					switch (SwitchValue.Type)
					{
					case EValueType::Number:
						bEqual = FMath::IsNearlyEqual(SwitchValue.Number, CaseValue.Number, 0.0001);
						break;
					case EValueType::String:
						bEqual = SwitchValue.String == CaseValue.String;
						break;
					case EValueType::Bool:
						bEqual = SwitchValue.Bool == CaseValue.Bool;
						break;
					case EValueType::Null:
						bEqual = true;
						break;
					default:
						bEqual = false;
						break;
					}
				}

				if (bEqual || bMatched)
				{
					bMatched = true;
					// case 문장들 실행
					for (const FStatementPtr& CaseStmt : Case.Statements)
					{
						Result = ExecuteStatement(CaseStmt, Env, Context);
						if (Result.bHasReturn || bAbortExecution)
						{
							return Result;
						}
					}
				}
			}
			break;
		}

		case EStatementKind::While:
		{
			TSharedPtr<FWhileStatement> WhileStmt = StaticCastSharedPtr<FWhileStatement>(Stmt);
			TSharedPtr<FEnvironment> LoopEnv = MakeShared<FEnvironment>(Env);

			// 과도한 while 루프 방지를 위한 안전 장치 (최대 128번)
			constexpr int32 MaxIterations = 128;
			int32 Iteration = 0;

			while (true)
			{
				if (bAbortExecution)
				{
					break;
				}

				// 반복 횟수 체크 (조건 평가 전에 먼저 체크)
				if (Iteration >= MaxIterations)
				{
					FString ErrorMsg = FString::Printf(
						TEXT("MagicScript Runtime Error: while loop exceeded maximum iterations (%d). Loop execution stopped to prevent infinite loop. "
							 "Current iteration: %d. Please check your loop condition."),
						MaxIterations, Iteration
					);
					AddScriptLog(EScriptLogType::Error, ErrorMsg);
					UE_LOG(LogMagicScript, Error, TEXT("%s"), *ErrorMsg);
					SignalRuntimeError();
					break;
				}

				// condition 평가
				if (WhileStmt->Condition.IsValid())
				{
					FValue CondVal = EvaluateExpression(WhileStmt->Condition, LoopEnv, Context);
					if (bAbortExecution)
					{
						break;
					}
					bool bCond = (CondVal.Type == EValueType::Bool) ? CondVal.Bool : false;
					if (!bCond)
					{
						break;
					}
				}
				else
				{
					// 조건이 없으면 무한 루프가 되므로 즉시 중단
					break;
				}

				// body 실행
				FExecResult BodyRes = ExecuteStatement(WhileStmt->Body, LoopEnv, Context);
				if (bAbortExecution)
				{
					break;
				}
				if (BodyRes.bHasReturn)
				{
					Result = BodyRes;
					return Result;
				}

				Iteration++;
			}

			break;
		}

		case EStatementKind::For:
		{
			TSharedPtr<FForStatement> ForStmt = StaticCastSharedPtr<FForStatement>(Stmt);
			TSharedPtr<FEnvironment> LoopEnv = MakeShared<FEnvironment>(Env);

			// init
			if (ForStmt->Init.IsValid())
			{
				FExecResult InitRes = ExecuteStatement(ForStmt->Init, LoopEnv, Context);
				if (InitRes.bHasReturn)
				{
					Result = InitRes;
					break;
				}
			}

			for (;;)
			{
				// condition
				if (ForStmt->Condition.IsValid())
				{
					FValue CondVal = EvaluateExpression(ForStmt->Condition, LoopEnv, Context);
					bool bCond = (CondVal.Type == EValueType::Bool) ? CondVal.Bool : false;
					if (!bCond)
					{
						break;
					}
				}

				// body
				FExecResult BodyRes = ExecuteStatement(ForStmt->Body, LoopEnv, Context);
				if (BodyRes.bHasReturn)
				{
					Result = BodyRes;
					return Result;
				}

				// increment
				if (ForStmt->Increment.IsValid())
				{
					EvaluateExpression(ForStmt->Increment, LoopEnv, Context);
				}
			}

			break;
		}

		case EStatementKind::Return:
		{
			TSharedPtr<FReturnStatement> RetStmt = StaticCastSharedPtr<FReturnStatement>(Stmt);
			Result.bHasReturn = true;
			if (RetStmt->Value.IsValid())
			{
				Result.ReturnValue = EvaluateExpression(RetStmt->Value, Env, Context);
			}
			else
			{
				Result.ReturnValue = FValue::Null();
			}
			break;
		}

		case EStatementKind::Expr:
		{
			TSharedPtr<FExpressionStatement> ExprStmt = StaticCastSharedPtr<FExpressionStatement>(Stmt);
			EvaluateExpression(ExprStmt->Expr, Env, Context);
			break;
		}

		default:
			break;
		}
		return Result;
	}

	FInterpreter::FExecResult FInterpreter::ExecuteBlock(const TSharedPtr<FBlockStatement>& Block, const TSharedPtr<FEnvironment>& Env, const FScriptExecutionContext& Context)
	{
		if (!Block.IsValid())
		{
			return FExecResult();
		}

		FExecResult Result;
		
		for (int32 i = 0; i < Block->Statements.Num(); ++i)
		{
			Result = ExecuteStatement(Block->Statements[i], Env, Context);
			
			if (Result.bHasReturn || bAbortExecution)
			{
				break;
			}
		}
		return Result;
	}

	FValue FInterpreter::EvaluateExpression(const FExpressionPtr& Expr, const TSharedPtr<FEnvironment>& Env, const FScriptExecutionContext& Context)
	{
		if (bAbortExecution)
		{
			return FValue::Null();
		}

		if (!Expr.IsValid())
		{
			return FValue::Null();
		}

		ExpressionEvaluationCount++;

		switch (Expr->Kind)
		{
		case EExpressionKind::Literal:
		{
			TSharedPtr<FLiteralExpression> Lit = StaticCastSharedPtr<FLiteralExpression>(Expr);
			const FToken& Tok = Lit->LiteralToken;

			switch (Tok.Type)
			{
			case ETokenType::Number:
				return FValue::FromNumber(FCString::Atod(*Tok.Lexeme));
			case ETokenType::String:
				return FValue::FromString(Tok.Lexeme);
			case ETokenType::True:
				return FValue::FromBool(true);
			case ETokenType::False:
				return FValue::FromBool(false);
			case ETokenType::Null:
				return FValue::Null();
			default:
				break;
			}
			break;
		}

		case EExpressionKind::Identifier:
		{
			TSharedPtr<FIdentifierExpression> Ident = StaticCastSharedPtr<FIdentifierExpression>(Expr);
			FEnvironment::FEntry* Entry = Env->Lookup(Ident->Name);
			if (Entry)
			{
				return Entry->Value;
			}
			// 정의되지 않은 변수는 런타임 에러로 처리하고 실행 중단
			AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Undefined identifier '%s'"), *Ident->Name));
			SignalRuntimeError();
			return FValue::Null();
		}

		case EExpressionKind::Binary:
		{
			TSharedPtr<FBinaryExpression> Bin = StaticCastSharedPtr<FBinaryExpression>(Expr);
			FValue L = EvaluateExpression(Bin->Left, Env, Context);

			// 단락 평가
			if (Bin->Op == EBinaryOp::And)
			{
				bool LB = (L.Type == EValueType::Bool) ? L.Bool : false;
				if (!LB)
				{
					return FValue::FromBool(false);
				}
				FValue R = EvaluateExpression(Bin->Right, Env, Context);
				bool RB = (R.Type == EValueType::Bool) ? R.Bool : false;
				return FValue::FromBool(RB);
			}
			if (Bin->Op == EBinaryOp::Or)
			{
				bool LB = (L.Type == EValueType::Bool) ? L.Bool : false;
				if (LB)
				{
					return FValue::FromBool(true);
				}
				FValue R = EvaluateExpression(Bin->Right, Env, Context);
				bool RB = (R.Type == EValueType::Bool) ? R.Bool : false;
				return FValue::FromBool(RB);
			}

			FValue R = EvaluateExpression(Bin->Right, Env, Context);

			auto GetNum = [](const FValue& V) -> double
			{
				return (V.Type == EValueType::Number) ? V.Number : 0.0;
			};

			switch (Bin->Op)
			{
			case EBinaryOp::Add:          return FValue::FromNumber(GetNum(L) + GetNum(R));
			case EBinaryOp::Sub:          return FValue::FromNumber(GetNum(L) - GetNum(R));
			case EBinaryOp::Mul:          return FValue::FromNumber(GetNum(L) * GetNum(R));
			case EBinaryOp::Div:          return FValue::FromNumber(GetNum(L) / GetNum(R));
			case EBinaryOp::Mod:          return FValue::FromNumber(FMath::Fmod(GetNum(L), GetNum(R)));
			case EBinaryOp::Equal:        return FValue::FromBool(L.Type == R.Type && L.Number == R.Number && L.String == R.String && L.Bool == R.Bool);
			case EBinaryOp::NotEqual:     return FValue::FromBool(!(L.Type == R.Type && L.Number == R.Number && L.String == R.String && L.Bool == R.Bool));
			case EBinaryOp::Less:         return FValue::FromBool(GetNum(L) < GetNum(R));
			case EBinaryOp::LessEqual:    return FValue::FromBool(GetNum(L) <= GetNum(R));
			case EBinaryOp::Greater:      return FValue::FromBool(GetNum(L) > GetNum(R));
			case EBinaryOp::GreaterEqual: return FValue::FromBool(GetNum(L) >= GetNum(R));
			default:                      break;
			}
			break;
		}

		case EExpressionKind::Unary:
		{
			TSharedPtr<FUnaryExpression> Un = StaticCastSharedPtr<FUnaryExpression>(Expr);
			switch (Un->Op)
			{
			case EUnaryOp::Negate:
			{
				FValue V = EvaluateExpression(Un->Operand, Env, Context);
				if (V.Type == EValueType::Number)
				{
					return FValue::FromNumber(-V.Number);
				}
				return FValue::FromNumber(0.0);
			}
			case EUnaryOp::Not:
			{
				FValue V = EvaluateExpression(Un->Operand, Env, Context);
				bool B = (V.Type == EValueType::Bool) ? V.Bool : false;
				return FValue::FromBool(!B);
			}
			case EUnaryOp::PreIncrement:
			case EUnaryOp::PreDecrement:
			{
				// 전위 증가/감소: ++x 또는 --x
				// 먼저 증가/감소하고 그 값을 반환
				if (Un->Operand->Kind != EExpressionKind::Identifier)
				{
					AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Pre-increment/decrement can only be applied to identifiers"));
					SignalRuntimeError();
					return FValue::Null();
				}
				
				TSharedPtr<FIdentifierExpression> Ident = StaticCastSharedPtr<FIdentifierExpression>(Un->Operand);
				FEnvironment::FEntry* Entry = Env->Lookup(Ident->Name);
				if (!Entry)
				{
					AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Undefined variable '%s'"), *Ident->Name));
					SignalRuntimeError();
					return FValue::Null();
				}
				
				if (Entry->Value.Type != EValueType::Number)
				{
					AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Pre-increment/decrement can only be applied to numbers")));
					SignalRuntimeError();
					return FValue::Null();
				}
				
				double NewValue = (Un->Op == EUnaryOp::PreIncrement) 
					? Entry->Value.Number + 1.0 
					: Entry->Value.Number - 1.0;
				
				FValue NewVal = FValue::FromNumber(NewValue);
				Env->Assign(Ident->Name, NewVal);
				return NewVal;
			}
			default:
				break;
			}
			break;
		}

		case EExpressionKind::Assignment:
		{
			TSharedPtr<FAssignmentExpression> Asg = StaticCastSharedPtr<FAssignmentExpression>(Expr);
			FValue RightValue = EvaluateExpression(Asg->Value, Env, Context);
			if (bAbortExecution)
			{
				return FValue::Null();
			}
			
			// 객체 멤버 할당 처리: obj.property = value
			if (!Asg->MemberName.IsEmpty())
			{
				FEnvironment::FEntry* Entry = Env->Lookup(Asg->TargetName);
				if (!Entry)
				{
					AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Undefined variable '%s'"), *Asg->TargetName));
					SignalRuntimeError();
					return FValue::Null();
				}
				
				if (Entry->Value.Type != EValueType::Object || !Entry->Value.Object.IsValid())
				{
					AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Cannot assign to member of non-object variable '%s'"), *Asg->TargetName));
					SignalRuntimeError();
					return FValue::Null();
				}
				
				Entry->Value.Object->Add(Asg->MemberName, RightValue);
				return RightValue;
			}
			
			// 배열/객체 인덱싱 할당 처리
			if (Asg->Index.IsValid())
			{
				FEnvironment::FEntry* Entry = Env->Lookup(Asg->TargetName);
				if (!Entry)
				{
					AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Undefined variable '%s'"), *Asg->TargetName));
					SignalRuntimeError();
					return FValue::Null();
				}
				
				FValue IndexValue = EvaluateExpression(Asg->Index, Env, Context);
				if (bAbortExecution)
				{
					return FValue::Null();
				}
				
				// 배열 인덱싱 할당
				if (Entry->Value.Type == EValueType::Array && Entry->Value.Array.IsValid())
				{
					if (IndexValue.Type != EValueType::Number)
					{
						AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array index must be a number"));
						SignalRuntimeError();
						return FValue::Null();
					}
					
					int32 Index = static_cast<int32>(IndexValue.Number);
					if (Index < 0 || Index >= Entry->Value.Array->Num())
					{
						AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Array index out of bounds (index: %d, size: %d)"), Index, Entry->Value.Array->Num()));
						SignalRuntimeError();
						return FValue::Null();
					}
					
					Entry->Value.Array->operator[](Index) = RightValue;
					return RightValue;
				}
				// 객체 인덱싱 할당 (문자열 키로 접근)
				else if (Entry->Value.Type == EValueType::Object && Entry->Value.Object.IsValid())
				{
					if (IndexValue.Type != EValueType::String)
					{
						AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Object index must be a string"));
						SignalRuntimeError();
						return FValue::Null();
					}
					
					Entry->Value.Object->Add(IndexValue.String, RightValue);
					return RightValue;
				}
				else
				{
					AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Cannot index non-array and non-object variable '%s'"), *Asg->TargetName));
					SignalRuntimeError();
					return FValue::Null();
				}
			}
			
			// 복합 할당 연산자의 경우 현재 변수 값을 가져와서 연산 수행
			if (Asg->Op != EAssignmentOp::Assign)
			{
				FEnvironment::FEntry* Entry = Env->Lookup(Asg->TargetName);
				if (!Entry)
				{
					AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Undefined variable '%s'"), *Asg->TargetName));
					SignalRuntimeError();
					return FValue::Null();
				}
				
				FValue LeftValue = Entry->Value;
				FValue Result;
				
				// 숫자 연산만 지원 (현재)
				auto GetNum = [](const FValue& V) -> double
				{
					return (V.Type == EValueType::Number) ? V.Number : 0.0;
				};
				
				switch (Asg->Op)
				{
				case EAssignmentOp::AddAssign:
					Result = FValue::FromNumber(GetNum(LeftValue) + GetNum(RightValue));
					break;
				case EAssignmentOp::SubAssign:
					Result = FValue::FromNumber(GetNum(LeftValue) - GetNum(RightValue));
					break;
				case EAssignmentOp::MulAssign:
					Result = FValue::FromNumber(GetNum(LeftValue) * GetNum(RightValue));
					break;
				case EAssignmentOp::DivAssign:
					{
						double RightNum = GetNum(RightValue);
						if (FMath::IsNearlyZero(RightNum))
						{
							AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Division by zero"));
							SignalRuntimeError();
							return FValue::Null();
						}
						Result = FValue::FromNumber(GetNum(LeftValue) / RightNum);
					}
					break;
				case EAssignmentOp::ModAssign:
					Result = FValue::FromNumber(FMath::Fmod(GetNum(LeftValue), GetNum(RightValue)));
					break;
				default:
					Result = RightValue;
					break;
				}
				
				Env->Assign(Asg->TargetName, Result);
				return Result;
			}
			else
			{
				// 일반 할당
				Env->Assign(Asg->TargetName, RightValue);
				return RightValue;
			}
		}

		case EExpressionKind::Call:
		{
			TSharedPtr<FCallExpression> CallExpr = StaticCastSharedPtr<FCallExpression>(Expr);
			
			// 멤버 메서드 호출인지 확인 (arr.push_back(value) 또는 console.log(value))
			TArray<FValue> Args;
			FString CalleeName = CallExpr->CalleeName;
			
			// 먼저 원래 CalleeName으로 함수를 찾아봄 (console.log 같은 경우)
			FEnvironment::FEntry* Entry = Env->Lookup(CalleeName);
			
			// 함수를 찾지 못했고 ThisValue가 있는 경우, 배열 메서드일 수 있으므로 확인
			if ((!Entry || Entry->Value.Type != EValueType::Function) && CallExpr->ThisValue.IsValid())
			{
				// ThisValue를 평가하여 타입 확인
				FValue ThisVal = EvaluateExpression(CallExpr->ThisValue, Env, Context);
				if (bAbortExecution)
				{
					return FValue::Null();
				}
				
				// 배열인 경우 Array.memberName으로 변환
				if (ThisVal.Type == EValueType::Array)
				{
					// CalleeName이 objectName.memberName 형태인 경우 Array.memberName으로 변환
					int32 DotIndex;
					if (CalleeName.FindChar(TEXT('.'), DotIndex))
					{
						FString MemberName = CalleeName.Mid(DotIndex + 1);
						CalleeName = FString::Printf(TEXT("Array.%s"), *MemberName);
					}
					// 배열을 첫 번째 인자로 추가
					Args.Add(ThisVal);
					// 다시 함수 찾기
					Entry = Env->Lookup(CalleeName);
				}
			}
			// 함수를 찾았거나 배열이 아닌 경우, ThisValue를 인자로 추가하지 않음
			
			// 나머지 인자들 평가
			for (const FExpressionPtr& ArgExpr : CallExpr->Arguments)
			{
				Args.Add(EvaluateExpression(ArgExpr, Env, Context));
				if (bAbortExecution)
				{
					return FValue::Null();
				}
			}
			
			// 최종 함수 찾기 (위에서 찾지 못한 경우)
			if (!Entry || Entry->Value.Type != EValueType::Function)
			{
				Entry = Env->Lookup(CalleeName);
			}
			
			if (!Entry)
			{
				FString TypeName = TEXT("Unknown");
				FString ErrorMsg = FString::Printf(
					TEXT("MagicScript Runtime Error: Undefined function '%s'. "
					     "Make sure the function is defined before calling it, or check for typos in the function name."),
					*CalleeName
				);
				AddScriptLog(EScriptLogType::Error, ErrorMsg);
				UE_LOG(LogMagicScript, Error, TEXT("%s"), *ErrorMsg);
				SignalRuntimeError();
				return FValue::Null();
			}

			if (Entry->Value.Type != EValueType::Function || !Entry->Value.Function.IsValid())
			{
				FString TypeName = TEXT("Unknown");
				switch (Entry->Value.Type)
				{
					case EValueType::Number: TypeName = TEXT("Number"); break;
					case EValueType::String: TypeName = TEXT("String"); break;
					case EValueType::Bool: TypeName = TEXT("Bool"); break;
					case EValueType::Null: TypeName = TEXT("Null"); break;
					case EValueType::Array: TypeName = TEXT("Array"); break;
					case EValueType::Function: TypeName = TEXT("Function (invalid)"); break;
					case EValueType::Object: TypeName = TEXT("Object (invalid)"); break;
				}
				
				FString ErrorMsg = FString::Printf(
					TEXT("MagicScript Runtime Error: '%s' is not a function, it is a %s (type: %d). "
					     "You cannot call a non-function value as a function."),
					*CalleeName, *TypeName, static_cast<int32>(Entry->Value.Type)
				);
				AddScriptLog(EScriptLogType::Error, ErrorMsg);
				UE_LOG(LogMagicScript, Error, TEXT("%s"), *ErrorMsg);
				SignalRuntimeError();
				return FValue::Null();
			}

			return CallFunction(Entry->Value.Function, Args, Context);
		}

		case EExpressionKind::MemberAccess:
		{
			// 객체 멤버 접근: obj.property
			TSharedPtr<FMemberAccessExpression> MemberAccess = StaticCastSharedPtr<FMemberAccessExpression>(Expr);
			FValue TargetValue = EvaluateExpression(MemberAccess->Target, Env, Context);
			if (bAbortExecution)
			{
				return FValue::Null();
			}

			// 객체인 경우 속성 접근
			if (TargetValue.Type == EValueType::Object && TargetValue.Object.IsValid())
			{
				if (const FValue* PropValue = TargetValue.Object->Find(MemberAccess->MemberName))
				{
					return *PropValue;
				}
				// 속성이 없으면 null 반환 (또는 에러)
				AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Property '%s' not found in object"), *MemberAccess->MemberName));
				SignalRuntimeError();
				return FValue::Null();
			}

			// 배열이나 다른 타입의 경우 기존 로직 유지 (함수 호출과 함께 사용)
			// 여기서는 단순 멤버 접근만 처리하므로 에러 반환
			AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Member access without function call or invalid target type"));
			SignalRuntimeError();
			return FValue::Null();
		}

		case EExpressionKind::Grouping:
		{
			TSharedPtr<FGroupingExpression> Group = StaticCastSharedPtr<FGroupingExpression>(Expr);
			return EvaluateExpression(Group->Inner, Env, Context);
		}

		case EExpressionKind::ArrayLiteral:
		{
			TSharedPtr<FArrayLiteralExpression> ArrayLit = StaticCastSharedPtr<FArrayLiteralExpression>(Expr);
			TSharedPtr<TArray<FValue>> Array = MakeShared<TArray<FValue>>();
			
			for (const FExpressionPtr& ElemExpr : ArrayLit->Elements)
			{
				FValue ElemValue = EvaluateExpression(ElemExpr, Env, Context);
				if (bAbortExecution)
				{
					return FValue::Null();
				}
				Array->Add(ElemValue);
				AddSpaceBytes(EstimateValueSizeBytes(ElemValue));
			}
			
			return FValue::FromArray(Array);
		}

		case EExpressionKind::ObjectLiteral:
		{
			TSharedPtr<FObjectLiteralExpression> ObjectLit = StaticCastSharedPtr<FObjectLiteralExpression>(Expr);
			TSharedPtr<TMap<FString, FValue>> Object = MakeShared<TMap<FString, FValue>>();
			
			for (const FObjectProperty& Prop : ObjectLit->Properties)
			{
				FValue PropValue = EvaluateExpression(Prop.Value, Env, Context);
				if (bAbortExecution)
				{
					return FValue::Null();
				}
				Object->Add(Prop.Key, PropValue);
				AddSpaceBytes(EstimateValueSizeBytes(PropValue));
			}
			
			return FValue::FromObject(Object);
		}

		case EExpressionKind::Index:
		{
			TSharedPtr<FIndexExpression> IndexExpr = StaticCastSharedPtr<FIndexExpression>(Expr);
			FValue TargetValue = EvaluateExpression(IndexExpr->Target, Env, Context);
			if (bAbortExecution)
			{
				return FValue::Null();
			}
			
			FValue IndexValue = EvaluateExpression(IndexExpr->Index, Env, Context);
			if (bAbortExecution)
			{
				return FValue::Null();
			}

			// 배열 인덱싱
			if (TargetValue.Type == EValueType::Array && TargetValue.Array.IsValid())
			{
				if (IndexValue.Type != EValueType::Number)
				{
					AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Array index must be a number"));
					SignalRuntimeError();
					return FValue::Null();
				}
				
				int32 Index = static_cast<int32>(IndexValue.Number);
				if (Index < 0 || Index >= TargetValue.Array->Num())
				{
					AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Array index out of bounds (index: %d, size: %d)"), Index, TargetValue.Array->Num()));
					SignalRuntimeError();
					return FValue::Null();
				}
				
				return TargetValue.Array->operator[](Index);
			}
			// 객체 인덱싱 (문자열 키로 접근)
			if (TargetValue.Type == EValueType::Object && TargetValue.Object.IsValid())
			{
				if (IndexValue.Type != EValueType::String)
				{
					AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Object index must be a string"));
					SignalRuntimeError();
					return FValue::Null();
				}
				
				if (const FValue* PropValue = TargetValue.Object->Find(IndexValue.String))
				{
					return *PropValue;
				}
				// 속성이 없으면 null 반환
				AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Property '%s' not found in object"), *IndexValue.String));
				SignalRuntimeError();
				return FValue::Null();
			}
				
			AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Cannot index non-array and non-object value"));
			SignalRuntimeError();
			return FValue::Null();
		}

		case EExpressionKind::ArrowFunction:
		{
			TSharedPtr<FArrowFunctionExpression> ArrowFunc = StaticCastSharedPtr<FArrowFunctionExpression>(Expr);
			
			if (!ArrowFunc.IsValid())
			{
				FString ErrorMsg = TEXT("MagicScript Runtime Error: Invalid arrow function expression");
				AddScriptLog(EScriptLogType::Error, ErrorMsg);
				UE_LOG(LogMagicScript, Error, TEXT("%s"), *ErrorMsg);
				SignalRuntimeError();
				return FValue::Null();
			}
			
			// Arrow 함수를 FFunctionValue로 변환
			TSharedPtr<FFunctionValue> FuncVal = MakeShared<FFunctionValue>();
			FuncVal->Name = TEXT("<anonymous>");
			FuncVal->Parameters = ArrowFunc->Parameters;
			FuncVal->Closure = Env;
			FuncVal->bIsNative = false;
			
			// 단일 표현식인지 블록인지 확인
			if (ArrowFunc->Body.IsValid())
			{
				// 단일 표현식: (x) => x + 1
				// return 문으로 감싸진 블록으로 변환
				TSharedPtr<FBlockStatement> BodyBlock = MakeShared<FBlockStatement>();
				TSharedPtr<FReturnStatement> ReturnStmt = MakeShared<FReturnStatement>();
				ReturnStmt->Value = ArrowFunc->Body;
				BodyBlock->Statements.Add(ReturnStmt);
				FuncVal->Body = BodyBlock;
			}
			else if (ArrowFunc->BodyBlock.IsValid())
			{
				// 블록: (x) => { return x + 1; }
				FuncVal->Body = ArrowFunc->BodyBlock;
			}
			else
			{
				FString ErrorMsg = TEXT("MagicScript Runtime Error: Arrow function has no body (neither expression nor block)");
				AddScriptLog(EScriptLogType::Error, ErrorMsg);
				UE_LOG(LogMagicScript, Error, TEXT("%s"), *ErrorMsg);
				SignalRuntimeError();
				return FValue::Null();
			}
			
			if (!FuncVal->Body.IsValid())
			{
				FString ErrorMsg = TEXT("MagicScript Runtime Error: Failed to create arrow function body");
				AddScriptLog(EScriptLogType::Error, ErrorMsg);
				UE_LOG(LogMagicScript, Error, TEXT("%s"), *ErrorMsg);
				SignalRuntimeError();
				return FValue::Null();
			}
			
			return FValue::FromFunction(FuncVal);
		}

		case EExpressionKind::PostfixIncrement:
		case EExpressionKind::PostfixDecrement:
		{
			// 후위 증가/감소: x++ 또는 x--
			// 먼저 현재 값을 반환하고 나중에 증가/감소
			TSharedPtr<FPostfixExpression> Postfix = StaticCastSharedPtr<FPostfixExpression>(Expr);
			
			if (Postfix->Operand->Kind != EExpressionKind::Identifier)
			{
				AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Post-increment/decrement can only be applied to identifiers"));
				SignalRuntimeError();
				return FValue::Null();
			}
			
			TSharedPtr<FIdentifierExpression> Ident = StaticCastSharedPtr<FIdentifierExpression>(Postfix->Operand);
			FEnvironment::FEntry* Entry = Env->Lookup(Ident->Name);
			if (!Entry)
			{
				AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Undefined variable '%s'"), *Ident->Name));
				SignalRuntimeError();
				return FValue::Null();
			}
			
			if (Entry->Value.Type != EValueType::Number)
			{
				AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Post-increment/decrement can only be applied to numbers")));
				SignalRuntimeError();
				return FValue::Null();
			}
			
			// 현재 값을 저장
			FValue OldValue = Entry->Value;
			
			// 증가/감소 수행
			double NewValue = Postfix->bIsIncrement 
				? Entry->Value.Number + 1.0 
				: Entry->Value.Number - 1.0;
			
			FValue NewVal = FValue::FromNumber(NewValue);
			Env->Assign(Ident->Name, NewVal);
			
			// 이전 값을 반환
			return OldValue;
		}

		default:
			break;
		}

		return FValue::Null();
	}

	FValue FInterpreter::CallFunction(const TSharedPtr<FFunctionValue>& FuncValue, const TArray<FValue>& Args, const FScriptExecutionContext& Context)
	{
		if (!FuncValue.IsValid())
		{
			AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Invalid function value"));
			return FValue::Null();
		}

		// 호출 스택 깊이 체크 (무한 재귀 방지)
		if (CallStackDepth >= MAX_CALL_STACK_DEPTH)
		{
			FString ErrorMsg = FString::Printf(
				TEXT("MagicScript Runtime Error: Call stack overflow! Maximum call stack depth (%d) exceeded in function '%s'. "
				     "Current depth: %d. This usually indicates infinite recursion. Please check your function calls."),
				MAX_CALL_STACK_DEPTH, *FuncValue->Name, CallStackDepth
			);
			AddScriptLog(EScriptLogType::Error, ErrorMsg);
			UE_LOG(LogMagicScript, Error, TEXT("%s"), *ErrorMsg);
			SignalRuntimeError();
			return FValue::Null();
		}

		CallStackDepth++;

		// 이 함수 호출 시 사용하는 메모리(추정치)를 추가
		if (FuncValue->SpaceCostBytes > 0)
		{
			AddSpaceBytes(FuncValue->SpaceCostBytes);
		}

		FunctionCallCount++;

		// 네이티브 함수라면 NativeImpl 호출
		if (FuncValue->bIsNative && FuncValue->NativeImpl)
		{
			const FValue Ret = FuncValue->NativeImpl(Args, Context);

			AddSpaceBytes(FuncValue->SpaceCostBytes);
			AccumulatedTimeComplexityScore += FuncValue->TimeComplexityAdditionalScore;
			
			CallStackDepth--;
			return Ret;
		}

		if (!FuncValue->Body.IsValid())
		{
			AddScriptLog(EScriptLogType::Error, FString::Printf(TEXT("MagicScript Runtime Error: Function '%s' has no body"), *FuncValue->Name));
			if (FuncValue->SpaceCostBytes > 0)
			{
				AddSpaceBytes(-FuncValue->SpaceCostBytes);
			}
			CallStackDepth--;
			return FValue::Null();
		}

		TSharedPtr<FEnvironment> FuncEnv = MakeShared<FEnvironment>(FuncValue->Closure);

		const int32 ParamCount = FuncValue->Parameters.Num();
		for (int32 Index = 0; Index < ParamCount; ++Index)
		{
			const FString& ParamName = FuncValue->Parameters[Index];
			FValue ArgValue = (Args.IsValidIndex(Index)) ? Args[Index] : FValue::Null();
			FuncEnv->Define(ParamName, ArgValue, false);
		}

		// 본문은 블록 문장이라고 가정
		if (!FuncValue->Body.IsValid())
		{
			FString ErrorMsg = FString::Printf(
				TEXT("MagicScript Runtime Error: Function '%s' has no body. This is an internal error - function definitions must have a body."),
				*FuncValue->Name
			);
			AddScriptLog(EScriptLogType::Error, ErrorMsg);
			UE_LOG(LogMagicScript, Error, TEXT("%s"), *ErrorMsg);
			if (FuncValue->SpaceCostBytes > 0)
			{
				AddSpaceBytes(-FuncValue->SpaceCostBytes);
			}
			CallStackDepth--;
			SignalRuntimeError();
			return FValue::Null();
		}
		
		TSharedPtr<FBlockStatement> BodyBlock = StaticCastSharedPtr<FBlockStatement>(FuncValue->Body);
		if (!BodyBlock.IsValid())
		{
			FString ErrorMsg = FString::Printf(
				TEXT("MagicScript Runtime Error: Function '%s' body is not a block statement. This is an internal error - function body must be a block."),
				*FuncValue->Name
			);
			AddScriptLog(EScriptLogType::Error, ErrorMsg);
			UE_LOG(LogMagicScript, Error, TEXT("%s"), *ErrorMsg);
			if (FuncValue->SpaceCostBytes > 0)
			{
				AddSpaceBytes(-FuncValue->SpaceCostBytes);
			}
			CallStackDepth--;
			SignalRuntimeError();
			return FValue::Null();
		}
		
		FExecResult Result = ExecuteBlock(BodyBlock, FuncEnv, Context);
		
		FValue RetValue = FValue::Null();
		if (Result.bHasReturn)
		{
			RetValue = Result.ReturnValue;
		}

		if (FuncValue->SpaceCostBytes > 0)
		{
			AddSpaceBytes(-FuncValue->SpaceCostBytes);
		}
		CallStackDepth--;
		return RetValue;
	}

	FValue FInterpreter::CallFunctionByName(const FString& Name, const TArray<FValue>& Args, const FScriptExecutionContext& Context)
	{
		if (!GlobalEnv.IsValid())
		{
			AddScriptLog(EScriptLogType::Error, TEXT("MagicScript Runtime Error: Global environment is invalid"));
			UE_LOG(LogMagicScript, Error, TEXT("MagicScript Runtime Error: Global environment is invalid"));
			return FValue::Null();
		}

		// PreAnalysis 모드: 스냅샷 생성
		TSharedPtr<FEnvironment> Snapshot = nullptr;
		if (Context.Mode == EExecutionMode::PreAnalysis)
		{
			Snapshot = GlobalEnv->Clone();
		}

		FEnvironment::FEntry* Entry = GlobalEnv->Lookup(Name);
		if (!Entry)
		{
			const FString ErrorMsg = FString::Printf(TEXT("MagicScript Runtime Error: Function '%s' is not defined"), *Name);
			AddScriptLog(EScriptLogType::Error, ErrorMsg);
			UE_LOG(LogMagicScript, Error, TEXT("%s"), *ErrorMsg);
			
			// PreAnalysis 모드: 스냅샷으로 복원
			if (Context.Mode == EExecutionMode::PreAnalysis && Snapshot.IsValid())
			{
				GlobalEnv = Snapshot;
			}
			return FValue::Null();
		}

		if (Entry->Value.Type != EValueType::Function || !Entry->Value.Function.IsValid())
		{
			const FString ErrorMsg = FString::Printf(TEXT("MagicScript Runtime Error: '%s' is not a function (type: %d)"), *Name, static_cast<int32>(Entry->Value.Type));
			AddScriptLog(EScriptLogType::Error, ErrorMsg);
			UE_LOG(LogMagicScript, Error, TEXT("%s"), *ErrorMsg);
			
			// PreAnalysis 모드: 스냅샷으로 복원
			if (Context.Mode == EExecutionMode::PreAnalysis && Snapshot.IsValid())
			{
				GlobalEnv = Snapshot;
			}
			return FValue::Null();
		}

		FValue Result = CallFunction(Entry->Value.Function, Args, Context);

		// PreAnalysis 모드: 스냅샷으로 복원
		if (Context.Mode == EExecutionMode::PreAnalysis && Snapshot.IsValid())
		{
			GlobalEnv = Snapshot;
		}

		return Result;
	}

	void FInterpreter::ResetSpaceTracking()
	{
		CurrentSpaceBytes = 0;
		PeakSpaceBytes = 0;
		CallStackDepth = 0;
		ExecutionCount = 0;
		ExpressionEvaluationCount = 0;
		FunctionCallCount = 0;
		AccumulatedTimeComplexityScore = 0;
	}

	void FInterpreter::AddSpaceBytes(int64 Delta)
	{
		CurrentSpaceBytes += Delta;
		if (CurrentSpaceBytes < 0)
		{
			CurrentSpaceBytes = 0;
		}
		if (CurrentSpaceBytes > PeakSpaceBytes)
		{
			PeakSpaceBytes = CurrentSpaceBytes;
		}
	}

	int32 FInterpreter::EstimateValueSizeBytes(const FValue& V)
	{
		switch (V.Type)
		{
		case EValueType::Number:   return 8;   // double 8 bytes
		case EValueType::Bool:     return 4;   // bool/align
		case EValueType::String:   return 24 + V.String.Len() * 2; // 대략적인 UE FString 비용
		case EValueType::Function: return 64;  // 함수 포인터/클로저 오버헤드 대략치
		case EValueType::Array:
		{
			if (!V.Array.IsValid())
			{
				return 24; // 배열 포인터 오버헤드
			}
			int32 TotalSize = 24; // 배열 자체 오버헤드
			for (const FValue& Elem : *V.Array)
			{
				TotalSize += EstimateValueSizeBytes(Elem);
			}
			return TotalSize;
		}
		case EValueType::Object:
		{
			if (!V.Object.IsValid())
			{
				return 24; // 객체 포인터 오버헤드
			}
			int32 TotalSize = 24; // 객체 자체 오버헤드
			for (const auto& Pair : *V.Object)
			{
				TotalSize += 24 + Pair.Key.Len() * 2; // 키 문자열 비용
				TotalSize += EstimateValueSizeBytes(Pair.Value); // 값 비용
			}
			return TotalSize;
		}
		case EValueType::Null:
		default:
			return 0;
		}
	}

	void FInterpreter::SignalRuntimeError()
	{
		bAbortExecution = true;
	}

}

