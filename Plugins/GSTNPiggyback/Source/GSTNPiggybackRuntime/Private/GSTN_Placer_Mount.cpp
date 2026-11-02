// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_Placer_Mount.h"

#include "GSTN_MountUtils.h"

FGSTN_ExitResult UGSTN_Placer_Mount::Place_Implementation(const FGSTN_ExitQuery& Query) const
{
	const TArray<FTransform> Candidates = UGSTN_MountUtils::ExitCandidates(Query.SocketTransform, Query.PartnerTransform, Mode, DistanceCm);
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FTransform& Candidate = Candidates[Index];
		if (IsFree(Query, Candidate.GetLocation(), Candidate.GetRotation()))
		{
			FGSTN_ExitResult Result;
			Result.Transform = Candidate;
			Result.Velocity = FVector::ZeroVector;
			Result.bValid = true;
			Result.CandidateIndex = Index;
			return Result;
		}
	}
	return MakeFallback(Query, Query.CapsuleHalfHeight * 2.0);
}
