// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class UGSTN_PrimaryKitDefinition;
class USkeletalMesh;
struct FGSTN_CompileInput;
struct FGSTN_LinkKitExtension;
struct FGSTN_LoadoutExtensionContext;

/**
 * The link content of one combat entity (FGSTN_LinkAttach): never state, resolved the same way on every machine from the same assets.
 *
 *  - The BAKED sockets: the root-relative transform of every socket a registered link role names as its SocketOnPartner
 *    (FGSTN_LinkRoleDef), taken from the entity's skeletal mesh REFERENCE POSE and the mesh's offset from the actor root at attach time.
 *    A parked partner is placed on these (m6 spec 7.3: hit queries of a linked entity use the baked socket, never the live mesh), so the
 *    hurtboxes of a grabbed entity are the same on the host and the clients whatever the animation shows. A name the mesh does not have
 *    (or an entity without a mesh) reads as the root.
 *  - The kit's link extension (grab and latch rules, anims by role), or null for a kit without one: the LinkRequest step hands it to the
 *    rules as the initiator's kit rules.
 */
struct GSTNLINKNP_API FGSTN_LinkBodyData
{
	/** Index in this machine's append-only table (what the snapshot carries; never on the wire). 0 = none */
	uint16 Index = 0;

	/** Socket name -> root-relative transform, from the reference pose */
	TMap<FName, FTransform> BakedSockets;

	/**
	 * The kit the loadout attached, held weakly, and its link extension looked up on every read (GetKitExtension): a raw pointer into the kit's
	 * extension list would dangle as soon as the list is edited in memory (the harness adds and removes kit content between runs; found in PIE)
	 */
	TWeakObjectPtr<const UGSTN_PrimaryKitDefinition> Kit;

	/** A test's own extension, used when no kit is attached (AddForTest) */
	const FGSTN_LinkKitExtension* TestKitExtension = nullptr;

	/** The kit's link extension, or the test's, or null */
	const FGSTN_LinkKitExtension* GetKitExtension() const;

	/** The socket's root-relative transform; identity for NAME_None and for a name that was not baked */
	FTransform GetSocket(FName Socket) const;
};

/**
 * Which link content each entity plays with (m6 spec 7.3, 7.0): the body table of the snapshot contributor and the steps. The pattern of
 * FGSTN_DefenceAttach: one Apply for every machine, called by the loadout extension wherever the loadout is compiled (host and clients
 * alike, so no OnRep and no IsServer branch), and by hand by the harness. Unlike the defence, EVERY combat entity is attached whether its kit
 * carries a link extension or not, because any entity may be grabbed: the body sockets are the partner's, not the kit's.
 *
 * The table is append-only for the life of the process: the snapshot and the lag compensation history carry a body INDEX, and an index must
 * never change meaning while a record that holds it can still be rewound. Indices are machine-local (they never travel). Game thread only.
 */
class GSTNLINKNP_API FGSTN_LinkAttach
{
public:
	/** The data attached to Owner, or null */
	static const FGSTN_LinkBodyData* Find(const AActor* Owner);

	/** The data at a table index, null for 0 or an unknown index */
	static const FGSTN_LinkBodyData* At(uint16 Index);

	/**
	 * Attaches Owner's link content on this machine: bakes the sockets of every role the link registry knows from Owner's skeletal mesh (the
	 * first skeletal mesh component; none = every socket is the root) and records the kit's link extension. Re-attaching appends a new entry
	 * and points Owner at it. Returns the index
	 */
	static uint16 Apply(AActor* Owner, const UGSTN_PrimaryKitDefinition* Kit);

	/** Bakes the named sockets of Mesh: reference pose of the socket's bone, the socket's local offset, then MeshRelative (mesh to actor root) */
	static void BakeSockets(const USkeletalMesh* Mesh, const FTransform& MeshRelative, TConstArrayView<FName> Sockets, TMap<FName, FTransform>& Out);

	static void Clear(const AActor* Owner);

	/** The loadout extension this module registers (id GSTNLinkNP): attaches the owner of every compiled loadout. Nothing is added to the compile input */
	static void AttachFromLoadout(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input);
	static FName GetLoadoutExtensionId();

	/** Number of attached entities (tests) */
	static int32 Num();

	/** Drops every attachment and the table (tests only: a running simulation holds indices into it) */
	static void Reset();

	/** Adds a body entry that no actor owns (tests): the sockets and kit given. Returns its index */
	static uint16 AddForTest(const TMap<FName, FTransform>& Sockets, const FGSTN_LinkKitExtension* KitExtension = nullptr);
};
