// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"

/**
 * The hitboxes of the timeline in the preview viewport (combat-m5-editor 5.4 overlays, "Show Hitboxes"): every hitbox whose track window holds the
 * playhead, as a wireframe coloured by its HitGroup, at the place the hit code puts it (keys evaluated by UGSTN_HitShapeUtils::EvaluateKeys, space
 * resolved by GSTNGizmo::SpaceTransform), the hitbox of the selected key drawn thicker. Each shape is drawn with an HGSTN_HitboxProxy so a click
 * on its lines selects it. It registers itself in FGSTN_PreviewOverlayRegistry; the viewports with no timeline model (the hurtbox set editor) skip it.
 */
namespace GSTNHitboxOverlay
{
	void Register();
	void Unregister();
}
