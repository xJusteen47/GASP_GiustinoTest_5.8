# Course ingestion (licensed video courses → private study notes)

Scope: video courses the owner is licensed to watch (e.g. Gnomon via the university). Purpose: internal study only.
**No redistribution:** no video, audio, transcript or course frame goes into git, project files, artifacts or the
public skills repo. Only our own paraphrased notes go into this folder (private game repo), each claim with its
lesson and timestamp so a person can check it in the original.

## Where things live
| Thing | Where | In git? |
|---|---|---|
| Original videos | desktop `F:\Corsi\<course>\` | no |
| Audio extracts (mp3 16 kHz mono) | desktop `F:\Corsi\_studio_claude\audio\` (`BM`, `SA`, `VR` + lesson number) | no |
| Raw transcripts (timestamped) | desktop `F:\Corsi\_studio_claude\trascrizioni\` | no |
| Course frames for study | desktop `F:\Corsi\_studio_claude\frames\` | no |
| Study notes (paraphrased, timestamped) | `docs/knowledge/study/<course>.md` | yes (private repo) |
| Principles promoted to general level 1 | skills repo `knowledge/`, rephrased and backed by a non-course source or a measurement | yes |

## Steps (any agent can repeat them for a new course)
1. **Audio** (desktop VM shell, ffmpeg): `ffmpeg -i <video> -vn -ac 1 -ar 16000 -b:a 24k <code><nn>.mp3`.
   Under 3 minutes per call; skip files already listed in `ok.txt`.
2. **Transcribe** (cloud container): stage the mp3s, run whisper.cpp through `pywhispercpp` with `ggml-base.en.bin`
   (SHA-1 `137c40403d78fd54d454da0f9bd998f78703390c`; the official Hugging Face host is blocked, a byte-identical
   copy on GitHub LFS is used, checked by hash). About 17x real time on 4 cores. Output `[mm:ss] text` per segment.
   Copy the transcripts back to the desktop folder, not to git.
3. **Digest** (one subagent per course, Sonnet): read the transcripts, write `study/<course>.md` with the template below.
   Rules are paraphrased, generalised, mapped to level-1 IDs or marked `NEW` (candidate criterion) or `TOOL` (software
   workflow, e.g. Maya-specific), with `lesson mm:ss`.
4. **Frames**: for each item that needs a picture, extract one frame on the desktop
   (`ffmpeg -ss <t> -i <video> -frames:v 1 frames/<code><nn>_<mmss>.jpg`) and cite its path. Frames stay on the desktop.
5. **Integrate**: `NEW` items that hold beyond one example and agree with a book or a measurement go to level 1 (skills
   repo) as new IDs, rephrased, citing the general source; game-specific taste goes to the profile; worked examples to
   `cases/`. Course-only items stay here.

## Study-note template
```
# <Course title> (<instructor>), study notes
Scope: what the course covers, rig/software used, what transfers to a game pipeline.
## Lesson <nn> — <title> (<duration>)
- <rule, paraphrased> — <lesson mm:ss> — maps to <ID | NEW | TOOL>
## Candidate new criteria (NEW)
| Proposed ID | Rule | Evidence (lessons) | Measurable how |
## Workflow notes (TOOL)
## Frames worth extracting
| lesson mm:ss | what it shows | for which ID |
```
