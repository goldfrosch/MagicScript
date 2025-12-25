#include "MagicScript/Runtime/MsEventLoop.h"
#include "MagicScript/Runtime/MsInterpreter.h"
#include "MagicScript/Core/MsValue.h"

namespace MagicScript
{
	FEventLoop::FEventLoop()
	{
	}

	void FEventLoop::Tick(FInterpreter* Interpreter)
	{
		if (!Interpreter)
		{
			return;
		}

		const double CurrentTime = FPlatformTime::Seconds();

		// setTimeout 작업 처리
		for (int32 i = Tasks.Num() - 1; i >= 0; --i)
		{
			FAsyncTask& Task = Tasks[i];
			if (CurrentTime >= Task.ScheduledTime)
			{
				// 작업 실행
				if (Task.Callback.IsValid())
				{
					Interpreter->CallFunction(Task.Callback, Task.Arguments, FScriptExecutionContext());
				}
				Tasks.RemoveAt(i);
			}
		}
	}

	int32 FEventLoop::SetTimeout(TSharedPtr<FFunctionValue> Callback, double DelaySeconds, const TArray<FValue>& Args)
	{
		FAsyncTask Task;
		Task.Type = EAsyncTaskType::SetTimeout;
		Task.ScheduledTime = FPlatformTime::Seconds() + DelaySeconds;
		Task.Callback = Callback;
		Task.Arguments = Args;
		Task.TaskId = NextTaskId++;

		Tasks.Add(Task);
		return Task.TaskId;
	}

	void FEventLoop::ClearAllTasks()
	{
		Tasks.Empty();
		NextTaskId = 1;
	}
}

