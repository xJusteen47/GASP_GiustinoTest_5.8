# Setup Guida: Sakurai AI Combat Animator

Questo pacchetto contiene tutto il necessario per animare il tuo picchiaduro 3D in Unreal Engine 5 (UEFN GASP Rig) sfruttando Agent AI (Cline, Google Anti-Gravity, o Aider) in locale o via Cloud.

## 1. Strumenti Necessari (Aggiornati a Nov 2026)
- **Motore:** Unreal Engine 5 (con progetto basato su GASP / UEFN Control Rig).
- **Harness (L'Interfaccia dell'Agente):** 
  - *Google Anti-Gravity 2.0* (Usa l'Agent Manager interno).
  - *VS Code + Estensione Cline* (o Roo Cline).
- **Modello AI (Il Cervello):**
  - *Consigliato Cloud:* DeepSeek-V4 o Kimi (via OpenRouter/API nativa). Costano pochissimo e hanno il miglior reasoning spaziale.
  - *Consigliato Locale:* Qwen2.5-Coder:32B (via Ollama) o OpenHands-lm-32b.
- **Protocollo di Comunicazione:**
  - Plugin UEMCP (Model Context Protocol per Unreal) oppure attivare la "Python Remote Execution" da dentro UE5.

## 2. Dove posizionare i file
1. Copia `sakurai_uefn_toolkit.py` nella cartella `Content/Python` del tuo progetto Unreal Engine. 
2. Copia `smash-animator.md` nella tua cartella `.claude/commands/`, oppure usalo come System Prompt personalizzato nel tuo Custom Agent di Google Anti-Gravity / Cline.

## 3. Workflow Operativo
1. Apri la tua Level Sequence in UE5 con il manichino UEFN pronto.
2. Apri il tuo AI Agent (es. Cline).
3. Usa questo prompt per avviare il loop:

> "Usa la skill smash-animator. Crea un attacco Forward Smash usando il modulo sakurai_uefn_toolkit. Imposta le chiavi cardinali. Una volta applicate, esegui l'audit 3D. Se l'audit segnala T-Rex Arms o Clipping col pavimento, modifica l'offset spaziale e riesegui finché tutti i test non passano."
