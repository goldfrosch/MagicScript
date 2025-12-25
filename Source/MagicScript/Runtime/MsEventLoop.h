#pragma once

#include "CoreMinimal.h"

namespace MagicScript
{
	class FInterpreter;
	struct FValue;
	struct FFunctionValue;

	/** 비동기 작업 타입 */
	enum class EAsyncTaskType : uint8
	{
		SetTimeout
	};

	/** 비동기 작업 */
	struct FAsyncTask
	{
		EAsyncTaskType Type;
		double ScheduledTime;  // 언제 실행할지 (초 단위, FPlatformTime::Seconds())
		TSharedPtr<FFunctionValue> Callback;  // 실행할 함수
		TArray<FValue> Arguments;  // 함수 인자
		int32 TaskId;  // 고유 ID

		FAsyncTask()
			: Type(EAsyncTaskType::SetTimeout)
			, ScheduledTime(0.0)
			, TaskId(0)
		{
		}
	};

	/**
	 * 이벤트 루프
	 * - setTimeout 등의 비동기 작업 처리
	 * - 언리얼의 Tick에서 호출되어야 함
	 */
	class FEventLoop
	{
	public:
		FEventLoop();
		~FEventLoop() = default;

		/** 이벤트 루프 업데이트 (언리얼 Tick에서 호출) */
		void Tick(FInterpreter* Interpreter);

		/** setTimeout 등록 */
		int32 SetTimeout(TSharedPtr<FFunctionValue> Callback, double DelaySeconds, const TArray<FValue>& Args = {});

		/** 현재 대기 중인 작업이 있는지 */
		bool HasPendingTasks() const { return Tasks.Num() > 0; }

		/** 모든 작업 취소 */
		void ClearAllTasks();

	private:
		TArray<FAsyncTask> Tasks;
		int32 NextTaskId = 1;
	};
}

