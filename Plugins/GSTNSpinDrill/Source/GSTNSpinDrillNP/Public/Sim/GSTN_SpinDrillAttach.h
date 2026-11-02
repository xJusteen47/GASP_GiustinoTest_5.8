// Spin and drill, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Sim/GSTN_DrillTickExtension.h"

class AActor;

/**
 * Which spin and drill tuning an entity plays with (plan D4): the content side of the feature, kept apart from the simulation state, the
 * same idea as the defence attach. Every machine resolves it the same way from the kit's FGSTN_SpinDrillKitExtension (the GAS module's
 * loadout extension calls Apply wherever the loadout is compiled, host and clients alike, so there is no OnRep and no IsServer branch). The
 * tick extension, the hop and the low gravity read it as they read any asset; an actor without an entry plays with the defaults of the
 * tuning structs, so a kit that binds the spin without an extension still works.
 *
 * Values only (no object reference), so nothing here has to be kept alive. Game thread only.
 */
class GSTNSPINDRILLNP_API FGSTN_SpinDrillAttach
{
public:
	/** The tuning attached to Owner, null when none (the caller then uses FGSTN_DrillSimTuning::MakeDefault) */
	static const FGSTN_DrillSimTuning* Find(const AActor* Owner);

	/** The attached tuning, or the defaults */
	static FGSTN_DrillSimTuning FindOrDefault(const AActor* Owner);

	/** Attaches Tuning to Owner on this machine, replacing what was there */
	static void Apply(const AActor* Owner, const FGSTN_DrillSimTuning& Tuning);

	static void Clear(const AActor* Owner);

	/** Number of attached entities (tests) */
	static int32 Num();

	/** Drops every attachment (tests only) */
	static void Reset();
};
