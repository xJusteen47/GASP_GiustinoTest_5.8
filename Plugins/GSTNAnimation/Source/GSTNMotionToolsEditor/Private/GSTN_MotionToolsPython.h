#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Calls gstn_motion_tools.panel.run() in the editor's Python. Request and reply are JSON objects carried as base64
// so no quoting crosses the C++/Python boundary. Never returns null: on any failure the reply is
// {"ok": false, "error": "..."}.
namespace GSTNMotionToolsPython
{
	TSharedRef<FJsonObject> Call(const TSharedRef<FJsonObject>& Request);
}
