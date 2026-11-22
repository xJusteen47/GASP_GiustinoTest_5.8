// Links, Network Prediction half: shared helpers of the NP automation tests.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Serialization/MemoryWriter.h"
#include "Sim/GSTN_LinkSnapshotContributor.h"

namespace GSTNLinkNPTest
{
	UE_DEFINE_GAMEPLAY_TAG(TestGrab, "Combat.Link.Type.GSTNTestOnlyNPGrab");
	UE_DEFINE_GAMEPLAY_TAG(TestGrabber, "Combat.Link.Role.GSTNTestOnlyNPGrabber");
	UE_DEFINE_GAMEPLAY_TAG(TestGrabbed, "Combat.Link.Role.GSTNTestOnlyNPGrabbed");
	UE_DEFINE_GAMEPLAY_TAG(TestLatch, "Combat.Link.Type.GSTNTestOnlyNPLatch");
	UE_DEFINE_GAMEPLAY_TAG(TestLatcher, "Combat.Link.Role.GSTNTestOnlyNPLatcher");
	UE_DEFINE_GAMEPLAY_TAG(TestLatched, "Combat.Link.Role.GSTNTestOnlyNPLatched");

	FScopedLinkTypes::FScopedLinkTypes()
	{
		Grab = NewObject<UGSTN_LinkTypeDefinition>(GetTransientPackage());
		Grab->AddToRoot();
		Grab->LinkType = TestGrab;
		Grab->InitiatorRole.RoleTag = TestGrabber;
		Grab->InitiatorRole.bActsWhileLinked = true;
		Grab->TargetRole.RoleTag = TestGrabbed;
		Grab->TargetRole.bParked = true;
		Grab->TargetRole.SocketOnPartner = HandSocket();
		Grab->TargetRole.SocketOffset = FTransform(FVector(30.0, 0.0, 0.0));

		Latch = NewObject<UGSTN_LinkTypeDefinition>(GetTransientPackage());
		Latch->AddToRoot();
		Latch->LinkType = TestLatch;
		Latch->InitiatorRole.RoleTag = TestLatcher;
		Latch->InitiatorRole.bParked = true;
		Latch->InitiatorRole.bActsWhileLinked = true;
		Latch->InitiatorRole.SocketOnPartner = SpineSocket();
		Latch->InitiatorRole.SocketOffset = FTransform(FVector(0.0, -40.0, 20.0));
		Latch->TargetRole.RoleTag = TestLatched;
		Latch->TargetRole.bActsWhileLinked = true;

		UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
		Registry->AddDefinition(Grab);
		Registry->AddDefinition(Latch);
		GrabIndex = Registry->GetTypeIndex(TestGrab);
		LatchIndex = Registry->GetTypeIndex(TestLatch);
	}

	FScopedLinkTypes::~FScopedLinkTypes()
	{
		UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
		Registry->RemoveDefinition(Grab);
		Registry->RemoveDefinition(Latch);
		Grab->RemoveFromRoot();
		Latch->RemoveFromRoot();
	}

	FGSTN_LinkFragment MakeLinked(uint8 TypeIndex, uint8 Role, int32 PartnerId, uint8 Serial, int32 LinkFrame)
	{
		FGSTN_LinkFragment Fragment;
		FGSTN_LinkState& Body = Fragment.Links.Body;
		Body.LinkTypeIndex = TypeIndex;
		Body.Role = Role;
		Body.Phase = EGSTN_LinkPhase::Active;
		Body.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Fighter, static_cast<uint16>(PartnerId));
		Body.Serial = Serial;
		Body.LinkFrame = LinkFrame;
		return Fragment;
	}

	FGSTN_SnapshotEntry MakeEntry(int32 EntityId, const FTransform& Transform, uint16 BodyIndex, const FGSTN_LinkFragment* Fragment)
	{
		FGSTN_SnapshotEntry Entry;
		Entry.EntityId = EntityId;
		Entry.Transform = Transform;
		Entry.Facing = Transform.GetRotation().GetForwardVector();
		FGSTN_LinkSnapshotExt Ext;
		Ext.BodyIndex = BodyIndex;
		if (Fragment)
		{
			Ext.SetFromFragment(*Fragment);
			const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
			Ext.Bits = FGSTN_LinkSnapshotContributor::ComputeBits(Registry->GetDefinition(Ext.BodyTypeIndex), Ext.BodyRole, static_cast<EGSTN_LinkPhase>(Ext.BodyPhase));
		}
		Entry.Extension.Set(GSTNFeatureIds::Link, Ext);
		return Entry;
	}

	int64 BitsOfList(FGSTN_SyncFragmentArray& Fragments)
	{
		FBitWriter Writer(1024, true);
		FGSTN_SyncFragmentList::NetSerialize(Writer, Fragments);
		return Writer.GetNumBits();
	}

	FGSTN_SyncFragmentArray ReadBack(FGSTN_SyncFragmentArray& Fragments, bool& bOutError)
	{
		FBitWriter Writer(1024, true);
		FGSTN_SyncFragmentList::NetSerialize(Writer, Fragments);
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_SyncFragmentArray Loaded;
		FGSTN_SyncFragmentList::NetSerialize(Reader, Loaded);
		bOutError = Reader.IsError() || !Reader.AtEnd();
		return Loaded;
	}

	int32 PayloadBytes(const FGSTN_LinkFragment& Fragment)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		const_cast<FGSTN_LinkFragment&>(Fragment).NetSerialize(Writer);
		return Bytes.Num();
	}

	FGSTN_SyncFragmentArray ListOf(const FGSTN_LinkFragment& Fragment)
	{
		FGSTN_SyncFragmentArray List;
		FGSTN_SyncFragmentList::FindOrAdd<FGSTN_LinkFragment>(List) = Fragment;
		return List;
	}
}

#endif
