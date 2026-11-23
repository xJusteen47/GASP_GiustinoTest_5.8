# F. throw v1 capture (fthrow-overlord-1 round 2): KIT_Test_Overlord grabs the dummy (Interact on v20) and throws F. (Attack + stick on v65).
# GSTN_LinkSettings properties are not visible to Python, so the capture's F. throw (GA_Overlord_FThrowV1 as the common Forward throw; the test
# kit names no Forward throw of its own) and the neck hold point go in a LOCAL Saved ini (not versioned), removed again after the capture.
# Views: behind (az180), side (az90), game. No hitbox run: a throw has no hitbox (link outcome key), nothing to draw.
$P = 'C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_n216qs'
$C = "$P\Saved\AnimPipeline\captures\fthrow-overlord\v1"
Set-Location $P
Remove-Item Env:PIE_CAPTURE_PRE_PY -ErrorAction SilentlyContinue
Remove-Item Env:PIE_CAPTURE_RUNS -ErrorAction SilentlyContinue
$H = if ($env:FTHROW_HOLD_POINT) { $env:FTHROW_HOLD_POINT -split ',' } else { @('38', '-5', '46') }
$ini = "$P\Saved\Config\WindowsEditor\GSTNLink.ini"
$fb = 'FallbackThrows=(Forward="/Game/GASNPPTest/Combat/Overlord/GA_Overlord_FThrowV1.GA_Overlord_FThrowV1_C",Back="/Script/GSTNLinkNP.GSTN_GA_LinkThrowBack",Up="/Script/GSTNLinkNP.GSTN_GA_LinkThrowUp",Down="/Script/GSTNLinkNP.GSTN_GA_LinkThrowDown",Neutral="/Script/GSTNLinkNP.GSTN_GA_LinkThrowNeutral",CrouchForward="/Script/GSTNLinkNP.GSTN_GA_LinkThrowCrouchForward")'
$hp = "HoldPose=(bHoldByNeck=True,HoldPointCm=(X=$($H[0]),Y=$($H[1]),Z=$($H[2])),ExtraOffsetCm=(X=0.0,Y=0.0,Z=0.0))"
[IO.File]::WriteAllText($ini, "[/Script/GSTNLinkCore.GSTN_LinkSettings]`r`n$fb`r`n$hp`r`n")
$env:PIE_CAPTURE_DIST = '350'; $env:PIE_CAPTURE_VIEWS = 'az90,az180,game'; $env:PIE_CAPTURE_NOVFX = '1'
& "$P\Tools\GASNPPTest\pie_capture_timed.ps1" -Kit KIT_Test_Overlord -Out $C -Sequences fthrow_hit -Assert "$P\Tools\AnimPipeline\cards\overlord\fthrow\cap\assert_v1.json"
$rc = $LASTEXITCODE
Remove-Item $ini -ErrorAction SilentlyContinue
"exit $rc"
Get-Content "$C\assert.txt" -TotalCount 1 -ErrorAction SilentlyContinue
