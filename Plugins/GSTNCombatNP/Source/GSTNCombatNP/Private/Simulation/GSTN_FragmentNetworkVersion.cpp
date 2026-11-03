#include "Simulation/GSTN_FragmentNetworkVersion.h"

#include "Misc/NetworkVersion.h"
#include "Sync/GSTN_SyncFragment.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNFragmentVersion, Log, All);

namespace
{
	FNetworkVersion::FGetLocalNetworkVersionOverride ChainedVersionOverride;
	bool bVersionOverrideInstalled = false;
	bool bCompatibleOverrideInstalled = false;

	uint32 LocalVersionWithFragments()
	{
		const uint32 Base = ChainedVersionOverride.IsBound() ? ChainedVersionOverride.Execute() : FNetworkVersion::GetLocalNetworkVersion(false);
		return FGSTN_FragmentNetworkVersion::Combine(Base, FGSTN_SyncFragmentRegistry::GetSignature());
	}
}

uint32 FGSTN_FragmentNetworkVersion::GetBaseVersion()
{
	if (ChainedVersionOverride.IsBound())
	{
		return ChainedVersionOverride.Execute();
	}
	FNetworkVersion::InvalidateNetworkChecksum();
	const uint32 Base = FNetworkVersion::GetLocalNetworkVersion(false);
	FNetworkVersion::InvalidateNetworkChecksum();
	return Base;
}

uint32 FGSTN_FragmentNetworkVersion::Combine(uint32 BaseVersion, uint32 FragmentSignature)
{
	return FragmentSignature == 0 ? BaseVersion : HashCombine(BaseVersion, FragmentSignature);
}

bool FGSTN_FragmentNetworkVersion::IsCompatible(uint32 LocalVersion, uint32 RemoteVersion, bool bLog)
{
	const bool bCompatible = LocalVersion == RemoteVersion;
	if (!bCompatible && bLog)
	{
		UE_LOG(LogGSTNFragmentVersion, Error, TEXT("Network version mismatch, the connection is refused: local 0x%08x, remote 0x%08x. The version includes ")
			TEXT("this machine's combat sync fragment signature 0x%08x (%s): a peer with a different set of fragment plugins (for example GSTNLink ")
			TEXT("or GSTNDefence enabled on one side only) has a different version. Engine and project versions are part of it too."),
			LocalVersion, RemoteVersion, FGSTN_SyncFragmentRegistry::GetSignature(), *FGSTN_SyncFragmentRegistry::Describe());
	}
	return bCompatible;
}

void FGSTN_FragmentNetworkVersion::Install()
{
	if (!bVersionOverrideInstalled)
	{
		ChainedVersionOverride = FNetworkVersion::GetLocalNetworkVersionOverride;
		FNetworkVersion::GetLocalNetworkVersionOverride.BindStatic(&LocalVersionWithFragments);
		bVersionOverrideInstalled = true;
	}
	if (!bCompatibleOverrideInstalled && !FNetworkVersion::IsNetworkCompatibleOverride.IsBound())
	{
		FNetworkVersion::IsNetworkCompatibleOverride.BindLambda([](uint32 Local, uint32 Remote) { return IsCompatible(Local, Remote); });
		bCompatibleOverrideInstalled = true;
	}
	FNetworkVersion::InvalidateNetworkChecksum();
}

void FGSTN_FragmentNetworkVersion::Uninstall()
{
	if (bVersionOverrideInstalled)
	{
		FNetworkVersion::GetLocalNetworkVersionOverride = ChainedVersionOverride;
		ChainedVersionOverride.Unbind();
		bVersionOverrideInstalled = false;
	}
	if (bCompatibleOverrideInstalled)
	{
		FNetworkVersion::IsNetworkCompatibleOverride.Unbind();
		bCompatibleOverrideInstalled = false;
	}
	FNetworkVersion::InvalidateNetworkChecksum();
}
