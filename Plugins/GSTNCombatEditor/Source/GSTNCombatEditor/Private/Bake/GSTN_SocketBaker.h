// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"

class FGSTN_PreviewPose;
class UAnimMontage;
class USkeletalMesh;

/** One BakedSocket hitbox that asks for a socket: where it is in the timeline, so a problem can name it */
struct FGSTN_BakeSocketUse
{
	FName Socket;
	int32 TrackIndex = INDEX_NONE;
	int32 HitboxIndex = INDEX_NONE;
};

/** Why a bake did not happen. Names the hitbox when the problem is about one */
struct FGSTN_BakeProblem
{
	FText Message;
	int32 TrackIndex = INDEX_NONE;
	int32 HitboxIndex = INDEX_NONE;
	FName Socket;
};

/** Everything outside the timeline data that the frames depend on */
struct FGSTN_BakeInputs
{
	/** The montage the timeline plays, already loaded. Null: no montage, the reference pose is baked */
	UAnimMontage* Montage = nullptr;

	/** The mesh the sockets are read from */
	USkeletalMesh* Mesh = nullptr;

	/** Frames per second of the simulation tick (UGSTN_CombatEditorSettings::GetSimulationTickRate) */
	float TickRateHz = 62.5f;
};

/** The product of a bake. Nothing is written to the timeline until Apply: a failed bake changes nothing */
struct FGSTN_BakeResult
{
	/** True when the frames in Sockets are complete (an empty set of sockets is a complete bake of nothing) */
	bool bBaked = false;

	/** Socket name -> its root-space transform for each timeline frame */
	TMap<FName, FGSTN_BakedSocketFrames> Sockets;

	/** What goes to FGSTN_CombatTimelineData::BakeInfo */
	FGSTN_BakeInfo Info;

	TArray<FGSTN_BakeProblem> Problems;

	/** Things worth knowing that do not stop the bake (no montage: the reference pose was baked) */
	TArray<FText> Warnings;

	int32 FrameCount = 0;
	double ElapsedMs = 0.0;

	/** All the problems as text, one per line */
	FText DescribeProblems() const;
};

/** Is the bake of a timeline still the bake of its inputs? */
enum class EGSTN_BakeStatus : uint8
{
	/** No BakedSocket hitbox: nothing to bake */
	NotNeeded,
	/** A hitbox needs a socket and there are no frames for it */
	NeverBaked,
	/** Frames exist but the editor did not write them (no BakeInfo): an outside tool did, and nothing says whether they are current */
	BakedElsewhere,
	/** The frames were baked from the inputs as they are now */
	Fresh,
	/** An input changed since the bake, or the frames no longer fit the timeline */
	Stale,
	/** The check could not be made (no mesh to read the inputs from) */
	Unknown
};

struct FGSTN_BakeStatusResult
{
	EGSTN_BakeStatus Status = EGSTN_BakeStatus::Unknown;

	/** In words, for the tooltip of the Bake button and the validator */
	FText Reason;

	/** The toolbar shows its dot and the validator reports: the frames are missing or no longer match */
	bool NeedsBake() const { return Status == EGSTN_BakeStatus::NeverBaked || Status == EGSTN_BakeStatus::Stale; }
};

/**
 * Socket baking (combat-m5-editor 7). A BakedSocket hitbox lives in the space of a bone or socket of the attacker's mesh, and at run time the
 * simulation cannot ask an animation graph where that is: so the editor samples the montage once per timeline frame, here, and the timeline
 * stores the root-relative transform of each socket per frame (FGSTN_CombatTimelineData::BakedSockets). The runtime only indexes that array.
 *
 * The sampling is the preview's: the same FGSTN_PreviewPose that draws the mesh at a frame gives the socket, so what is baked is what is seen.
 * Frame f is sampled at the START of the frame, montage time SectionStart + f / TickRate x PlayRate, which is where the synced montage of the
 * simulation is when it runs frame f (it starts at the section start and advances one tick step per frame at the play rate). Frames are
 * independent: the montage is set to the time, nothing carries over, so a bake is deterministic and two bakes of the same inputs are
 * bit-identical.
 */
class FGSTN_SocketBaker
{
public:
	/** Every BakedSocket hitbox that names a socket, in track order. A BakedSocket hitbox with no socket name is the validator's error and is not listed */
	static void CollectUses(const FGSTN_CombatTimelineData& Data, TArray<FGSTN_BakeSocketUse>& OutUses);

	/** The distinct socket names of the uses, sorted by name (the order the hash and the result use) */
	static TArray<FName> GetDistinctSockets(TConstArrayView<FGSTN_BakeSocketUse> Uses);

	/** Is it a bone or a socket of the mesh (or of its skeleton)? */
	static bool MeshHasSocket(const USkeletalMesh& Mesh, FName SocketOrBone);

	/**
	 * Bakes with a pose that is already bound to Inputs.Mesh (the open editor's preview, so nothing is built twice). The pose's montage, section and
	 * rate are set here from Data and Inputs and are the same values the preview controller uses; the caller refreshes the preview afterwards, since
	 * the baker moves the pose. Never writes Data.
	 */
	static FGSTN_BakeResult Bake(const FGSTN_CombatTimelineData& Data, FGSTN_PreviewPose& Pose, const FGSTN_BakeInputs& Inputs);

	/**
	 * Bakes with a scene of its own for the mesh: the path of Bake all and of the Blueprint-callable utilities, where no editor is open. Loads the
	 * timeline's montage itself. The mesh is placed in root space exactly as the preview scene places it.
	 */
	static FGSTN_BakeResult BakeStandalone(const FGSTN_CombatTimelineData& Data, USkeletalMesh* Mesh, float TickRateHz);

	/** Puts a finished bake into the data: the frames replace BakedSockets, the info replaces BakeInfo. Does nothing for a result that did not bake */
	static bool Apply(FGSTN_CombatTimelineData& Data, const FGSTN_BakeResult& Result);
};

/** The inputs of a bake as a number, and the staleness check built on it (combat-m5-editor 7.3) */
namespace GSTNBake
{
	/**
	 * Hash of everything the baked frames depend on: the data of the animation sequences the section plays (their data model GUIDs and the
	 * segment timing in the montage), the section name and start, the play rate, TotalFrames, the tick rate, the mesh path and its skeleton GUID,
	 * the sorted socket list and what the mesh says about each socket (the bone and the local transform of a mesh socket). Quantized, built from
	 * a canonical text and CRC'd, so the same inputs give the same number on any machine. Montage and mesh may be null: they hash as absent.
	 */
	uint32 ComputeInputHash(const FGSTN_CombatTimelineData& Data, const FGSTN_BakeInputs& Inputs);

	/** The text the hash is made of, one input per line. For a test, and for the day someone has to ask why a bake went stale */
	FString DescribeInputs(const FGSTN_CombatTimelineData& Data, const FGSTN_BakeInputs& Inputs);

	/** Where the bake stands against the inputs as they are now. Reads GUIDs and paths only, no sampling: cheap enough for a toolbar refresh */
	FGSTN_BakeStatusResult GetStatus(const FGSTN_CombatTimelineData& Data, const FGSTN_BakeInputs& Inputs);
}
