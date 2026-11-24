# F. throw v1 capture (round 2, fixed 2026-11-24): the grabber grabs the dummy (Interact on v20) and throws F. (Attack + stick on v65).
# GSTN_LinkSettings properties are not visible to Python, so the capture's F. throw (GA_Overlord_FThrowV1 as the common Forward throw; the test
# kit names no Forward throw of its own) and the neck hold point go in a LOCAL Saved ini (not versioned), removed again after the capture.
# Views: behind (az180), side (az90), game. No hitbox run: a throw has no hitbox (link outcome key), nothing to draw.
# fthrow-capture-fix (2026-11-24): round 2 grabbed nothing (KIT_Test_Overlord has no link slots, Attack played the jab). Now the grab setup of the
# network link tests: kit KIT_Test_Link on the grabber and the dummy, link steps and the kit's Throws.Forward = GA_Overlord_FThrowV1 IN MEMORY
# (fthrow_capture_pre.py, before Play), hold point Z 36 (pawn z 100 in the capture, neck at 136). capture.log gets 'grab probe' and 'host Body
# link ... FORMED' lines: read them first if the assert fails. KIT_Test_Link has its own locomotion and look: judge only the grab and the throw.
$P = 'C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_n216qs'
$C = "$P\Saved\AnimPipeline\captures\fthrow-overlord\v1r3"
Set-Location $P
Remove-Item Env:PIE_CAPTURE_PRE_PY -ErrorAction SilentlyContinue
Remove-Item Env:PIE_CAPTURE_RUNS -ErrorAction SilentlyContinue
$env:PIE_CAPTURE_PRE_PY = "$P\Tools\AnimPipeline\cards\overlord\fthrow\cap\fthrow_capture_pre.py"
$H = if ($env:FTHROW_HOLD_POINT) { $env:FTHROW_HOLD_POINT -split ',' } else { @('38', '-5', '36') }
$ini = "$P\Saved\Config\WindowsEditor\GSTNLink.ini"
$fb = 'FallbackThrows=(Forward="/Game/GSTN/Characters/Overlord/Moves/FThrow/GA_Overlord_FThrowV1.GA_Overlord_FThrowV1_C",Back="/Script/GSTNLinkNP.GSTN_GA_LinkThrowBack",Up="/Script/GSTNLinkNP.GSTN_GA_LinkThrowUp",Down="/Script/GSTNLinkNP.GSTN_GA_LinkThrowDown",Neutral="/Script/GSTNLinkNP.GSTN_GA_LinkThrowNeutral",CrouchForward="/Script/GSTNLinkNP.GSTN_GA_LinkThrowCrouchForward")'
$hp = "HoldPose=(bHoldByNeck=True,HoldPointCm=(X=$($H[0]),Y=$($H[1]),Z=$($H[2])),ExtraOffsetCm=(X=0.0,Y=0.0,Z=0.0))"
[IO.File]::WriteAllText($ini, "[/Script/GSTNLinkCore.GSTN_LinkSettings]`r`n$fb`r`n$hp`r`n")
$env:PIE_CAPTURE_DIST = '350'; $env:PIE_CAPTURE_VIEWS = 'az90,az180,game'; $env:PIE_CAPTURE_NOVFX = '1'
& "$P\Tools\GASNPPTest\pie_capture_timed.ps1" -Kit KIT_Test_Link -Out $C -Sequences fthrow_hit -Assert "$P\Tools\AnimPipeline\cards\overlord\fthrow\cap\assert_v1.json"
$rc = $LASTEXITCODE
Remove-Item $ini -ErrorAction SilentlyContinue
Remove-Item Env:PIE_CAPTURE_PRE_PY -ErrorAction SilentlyContinue
"exit $rc"
Select-String -Path "$C\capture.log" -Pattern 'grab probe|Body link' -ErrorAction SilentlyContinue | Select-Object -First 2 | ForEach-Object { $_.Line }
Get-Content "$C\assert.txt" -TotalCount 1 -ErrorAction SilentlyContinue
