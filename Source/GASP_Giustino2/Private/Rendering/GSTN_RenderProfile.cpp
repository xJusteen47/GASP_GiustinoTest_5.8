// Switchable rendering profiles (forward-rendering groundwork, 2026-11-20). Reversible at runtime, no project default changes.
// Doc: docs/notes/rendering-profiles.md
//
// gstn.Render.Profile
//   0 = project default (deferred + Lumen, nothing overridden)
//   1 = flicker probe A: Lumen kept, auto exposure frozen      -> if the flicker stops, the cause is eye adaptation
//   2 = no Lumen (still deferred): no dynamic GI, SSR, DFAO     -> if the flicker stops, the cause is Lumen
//   3 = low-end preview: profile 2 + cheaper shadows/fog/resolution (a rough look at a Switch 2-class budget, NOT a perf measure)
// Every value this sets is put back exactly when the profile goes back to 0 (or to another profile).
// r.ForwardShading is read-only at runtime (restart + shader recompile): see Tools/Rendering/Open-Editor-Forward.ps1.
// gstn.Render.Info prints the active renderer and the main lighting settings.

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNRender, Log, All);

namespace GSTNRenderProfile
{
	struct FOverride
	{
		const TCHAR* Name;
		const TCHAR* Value;
	};

	// Freeze eye adaptation (exposure stays at the post-process bias / manual value)
	static const FOverride ExposureFrozen[] = {
		{ TEXT("r.EyeAdaptationQuality"), TEXT("0") },
	};

	// Lumen off while staying deferred. The *.Allow switches beat post-process volumes that force Lumen.
	static const FOverride NoLumen[] = {
		{ TEXT("r.Lumen.DiffuseIndirect.Allow"), TEXT("0") },
		{ TEXT("r.Lumen.Reflections.Allow"), TEXT("0") },
		{ TEXT("r.DynamicGlobalIlluminationMethod"), TEXT("0") },
		{ TEXT("r.ReflectionMethod"), TEXT("2") },
		{ TEXT("r.DistanceFieldAO"), TEXT("1") },
	};

	// Extra savings on top of NoLumen (approximate low-end look)
	static const FOverride LowEnd[] = {
		{ TEXT("r.ScreenPercentage"), TEXT("67") },
		{ TEXT("r.Shadow.CSM.MaxCascades"), TEXT("2") },
		{ TEXT("r.Shadow.MaxResolution"), TEXT("1024") },
		{ TEXT("r.VolumetricFog"), TEXT("0") },
		{ TEXT("r.SSR.Quality"), TEXT("1") },
		{ TEXT("r.DistanceFieldAO"), TEXT("0") },
		{ TEXT("r.AmbientOcclusionLevels"), TEXT("1") },
	};

	static int32 Profile = 0;
	static int32 AppliedProfile = 0;
	/** Original value of every cvar we touched, in the order we touched it */
	static TArray<TPair<FString, FString>> Saved;

	static void SetOne(const TCHAR* Name, const TCHAR* Value)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
		if (!CVar)
		{
			UE_LOG(LogGSTNRender, Warning, TEXT("gstn.Render.Profile: %s not found in this engine version, skipped"), Name);
			return;
		}
		if (!Saved.ContainsByPredicate([Name](const TPair<FString, FString>& P) { return P.Key == Name; }))
		{
			Saved.Emplace(Name, CVar->GetString());
		}
		CVar->Set(Value, ECVF_SetByConsole);
	}

	template <int32 N>
	static void ApplyList(const FOverride (&List)[N])
	{
		for (const FOverride& O : List)
		{
			SetOne(O.Name, O.Value);
		}
	}

	static void RestoreAll()
	{
		// Reverse order so a cvar touched twice ends on its true original
		for (int32 i = Saved.Num() - 1; i >= 0; --i)
		{
			if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Saved[i].Key))
			{
				CVar->Set(*Saved[i].Value, ECVF_SetByConsole);
			}
		}
		Saved.Reset();
	}

	static void OnProfileChanged(IConsoleVariable* /*Var*/)
	{
		const int32 Wanted = FMath::Clamp(Profile, 0, 3);
		if (Wanted == AppliedProfile)
		{
			return;
		}
		RestoreAll();
		switch (Wanted)
		{
		case 1: ApplyList(ExposureFrozen); break;
		case 2: ApplyList(NoLumen); break;
		case 3: ApplyList(NoLumen); ApplyList(LowEnd); break;
		default: break;
		}
		AppliedProfile = Wanted;
		UE_LOG(LogGSTNRender, Display, TEXT("gstn.Render.Profile = %d (%d cvars overridden)"), Wanted, Saved.Num());
	}

	static FAutoConsoleVariableRef CVarProfile(
		TEXT("gstn.Render.Profile"), Profile,
		TEXT("Rendering profile, reversible at runtime. 0 = project default (Lumen). 1 = Lumen + frozen auto exposure (flicker probe). ")
		TEXT("2 = no Lumen, still deferred (flicker probe / forward look preview). 3 = 2 + low-end savings (rough Switch 2-class look). ")
		TEXT("Forward shading itself needs a restart: Tools/Rendering/Open-Editor-Forward.ps1. Doc: docs/notes/rendering-profiles.md"),
		FConsoleVariableDelegate::CreateStatic(&OnProfileChanged));

	static FString ValueOf(const TCHAR* Name)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
		return CVar ? CVar->GetString() : FString(TEXT("(missing)"));
	}

	static void PrintInfo()
	{
		static const TCHAR* const Names[] = {
			TEXT("r.ForwardShading"), TEXT("r.DynamicGlobalIlluminationMethod"), TEXT("r.ReflectionMethod"),
			TEXT("r.Lumen.DiffuseIndirect.Allow"), TEXT("r.Lumen.Reflections.Allow"), TEXT("r.AntiAliasingMethod"),
			TEXT("r.MSAACount"), TEXT("r.EyeAdaptationQuality"), TEXT("r.Shadow.Virtual.Enable"), TEXT("r.ScreenPercentage"),
		};
		UE_LOG(LogGSTNRender, Display, TEXT("gstn.Render.Profile = %d"), AppliedProfile);
		for (const TCHAR* Name : Names)
		{
			UE_LOG(LogGSTNRender, Display, TEXT("  %s = %s"), Name, *ValueOf(Name));
		}
	}

	static FAutoConsoleCommand CmdInfo(
		TEXT("gstn.Render.Info"),
		TEXT("Prints the active renderer (forward/deferred), GI, reflections, AA and the gstn.Render.Profile in use"),
		FConsoleCommandDelegate::CreateStatic(&PrintInfo));
}
