# Guida pratica: Comandi (`Command.*`), Abilità di Combattimento e Verbi di Modalità (`GSTNModeVerbs`)

Questa guida spiega la differenza tra i tre concetti e mostra un esempio concreto di implementazione di un **Verbo di Modalità** (`UGSTN_GA_ModeVerb`).

Per la mappa completa (tutti i tag, tutti i verbi del giocatore, dentro/fuori simulazione, rollback sì/no) vedi [tag-verbi-simulazione-rollback.md](tag-verbi-simulazione-rollback.md).

---

## 1. Distinzione rapida

| Concetto | Dove vive | Ruolo | Rollback / Network Prediction | Esempio |
|---|---|---|---|---|
| **Comando classico** (`Command.*`) | `GSTNInput` / `GSTNInputNP` | È **solo l'input** (tasto, hold, motion input, contesto) messo nel buffer di input del pawn. Da solo non fa nulla. | **Sì** (entra nel frame input della simulazione NP) | `Command.Attack.Light`, `Command.Mode.Interact` (`Hold(Interact)`) |
| **Abilità di combattimento** (`UGSTN_GA_Base`) | `GSTNCombatGAS` / `GSTNCombatNP` | Esegue l'azione di gameplay **dentro** la simulazione: muove il pawn, apre hitbox, applica danni/stati. | **Sì** (tutta la logica di gioco è simulata e rieseguita al rollback) | Jab, Dropkick, Dodge |
| **Verbo di modalità** (`UGSTN_GA_ModeVerb`) | `GSTNModeVerbsNP` | **Ibrido:** la parte del giocatore (input, hold, animazione, interruzione da hit) è **dentro prediction**, ma al frame di conferma emette **un solo evento sul server** verso stato **fuori dal rollback** (`GSTNMission`, `GSTNRespawn`, drop). | **Metà dentro, metà fuori:** hold/anim nel rollback; effetto sulla modalità solo sul server su frame finalizzato | `Ability.Mode.Interact` (terminale), `Ability.Mode.CallDrop`, `Ability.Mode.CallExtraction` |

---

## 2. Perché il Verbo è diviso in due metà

Se un terminale o una chiamata rifornimenti fosse solo una RPC fuori prediction, il server non saprebbe sincronizzare al frame esatto animazione, blocco movimento o interruzione se un nemico ti colpisce mentre stai tenendo premuto.  
Al contrario, se lo stato della missione stesse dentro prediction, un rollback conterebbe due volte il completamento del terminale.

Quindi `UGSTN_GA_ModeVerb` separa i due livelli:
1. **Dentro la simulazione (`UGSTN_GA_ModeVerb`):** tiene `HoldStartClock` e lo stato (`Idle -> Holding -> Confirmed / Cancelled`) in variabili `FSyncInteger`/`FSyncByte` rollbackate. Quando scadono i `HoldFrames`, chiama `UGSTN_VerbSubsystem::QueueConfirm(...)` e chiude l'abilità.
2. **Fuori dalla simulazione (`UGSTN_VerbSubsystem`, solo server):** valida il bersaglio (`UGSTN_RequestableComponent::AcceptedVerbs`, `MaxUses`, `ActionableFrom`), esegue `OnInteracted` e pubblica **una sola volta** `Message.Mode.Verb` sullo stream autoritativo. Se il server rifiuta (es. un compagno ha attivato il terminale 1 frame prima), l'abilità del client si è già chiusa al frame di conferma: nessun rollback di stato necessario.

---

## 3. Esempio concreto: "Attiva un terminale" (`Ability.Mode.Interact`)

### Passo A — Input & Tag (Dati / Ini)
1. **Comando (`GSTNInput`):** definisci `Command.Mode.Interact` nello slot set del kit come `Hold(Interact, HoldFrames)` con requisito `Interact.Target.Objective` (proiettato dalla sonda `GSTNInteract` quando guardi il terminale).
2. **Tag (`Config/Tags/`):** `Ability.Mode.Interact` (identità verbo) e `Mission.Objective.Terminal1` (identità bersaglio).

### Passo B — L'Attore Terminale (`UGSTN_RequestableComponent`)
Sul Blueprint del terminale (es. `BP_MissionTerminal`), aggiungi **`UGSTN_RequestableComponent`** (figlio di `UGSTN_InteractableComponent`):
- `AcceptedVerbs`: `Ability.Mode.Interact`
- `IdentityTags`: `Mission.Objective.Terminal1`
- `MaxUses`: `1` (si disarma automaticamente dopo il primo utilizzo accettato)
- `bStartArmed`: `false` se la missione lo arma al momento giusto chiamando `Arm()`, oppure `true` se usabile da subito.

### Passo C — L'Abilità Verbo (C++ / Blueprint figlio di `UGSTN_GA_ModeVerb`)
Puoi creare un Blueprint figlio di `UGSTN_GA_ModeVerb` (es. `GA_Mode_ActivateTerminal`) e impostare solo i default:
- `VerbSpec.VerbTag` = `Ability.Mode.Interact`
- `VerbSpec.HoldFrames` = `45` (oppure `0` se il comando `Command.Mode.Interact` fa già lui il hold)
- `VerbSpec.TargetSource` = `InteractProbe` (`Interact.Target.Objective`)
- `VerbSpec.MaxRange` = `250.0`
- Aggiungi `Ability.Mode.Interact` negli **Asset Tags** della classe.
- Nei Blueprint events (`K2_OnVerbConfirmed`, `K2_OnVerbCancelled`) avvii solo presentazione (montage, barra hold, suono, luci del terminale — **mai** logica di missione).

Se invece vuoi personalizzare in C++:
```cpp
UCLASS()
class UGA_Mode_ActivateTerminal : public UGSTN_GA_ModeVerb
{
    GENERATED_BODY()
public:
    UGA_Mode_ActivateTerminal(const FObjectInitializer& ObjectInitializer)
        : Super(ObjectInitializer)
    {
        VerbSpec.VerbTag = GSTNModeVerbTags::Ability_Mode_Interact;
        VerbSpec.HoldFrames = 45;
        VerbSpec.MaxRange = 250.f;
    }
};
```

### Passo D — Collegamento a Missione & Experience (Solo Dati)
1. **Missione (`UGSTN_MissionDefinition`):** aggiungi un obiettivo di tipo `Activate` che ascolta `Message.Mode.Verb` con tag query su `Mission.Objective.Terminal1`.
2. **Experience (`UGSTN_ExperienceDefinition` / Game Feature):** inserisci `GA_Mode_ActivateTerminal` nell'AbilitySet della modalità. Disattivando la Game Feature della modalità, il verbo sparisce senza toccare `GSTNMission` o `GSTNCombat`.
