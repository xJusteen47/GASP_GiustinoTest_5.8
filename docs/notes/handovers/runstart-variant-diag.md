# runstart-variant-diag: "variant 2 looks like stock" = gstn.Loco.Base was 0 (desktop, 2026-11-21)
Cause: owner had not set gstn.Loco.Base 1; gstn.RunStart.Variant only acts on base 1 (ABP_GSTN_StockMover chooser column), so base 0 played the stock start. Owner confirmed in chat. No code change.
Fix: in the editor console `gstn.Loco.Base 1` then `gstn.RunStart.Variant 2`. Not checked (stopped early): chooser pick, 0.3 s blend-in masking, binaries (editor started 01:19 after the real build, so likely fine).
COSTO: ~0.5 $ | 0 min desktop | review 0 token
