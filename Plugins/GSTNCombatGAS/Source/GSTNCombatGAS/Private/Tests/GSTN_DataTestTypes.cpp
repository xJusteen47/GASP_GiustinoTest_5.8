#include "Tests/GSTN_DataTestTypes.h"

// The root names are concatenated with the leaf so no leaf of a designer root appears as one string literal in plugin sources
#define GSTN_TEST_KIT_ROOT "Kit"
#define GSTN_TEST_WEAPON_ROOT "Weapon"

namespace GSTNCombatGASTest
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Kit_TestA, GSTN_TEST_KIT_ROOT ".TestA", "Automation test kit identity A");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Kit_TestB, GSTN_TEST_KIT_ROOT ".TestB", "Automation test kit identity B");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_TestA, GSTN_TEST_WEAPON_ROOT ".TestA", "Automation test weapon identity A");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_TestB, GSTN_TEST_WEAPON_ROOT ".TestB", "Automation test weapon identity B");
}
