# Guida rapida

AI File Sorter ti aiuta a organizzare i file con una revisione preliminare. L'approvazione automatica facoltativa può preselezionare le modifiche nell'elenco di revisione.

L'IA suggerisce categorie, cartelle di destinazione e nomi. L'app sposta o rinomina i file solo dopo che confermi le modifiche selezionate nella finestra di revisione.

## Prima esecuzione sicura

Se usi AI File Sorter per la prima volta, inizia con una piccola cartella di prova prima di scegliere un grande archivio o un intero disco.

Buone cartelle per la prima esecuzione includono:

- una copia di 20-50 file da `Downloads`
- una piccola cartella di screenshot o foto da pulire
- una cartella temporanea con alcuni PDF o documenti

Questo rende facile controllare la prima esecuzione. I file restano sul computer quando usi modelli locali, e l'IA suggerisce solo categorie e nomi. Lascia disattivate le opzioni di approvazione automatica per la prima esecuzione, cosi nulla viene spostato o rinominato finche non approvi l'elenco di revisione.

## Configurare un LLM

Apri **Settings -> Select LLM**. Scegli un modello dall'elenco a discesa **Local LLMs** o un fornitore in **API connections**.

**Qwen3.5 4B (Recommended)** è la prima opzione locale e quella predefinita per le nuove configurazioni. Offre un'elevata precisione di categorizzazione ed è consigliato per le strutture di cartelle esistenti. La selezione del modello già salvata viene mantenuta. Per usare un tuo file GGUF, scegli **Custom local LLM (gguf)** e poi **Add**.

Per un modello locale proposto dall'app, espandi **Downloads**, fai clic su **Download** e attendi **Model ready** prima di confermare con **OK**. **Model storage** permette di scegliere la cartella di download. **See details** mostra l'URL di download, il percorso locale e le informazioni tecniche.

Per l'analisi delle immagini, scegli anche un **Visual model** in **Image analysis models** e scarica tutti i file necessari. Questa scelta è separata dal modello di testo.

In **API connections**, scegli Gemini, ChatGPT (OpenAI) o un endpoint personalizzato compatibile con OpenAI, poi inserisci i dati di connessione richiesti e il nome del modello. Le API cloud richiedono Internet e inviano i dati di analisi al servizio configurato. Gli endpoint personalizzati possono anche funzionare localmente.

## Verificare le prestazioni locali (facoltativo)

Dopo aver scaricato i modelli, apri **File -> System compatibility check** e fai clic su **Run benchmark**. Chiudi prima le altre app che usano intensamente CPU o GPU. La verifica prova i modelli locali scaricati, compresi i modelli GGUF personalizzati.

Lascia **Backend** su **Automatic (try CPU if GPU fails)** oppure scegli **All available backends** per confrontare la CPU e le opzioni GPU supportate. I risultati vengono salvati separatamente per ogni modello e backend; un errore su uno non esclude una combinazione funzionante su un altro.

Seleziona un risultato e usa **Retry selected** per ripetere quella combinazione, **Retry failed** per ripetere le combinazioni non riuscite o **Try chosen backend** per provare il modello selezionato con un backend specifico dell'elenco a discesa.

I consigli stimano le prestazioni locali. La verifica non prova i servizi API né la precisione nella scelta delle cartelle esistenti e non cambia automaticamente il modello selezionato.

## 1. Scegliere una cartella

Usa **Browse** o il riquadro **File Explorer** per scegliere la cartella da ordinare.

Esempi tipici:

- `Downloads`
- una cartella di pulizia del desktop
- una cartella su un disco esterno
- una cartella di rete o sincronizzata con il cloud
- un archivio di progetto

**Destination** determina dove vanno i file ordinati. Lascia **Use analyzed folder** attivo per ordinare nella cartella selezionata, oppure disattivalo e scegli un'altra destinazione.

## 2. Scegliere cosa deve fare l'app

Usa le opzioni principali per decidere se l'app deve:

- categorizzare i file in cartelle
- analizzare immagini
- analizzare documenti
- offrire suggerimenti di rinomina per i file supportati

Se vuoi solo suggerimenti di rinomina, abilita la modalita solo rinomina pertinente.

## 3. Scegliere come ordinare

Scegli una modalità di ordinamento:

- **Create category folders** crea cartelle dai nomi di categoria suggeriti.
- **Use existing folder structure** sceglie cartelle nell'albero di **Destination**. Attiva **Suggest new folders when needed** per consentire nuove destinazioni quando nessuna è adatta; lascialo disattivato per usare solo cartelle esistenti.

Per le cartelle esistenti consigliamo **Qwen3.5 4B**. Anche **Gemma 1.1 7B** e i modelli utilizzati tramite API sono opzioni adatte. Gli altri modelli locali mostrano un avviso; puoi mantenere il modello o cambiarlo in **Settings -> Select LLM**.

In modalità **Create category folders**, scegli uno stile di categorizzazione:

- **More refined** per uso generale e raggruppamenti piu dettagliati
- **More consistent** se vuoi maggiore coerenza delle etichette tra file simili

Puoi anche abilitare whitelist di categorie se vuoi che l'app resti entro un insieme piu ristretto di nomi categoria.

## 4. Avviare l'analisi

Fai clic su **Analyze folder**.

L'app scansiona la cartella selezionata, raccoglie le informazioni necessarie e prepara un elenco di revisione.

## 5. Rivedere prima di applicare modifiche

La finestra di revisione permette di controllare:

- categorie suggerite
- sottocategorie opzionali
- suggerimenti di rinomina per i file supportati
- percorsi di destinazione finali
- anteprime dei file dove supportate

Puoi modificare o rifiutare i suggerimenti prima di confermare qualsiasi cosa.

Attiva **Dry run (preview only, do not move files)** per visualizzare in anteprima gli spostamenti e le rinomine previsti senza applicarli.

## 6. Applicare le modifiche

Seleziona le righe da elaborare e fai clic su **Confirm and Process**. L'app crea le cartelle necessarie ed esegue gli spostamenti o le rinomine selezionati.

**Auto-approve filename changes** e **Auto-approve categorization** preselezionano le righe idonee. Devi comunque confermare prima che le modifiche vengano applicate.

## 7. Annullare l'ultima esecuzione

Se applichi le modifiche e poi vuoi annullarle, usa **Edit -> Undo last run** dal menu.

L'annullamento e progettato per l'ultima esecuzione di ordinamento confermata. Usa la cronologia registrata dall'app per riportare i file indietro e annullare le rinomine supportate quando possibile.

Per risultati migliori, usa Annulla prima di iniziare un'altra grande pulizia nella stessa cartella.

## 8. Apprendere dalle revisioni

Quando approvi categorie nella finestra di revisione, l'app puo ricordare queste decisioni locali e usarle come indicazioni per le esecuzioni future. Questo non addestra ne modifica il modello IA.

Gli esempi appresi sono memorizzati in un database locale separato, quindi svuotare la normale cache di categorizzazione non li rimuove. Per rimuovere questi dati di apprendimento locali, usa **Settings -> Reset learned behavior**.

## Buono a sapersi

- L'app usa una cache locale per evitare di rielaborare gli stessi file e migliorare la coerenza.
- L'approvazione automatica e opzionale. Lasciala disattivata finche non ti senti a tuo agio con i suggerimenti dell'app.
- Le opzioni per immagini e documenti possono essere espanse separatamente se serve piu controllo.

## Se qualcosa sembra sbagliato

Controlla prima quanto segue:

- la cartella selezionata e quella prevista
- le opzioni di analisi pertinenti sono abilitate
- la modalita solo rinomina non limita il risultato in modo inatteso
- una whitelist di categorie non restringe troppo i suggerimenti

Per ulteriore risoluzione dei problemi, apri **Help -> FAQ**.
