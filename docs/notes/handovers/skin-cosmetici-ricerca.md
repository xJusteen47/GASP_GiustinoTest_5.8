# Handover skin-cosmetici-ricerca: temi 3D modulari per armi e oggetti (2026-11-23)

Branch `claude/skin-cosmetici-ricerca`, base `feature/gas-npp-5.8` (a5b2095). Thread cloud, Opus, SOLO ricerca: nessun codice, _ue59 non toccata.

## Cosa
- Documento `docs/notes/skin-cosmetici-modulari.md`: riassunto 10 righe + 3 decisioni in cima, poi
  giochi veri e limite di scala, tecniche UE (socket, spline mesh, kit modulare, PCG, Geometry Script, Nanite displacement,
  material layer + vertex color, trim sheet, morph, cloth), convenzioni sul modello base + checklist designer (tutto in UE,
  niente Blender), skin reattive e stili, modello dati (ThemeData, HostData, Loadout, fallback), integrazione, rischi, fasi.
- Idea chiave: il costo passa da temi x armi a temi + armi (l'arma dichiara dove decorare, il tema come decorare,
  un assemblatore unisce e fa bake). Slot del corpo una volta sullo scheletro unico.

## Decisioni
- Owner 11-23: 1A confermata; il tema si vende una volta e va su qualsiasi oggetto, quindi niente bake per coppia;
  Orokin/Dax/Sentient erano solo esempi. Nuove sezioni 2b (costo runtime per oggetto, fusione in una mesh, cache,
  limiti, LOD a solo materiale) e 2c (idea owner: costruzione all'armadietto + al caricamento per i loadout altrui).
- Pendente owner: decisione 2 nuova, consigliata A (armadietto + caricamento, stesso assemblatore, runtime solo rete di sicurezza).

## Non verificato
- Stato in UE 5.9 di Geometry Script (spline deform, bake), PCG su superfici mesh, Nanite displacement: marcati [da verificare 5.9],
  fonte = conoscenza fino a 5.6/5.7. F0 del piano li verifica con Tools/UE59/ask_unreal_assistant.py.
- Costi runtime (draw call, ms, KB) della 2b/2c sono ordini di grandezza, non misurati: F1 li misura.
- Valori di silhouette (+5% lunghezza, +15% larghezza) e i tempi per arma (30-60 min) sono stime di partenza, non misurati.

## Seguito
Nessun thread finché l'owner non sblocca (coda: prima skill di animazione e architettura skin). F0-F2 stimati 16-26 $ (cache su disco +1-2 $).

SUBAGENTI: nessuno
COSTO: ~3 $ | 0 min desktop | 0 token review
