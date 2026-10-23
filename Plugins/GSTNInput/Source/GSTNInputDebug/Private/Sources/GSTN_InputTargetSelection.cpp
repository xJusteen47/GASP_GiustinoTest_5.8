// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#include "Sources/GSTN_InputTargetSelection.h"

FString FGSTN_InputTargetSelection::Choose(TConstArrayView<FGSTN_InputTargetCandidate> Candidates)
{
	if (Candidates.IsEmpty())
	{
		return FString();
	}

	if (!bFollowFocus)
	{
		for (const FGSTN_InputTargetCandidate& Candidate : Candidates)
		{
			if (Candidate.Key == FixedKey)
			{
				return Candidate.Key;
			}
		}
		// The picked player is not in a session right now: show what following the focus would, and keep the pick for when it is back
	}

	// The focused window wins and is remembered for the moments the focus is elsewhere
	for (const FGSTN_InputTargetCandidate& Candidate : Candidates)
	{
		if (Candidate.bFocused)
		{
			LastFocusedKey = Candidate.Key;
			return Candidate.Key;
		}
	}
	for (const FGSTN_InputTargetCandidate& Candidate : Candidates)
	{
		if (Candidate.Key == LastFocusedKey)
		{
			return Candidate.Key;
		}
	}

	// Nothing had the focus yet: a player with something to read, else the first
	for (const FGSTN_InputTargetCandidate& Candidate : Candidates)
	{
		if (Candidate.bHasSource)
		{
			return Candidate.Key;
		}
	}
	return Candidates[0].Key;
}
