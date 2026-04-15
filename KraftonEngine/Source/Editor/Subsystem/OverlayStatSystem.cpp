#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/MemoryStats.h"
#include "Engine/Profiling/Stats.h"
#include <cstdio>

void FOverlayStatSystem::AppendLine(TArray<FOverlayStatLine>& OutLines, float Y, const FString& Text) const
{
	FOverlayStatLine Line;
	Line.Text = Text;
	Line.ScreenPosition = FVector2(Layout.StartX, Y);
	OutLines.push_back(std::move(Line));
}

void FOverlayStatSystem::RecordPickingAttempt(double ElapsedMs)
{
	LastPickingTimeMs = ElapsedMs;
	AccumulatedPickingTimeMs += ElapsedMs;
	++PickingAttemptCount;
}

TArray<FOverlayStatGroup> FOverlayStatSystem::BuildGroups(const UEditorEngine& Editor) const
{
	TArray<FOverlayStatGroup> Groups;

	if (bShowFPS)
	{
		FOverlayStatGroup Group;

		const FTimer* Timer = Editor.GetTimer();
		const float FPS = Timer ? Timer->GetDisplayFPS() : 0.0f;
		const float MS = FPS > 0.0f ? 1000.0f / FPS : 0.0f;
		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "FPS : %.1f (%.2f ms)", FPS, MS);
			Group.Lines.push_back(FString(Buffer));
		}

		Groups.push_back(std::move(Group));
	}

	if (bShowDecal)
	{
		FOverlayStatGroup Group;
      {
			char Buffer[160] = {};
			snprintf(Buffer, sizeof(Buffer), "Decal Actors : %u", FDecalStats::GetDecalActorCount());
			Group.Lines.push_back(FString(Buffer));
		}
		{
			char Buffer[160] = {};
			snprintf(Buffer, sizeof(Buffer), "Rendered Decals : %u", FDecalStats::GetRenderedDecalCount());
			Group.Lines.push_back(FString(Buffer));
		}
		{
			char Buffer[160] = {};
			snprintf(Buffer, sizeof(Buffer), "Affected Objects (sum) : %u", FDecalStats::GetAffectedObjectCount());
			Group.Lines.push_back(FString(Buffer));
		}

		const TArray<FStatEntry>& Entries = FStatManager::Get().GetSnapshot();
		for (const FStatEntry& Entry : Entries)
		{
			if (!Entry.Name)
			{
				continue;
			}

			const FString StatName = Entry.Name;
			if (StatName.find("Decal") == FString::npos && StatName.find("decal") == FString::npos)
			{
				continue;
			}
			if (StatName == "RenderPass::Decal" || StatName == "Renderpass::decal")
			{
				continue;
			}

			char Buffer[196] = {};
			snprintf(Buffer, sizeof(Buffer), "%s : %.3f ms", Entry.Name, Entry.LastTime * 1000.0);
			Group.Lines.push_back(FString(Buffer));
		}

		if (!Group.Lines.empty())
		{
			Groups.push_back(std::move(Group));
		}
	}

	if (bShowPickingTime)
	{
		FOverlayStatGroup Group;

		{
			char Buffer[128] = {};
			const int32 NumAttempts = static_cast<int32>(PickingAttemptCount);
			const double PickingTimeMS = LastPickingTimeMs;
			const double AccumulatedTime = AccumulatedPickingTimeMs;
			snprintf(Buffer, sizeof(Buffer), "Picking Time %.5f ms : Num Attempts %d : Accumulated Time %.5f ms",
				PickingTimeMS, NumAttempts, AccumulatedTime);
			Group.Lines.push_back(FString(Buffer));
		}

		Groups.push_back(std::move(Group));
	}

	if (bShowMemory)
	{
		FOverlayStatGroup Group;

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Object Memory : %.2f KB", MemoryStats::GetTotalAllocationBytes() / 1024.0);
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Object Count  : %u", MemoryStats::GetTotalAllocationCount());
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "PixelShader Memory : %.2f KB", static_cast<double>(MemoryStats::GetPixelShaderMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "VertexShader Memory : %.2f KB", static_cast<double>(MemoryStats::GetVertexShaderMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "VertexBuffer Memory : %.2f KB", static_cast<double>(MemoryStats::GetVertexBufferMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "IndexBuffer Memory : %.2f KB", static_cast<double>(MemoryStats::GetIndexBufferMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "StaticMesh CPU Memory : %.2f KB", static_cast<double>(MemoryStats::GetStaticMeshCPUMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Texture Memory : %.2f KB", static_cast<double>(MemoryStats::GetTextureMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		Groups.push_back(std::move(Group));
	}

	return Groups;
}

void FOverlayStatSystem::BuildLines(const UEditorEngine& Editor, TArray<FOverlayStatLine>& OutLines) const
{
	OutLines.clear();

	uint32 EstimatedLineCount = 0;
	if (bShowFPS)
	{
		++EstimatedLineCount;
	}
	if (bShowPickingTime)
	{
		++EstimatedLineCount;
	}
	if (bShowMemory)
	{
		EstimatedLineCount += 8;
	}
   if (bShowDecal)
	{
		EstimatedLineCount += 8;
	}
	OutLines.reserve(EstimatedLineCount);

	float CurrentY = Layout.StartY;
	if (bShowFPS)
	{
		const FTimer* Timer = Editor.GetTimer();
		const float FPS = Timer ? Timer->GetDisplayFPS() : 0.0f;
		const float MS = FPS > 0.0f ? 1000.0f / FPS : 0.0f;

		char Buffer[128] = {};
		snprintf(Buffer, sizeof(Buffer), "FPS : %.1f (%.2f ms)", FPS, MS);
		CachedFPSLine = Buffer;
		AppendLine(OutLines, CurrentY, CachedFPSLine);
		CurrentY += Layout.LineHeight + Layout.GroupSpacing;
	}

	if (bShowPickingTime)
	{
		char Buffer[160] = {};
		snprintf(Buffer, sizeof(Buffer), "Picking Time %.5f ms : Num Attempts %d : Accumulated Time %.5f ms",
			LastPickingTimeMs,
			static_cast<int32>(PickingAttemptCount),
			AccumulatedPickingTimeMs);
		CachedPickingLine = Buffer;
		AppendLine(OutLines, CurrentY, CachedPickingLine);
		CurrentY += Layout.LineHeight + Layout.GroupSpacing;
	}

	if (bShowMemory)
	{
		char Buffer[128] = {};

		// 액터/오브젝트 단위 추적 (생성·삭제 시 변동)
		snprintf(Buffer, sizeof(Buffer), "Object Memory : %.2f KB", MemoryStats::GetTotalAllocationBytes() / 1024.0);
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		snprintf(Buffer, sizeof(Buffer), "Object Count  : %u", MemoryStats::GetTotalAllocationCount());
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		// GPU/CPU 리소스 단위 추적 (에셋 로드 시 변동)
		constexpr int32 MemoryLineCount = 6;
		const double ValuesKB[MemoryLineCount] = {
			static_cast<double>(MemoryStats::GetPixelShaderMemory() / 1024.0f),
			static_cast<double>(MemoryStats::GetVertexShaderMemory() / 1024.0f),
			static_cast<double>(MemoryStats::GetVertexBufferMemory() / 1024.0f),
			static_cast<double>(MemoryStats::GetIndexBufferMemory() / 1024.0f),
			static_cast<double>(MemoryStats::GetStaticMeshCPUMemory() / 1024.0f),
			static_cast<double>(MemoryStats::GetTextureMemory() / 1024.0f),
		};

		const char* Labels[MemoryLineCount] = {
			"PixelShader Memory",
			"VertexShader Memory",
			"VertexBuffer Memory",
			"IndexBuffer Memory",
			"StaticMesh CPU Memory",
			"Texture Memory",
		};

		for (int32 Index = 0; Index < MemoryLineCount; ++Index)
		{
			snprintf(Buffer, sizeof(Buffer), "%s : %.2f KB", Labels[Index], ValuesKB[Index]);
			AppendLine(OutLines, CurrentY, FString(Buffer));
			CurrentY += Layout.LineHeight;
		}
       CurrentY += Layout.GroupSpacing;
	}

	if (bShowDecal)
	{
      char Buffer[160] = {};
		snprintf(Buffer, sizeof(Buffer), "Decal Actors : %u", FDecalStats::GetDecalActorCount());
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		snprintf(Buffer, sizeof(Buffer), "Rendered Decals : %u", FDecalStats::GetRenderedDecalCount());
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		snprintf(Buffer, sizeof(Buffer), "Affected Objects (sum) : %u", FDecalStats::GetAffectedObjectCount());
		AppendLine(OutLines, CurrentY, FString(Buffer));
		CurrentY += Layout.LineHeight;

		const TArray<FStatEntry>& Entries = FStatManager::Get().GetSnapshot();
		for (const FStatEntry& Entry : Entries)
		{
			if (!Entry.Name)
			{
				continue;
			}

			const FString StatName = Entry.Name;
			if (StatName.find("Decal") == FString::npos && StatName.find("decal") == FString::npos)
			{
				continue;
			}
			if (StatName == "RenderPass::Decal" || StatName == "Renderpass::decal")
			{
				continue;
			}

			char Buffer[196] = {};
			snprintf(Buffer, sizeof(Buffer), "%s : %.3f ms", Entry.Name, Entry.LastTime * 1000.0);
			AppendLine(OutLines, CurrentY, FString(Buffer));
			CurrentY += Layout.LineHeight;
		}
	}
}

TArray<FOverlayStatLine> FOverlayStatSystem::BuildLines(const UEditorEngine& Editor) const
{
	TArray<FOverlayStatLine> Result;
	BuildLines(Editor, Result);
	return Result;
}
