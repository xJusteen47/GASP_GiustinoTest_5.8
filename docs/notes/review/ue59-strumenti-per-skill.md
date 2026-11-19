# UE 5.9 preview 2: strumenti utili alle skill di animazione e VFX

Owner 2026-11-19: "è la preview 2 di 5.9.0, vedi cosa hanno aggiunto di utile". Fonte: le note della preview riportate
dall'owner/coordinatore; **nessuno è ancora provato nel nostro progetto**. Prima di usarne uno in una skill: provarlo
una volta sul fisso, poi togliere "da provare" qui. Nomi esatti di toolset e funzioni: leggerli dall'editor (o
chiederli all'Unreal Assistant), mai a memoria.

| Novità | A cosa serve a noi | Chi la usa |
|---|---|---|
| **MCP che parte con l'editor**, con toolset di test e debug del gameplay | Catture, dump di curve e stati senza il nostro bridge: il thread sul fisso legge curve di clip e montage (`curve.txt`, reviewer anim §3.7) e lancia PIE di prova da lì | esecutore anim, catture VFX |
| **Unreal Assistant** (pannello AI nell'editor) | È l'"assistente AI dell'Unreal Editor" delle skill: domande sul motore (ABP, montage, curve, Niagara, materiali). Per come funziona il motore, mai per giudicare l'aspetto | tutti |
| **Chooser modificabili da Blueprint/Python** | Le righe di `CHT_GSTN_OwnStates` (clip dei nostri stati) si aggiungono da script invece che a mano: un passo ripetibile e verificabile | esecutore anim, thread di codice |
| **Notify UAF che lanciano Niagara** | L'effetto parte dal frame della mossa (contatto, appoggio) scritto nell'animazione: aiuta la domanda 6 del revisore VFX (frame dell'evento) | chi costruisce VFX |
| **API Python per Niagara** | Leggere e scrivere parametri di un sistema (vita, curve di alpha e taglia, numero di particelle): chi costruisce applica una correzione in modo ripetibile; il revisore può chiedere il dump delle curve accanto al video (es. "l'alpha arriva a 0 entro la vita?") | costruttore e revisore VFX |

Il progetto oggi lavora su `_ue59` (preview); quando una di queste è provata, la riga va nella skill che la usa.
