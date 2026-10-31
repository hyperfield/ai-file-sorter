# Hurtigstartguide

AI File Sorter hjelper deg å organisere filer med gjennomgang før endringer utføres. Valgfri automatisk godkjenning kan forhåndsvelge endringer i gjennomgangslisten.

AI-en foreslår kategorier, målmapper og navn. Appen flytter eller gir filer nytt navn først etter at du bekrefter de valgte endringene i gjennomgangsdialogen.

## Sikker forste kjoring

Hvis dette er forste gang du bruker AI File Sorter, start med en liten testmappe for du peker den mot et stort arkiv eller en hel disk.

Gode mapper for forste kjoring er:

- en kopi av 20-50 filer fra `Downloads`
- en liten mappe for skjermbilder eller fotoopprydding
- en midlertidig mappe med noen PDF-er eller dokumenter

Dette gjor den forste kjoringen enkel a gjennomga. Filene dine blir pa datamaskinen nar du bruker lokale modeller, og AI-en foreslar bare kategorier og navn. La auto-godkjenning vaere av ved forste kjoring, slik at ingenting flyttes eller gis nytt navn for du godkjenner gjennomgangslisten.

## Sett opp en LLM

Åpne **Settings -> Select LLM**. Velg en modell fra nedtrekkslisten **Local LLMs** eller en leverandør under **API connections**.

**Qwen3.5 4B (Recommended)** er det første lokale alternativet og standarden for nye oppsett. Modellen gir høy presisjon ved kategorisering og anbefales for eksisterende mappestrukturer. Tidligere lagrede modellvalg beholdes. For å bruke din egen GGUF-fil velger du **Custom local LLM (gguf)** og deretter **Add**.

For en lokal modell som tilbys i appen, utvider du **Downloads**, klikker **Download** og venter på **Model ready** før du bekrefter med **OK**. **Model storage** lar deg velge nedlastingsmappe. **See details** viser nedlastingsadressen, lokal filsti og teknisk informasjon.

For bildeanalyse velger du også en **Visual model** under **Image analysis models** og laster ned alle nødvendige filer. Dette valget er uavhengig av tekstmodellen.

Under **API connections** velger du Gemini, ChatGPT (OpenAI) eller et egendefinert OpenAI-kompatibelt endepunkt og oppgir nødvendige tilkoblingsdetaljer og modellnavn. Sky-API-er krever internett og sender analysegrunnlaget til den konfigurerte tjenesten. Egendefinerte endepunkter kan også kjøre lokalt.

## Sjekk lokal ytelse (valgfritt)

Når modellene er lastet ned, åpner du **File -> System compatibility check** og klikker **Run benchmark**. Lukk andre CPU- eller GPU-krevende apper først. Sjekken tester nedlastede lokale modeller, inkludert egendefinerte GGUF-modeller.

La **Backend** stå på **Automatic (try CPU if GPU fails)**, eller velg **All available backends** for å sammenligne CPU og støttede GPU-alternativer. Resultatene lagres separat for hver modell og backend; en feil på én utelukker ikke en fungerende kombinasjon på en annen.

Velg et resultat og bruk **Retry selected** for å teste den kombinasjonen på nytt, **Retry failed** for å teste mislykkede kombinasjoner på nytt, eller **Try chosen backend** for å teste den valgte modellen med en bestemt backend fra nedtrekkslisten.

Anbefalingene anslår lokal ytelse. Sjekken tester verken API-tjenester eller nøyaktigheten ved valg av eksisterende mapper, og den endrer ikke den valgte modellen automatisk.

## 1. Velg en mappe

Bruk **Browse** eller **File Explorer**-panelet til a velge mappen du vil sortere.

Typiske eksempler:

- `Downloads`
- en mappe for skrivebordsopprydding
- en mappe pa en ekstern disk
- en nettverks- eller skysynkronisert mappe
- et prosjektarkiv

**Destination** bestemmer hvor sorterte filer havner. La **Use analyzed folder** være aktivert for å sortere i den valgte mappen, eller slå det av og velg en annen målmappe.

## 2. Velg hva appen skal gjore

Bruk hovedalternativene til a bestemme om appen skal:

- kategorisere filer i mapper
- analysere bilder
- analysere dokumenter
- tilby forslag til nytt navn for stottede filer

Hvis du bare vil ha forslag til nytt navn, aktiverer du den relevante bare-gi-nytt-navn-modusen.

## 3. Velg hvordan filene skal sorteres

Velg en sorteringsmodus:

- **Create category folders** oppretter mapper fra foreslåtte kategorinavn.
- **Use existing folder structure** velger mapper i mappetreet under **Destination**. Aktiver **Suggest new folders when needed** for å tillate nye målmapper når ingen passer godt; la det være av for bare å bruke eksisterende mapper.

For eksisterende mapper anbefaler vi **Qwen3.5 4B**. **Gemma 1.1 7B** og modeller som brukes via API, er også egnede alternativer. Andre lokale modeller viser en anbefaling; du kan beholde modellen eller endre den i **Settings -> Select LLM**.

I modusen **Create category folders** velger du en kategoriseringsstil:

- **More refined** for generell bruk og finere gruppering
- **More consistent** hvis du vil ha sterkere etikettkonsistens pa tvers av lignende filer

Du kan ogsa aktivere kategorihvitelister hvis du vil at appen skal holde seg innenfor et smalere sett med kategorinavn.

## 4. Start analysen

Klikk **Analyze folder**.

Appen skanner den valgte mappen, samler inn informasjonen den trenger, og forbereder en gjennomgangsliste.

## 5. Gjennomga for du bruker endringer

Gjennomgangsdialogen lar deg kontrollere:

- foreslatte kategorier
- valgfrie underkategorier
- forslag til nytt navn for stottede filer
- endelige malstier
- filforhandsvisninger der det stottes

Du kan justere eller avvise forslag for du bekrefter noe.

Aktiver **Dry run (preview only, do not move files)** for å forhåndsvise planlagte flyttinger og navneendringer uten å utføre dem.

## 6. Bruk endringene

Velg radene du vil behandle, og klikk **Confirm and Process**. Appen oppretter nødvendige mapper og utfører de valgte flyttingene eller navneendringene.

**Auto-approve filename changes** og **Auto-approve categorization** forhåndsvelger aktuelle rader. Du må fortsatt bekrefte før endringene utføres.

## 7. Angre siste kjoring

Hvis du bruker endringer og deretter onsker a reversere dem, bruk **Edit -> Undo last run** fra menyen.

Angre er laget for den siste bekreftede sorteringskjoringen. Den bruker appens registrerte kjoringshistorikk til a flytte filer tilbake og reversere stottede navneendringer der det er mulig.

For best resultat bor du bruke Angre for du starter en ny stor opprydding i samme mappe.

## 8. Laer av gjennomgangene dine

Nar du godkjenner kategorier i gjennomgangsdialogen, kan appen huske disse lokale beslutningene og bruke dem som hint i fremtidige kjoringer. Dette trener eller endrer ikke AI-modellen.

De laerte eksemplene lagres i en separat lokal database, sa rydding av den vanlige kategoriseringscachen fjerner dem ikke. For a fjerne disse lokale laeringsdataene bruker du **Settings -> Reset learned behavior**.

## Greit a vite

- Appen bruker en lokal cache for a unnga a behandle de samme filene pa nytt og for a forbedre konsistensen.
- Auto-godkjenning er valgfritt. La det vaere av til du er komfortabel med appens forslag.
- Bilde- og dokumentalternativer kan utvides separat hvis du trenger mer kontroll.

## Hvis noe ser feil ut

Sjekk dette forst:

- den valgte mappen er den du mente a bruke
- de relevante analysealternativene er aktivert
- bare-gi-nytt-navn-modusen begrenser ikke resultatet pa en uventet mate
- en kategorihviteliste snevrer ikke inn forslagene for mye

For mer feilsoking, apne **Help -> FAQ**.
