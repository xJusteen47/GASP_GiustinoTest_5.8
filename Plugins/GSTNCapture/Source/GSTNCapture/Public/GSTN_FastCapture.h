#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GSTN_FastCapture.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
struct FGSTN_FastCaptureShared;

/** Tuning of one fast capture session (all views share it). */
USTRUCT(BlueprintType)
struct GSTNCAPTURE_API FGSTN_FastCaptureSettings
{
	GENERATED_BODY()

	/** Square picture size of every view, in pixels (the review captures use 480; 960 for an appeal review). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture|Output", meta = (ClampMin = "64", ClampMax = "2048"))
	int32 Size = 480;

	/**
	 * Frames kept in memory before Commit: a move's press frame is often known only when it happens (an aerial pressed at a height), so
	 * the driver shoots from the jump into this ring and Commit(PressFrame) keeps the PreRollFrames before the press. Older frames are
	 * dropped without touching the disk.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture|Window", meta = (ClampMin = "0", ClampMax = "240"))
	int32 PreRollFrames = 20;

	/**
	 * Readbacks per view that may wait on the GPU before Shoot blocks on the oldest. The GPU normally runs 1-3 frames behind; the bound
	 * only stops memory from growing when the GPU is much slower than the game thread.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture|Pipeline", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxInFlightPerView = 6;

	/** PNG quality passed to the image wrapper (0 = engine default). The pictures are 480 px: compression time is small next to readback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture|Output")
	int32 PngQuality = 0;

	/**
	 * Show flags turned OFF on the review views (AddView with bReviewLighting), comma separated FEngineShowFlags names. "Simple light",
	 * not unlit: the shadowless fill lights still shade the body, so elbows and arms in front of the chest keep their depth (owner,
	 * 2026-11-23). Unknown names are logged and skipped. The game view keeps the full lighting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture|Lighting")
	FString ReviewFlagsOff = TEXT("DynamicShadows,ContactShadows,LumenGlobalIllumination,LumenReflections,AmbientOcclusion,DistanceFieldAO,ScreenSpaceReflections,MotionBlur,VolumetricFog");
};

/**
 * Fast review capture: the replacement of pie_capture_move.py's per frame capture_scene() + export_render_target(), which rendered each
 * view, stopped the engine until the GPU finished, read the picture back and compressed a PNG on the game thread, view after view
 * (4.4 fps with 7 views, docs/notes/reviews/cp5-data/dair_base/capture.log).
 *
 * Here Shoot() only queues work: each view's scene capture is enqueued (same as CaptureScene from Python), then a GPU->CPU copy into an
 * FRHIGPUTextureReadback. Pump() (called by Shoot and EndSequence) collects the copies the GPU has finished, on the render thread, and
 * hands the pixels to worker tasks that write <SequenceDir>/<view>/f<NNN>.png, the same files as the Python path, so every downstream tool
 * (capture_assert, pie_capture_grid, review_pack) is unchanged. The game thread never waits on the GPU unless MaxInFlightPerView is hit.
 *
 * The simulation is untouched: the driver still steps 1/60 s per rendered frame (t.OverrideFPS 60), so base and variant show the same
 * frames (project rule 10). Only the frames between Commit's pre-roll and the driver's last Shoot reach the disk.
 *
 * Use (Python): fc = unreal.GSTN_FastCapture.create_fast_capture(world, settings); fc.add_view('az90', comp, True) per view;
 * per sequence fc.begin_sequence(dir), fc.shoot(frame) every frame, fc.commit(press_frame) once, fc.end_sequence(20.0); fc.release().
 * The object is rooted until Release(): the Python handle may be the only reference.
 */
UCLASS(BlueprintType)
class GSTNCAPTURE_API UGSTN_FastCapture : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GSTN|Capture", meta = (WorldContext = "WorldContext"))
	static UGSTN_FastCapture* CreateFastCapture(UObject* WorldContext, const FGSTN_FastCaptureSettings& Settings);

	/**
	 * Adds a view: the component gets its own render target (RTF_RGBA8, as the Python path's create_render_target2d) and is set to
	 * capture only on request. bReviewLighting applies the simple light (Settings.ReviewFlagsOff, Lumen GI and reflections off through the
	 * post process override). Returns the view index, or -1 for a null component.
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Capture")
	int32 AddView(const FString& Name, USceneCaptureComponent2D* Capture, bool bReviewLighting);

	/** Starts a sequence: pictures go to <SequenceDir>/<view name>/. Ends the previous one first if it is still open. */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Capture")
	void BeginSequence(const FString& SequenceDir);

	/** Captures every view for this rendered frame. Before Commit the frame waits in the pre-roll ring; after it, it is written. */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Capture")
	void Shoot(int32 Frame);

	/**
	 * Keeps frames from PressFrame - PreRollFrames on: the ring's frames in that range are written, older ones dropped. Call it once per
	 * sequence; a sequence without a press commits on its first frame (everything is kept, as before).
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Capture")
	void Commit(int32 PressFrame);

	/** Collects finished readbacks and starts their writes. Cheap; Shoot calls it. */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Capture")
	void Pump();

	/**
	 * Waits (blocking) for the GPU, the readbacks and the PNG writes of this sequence. Returns false on timeout. Uncommitted frames are
	 * dropped. Writes <SequenceDir>/fastcap.json (counts and timings).
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Capture")
	bool EndSequence(float TimeoutSeconds = 20.f);

	/** One JSON line: frames shot, written, dropped, game thread ms in Shoot, peak readbacks in flight. */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Capture")
	FString GetStats() const;

	/** Ends the sequence, drops the views and un-roots the object. */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Capture")
	void Release();

	virtual void BeginDestroy() override;

private:
	struct FView
	{
		FString Name;
		TWeakObjectPtr<USceneCaptureComponent2D> Capture;
	};

	void StartWrite(int32 Frame, int32 ViewIndex, TArray<FColor>&& Pixels);
	void TrimRing();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> Targets;

	TArray<FView> Views;
	FGSTN_FastCaptureSettings Settings;
	TSharedPtr<FGSTN_FastCaptureShared, ESPMode::ThreadSafe> Shared;

	/** Frames read back before Commit, by frame then view. */
	TMap<int32, TArray<TPair<int32, TArray<FColor>>>> Ring;

	FString SequenceDir;
	bool bInSequence = false;
	bool bCommitted = false;
	int32 CommitFrom = 0;
	int32 LastShotFrame = INDEX_NONE;

	// Stats of the current sequence
	int32 Shot = 0;
	int32 Dropped = 0;
	int32 PeakInFlight = 0;
	int32 Stalls = 0;
	double ShootSeconds = 0.0;
	double SequenceStart = 0.0;
};
