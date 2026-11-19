// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

NETWORKPREDICTION_API DECLARE_LOG_CATEGORY_EXTERN(LogNetworkPrediction, Log, All);

#include "HAL/PlatformTime.h"

// GSTN: rate-limited LogNetworkPrediction line for warnings that can fire every frame or every
// packet (starved interpolation, discarded delta updates, offset changes). With several PIE
// clients in one editor process the unthrottled lines flood the Output Log, which costs frame
// time on its own. Logs at most once per IntervalSeconds per call site and reports how many
// lines were folded since the last one.
#define UE_NP_LOG_THROTTLED(Verbosity, IntervalSeconds, Format, ...) \
	{ \
		static double NpThrottleLastLogTime = -1.0e9; \
		static int32 NpThrottleSuppressed = 0; \
		const double NpThrottleNow = FPlatformTime::Seconds(); \
		if (NpThrottleNow - NpThrottleLastLogTime >= (IntervalSeconds)) \
		{ \
			UE_LOG(LogNetworkPrediction, Verbosity, Format TEXT(" [+%d similar in the last %.0fs]"), ##__VA_ARGS__, NpThrottleSuppressed, (double)(IntervalSeconds)); \
			NpThrottleLastLogTime = NpThrottleNow; \
			NpThrottleSuppressed = 0; \
		} \
		else \
		{ \
			++NpThrottleSuppressed; \
		} \
	}
