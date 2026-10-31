# Snabbstartguide

AI File Sorter hjälper dig att organisera filer med granskning innan ändringar genomförs. Valfritt automatiskt godkännande kan förvälja ändringar i granskningslistan.

AI:n föreslår kategorier, målmappar och namn. Appen flyttar eller byter namn på filer först efter att du har bekräftat de valda ändringarna i granskningsdialogen.

## Saker forsta korning

Om det ar forsta gangen du anvander AI File Sorter, borja med en liten testmapp innan du valjer ett stort arkiv eller en hel disk.

Bra mappar for forsta korningen ar:

- en kopia av 20-50 filer fran `Downloads`
- en liten mapp for skarmbilder eller fotorensning
- en temporar mapp med nagra PDF:er eller dokument

Detta gor den forsta korningen enkel att granska. Dina filer stannar pa datorn nar du anvander lokala modeller, och AI:n foreslar bara kategorier och namn. Lat automatiskt godkannande vara av vid forsta korningen sa att inget flyttas eller byter namn innan du godkanner granskningslistan.

## Ställ in en LLM

Öppna **Settings -> Select LLM**. Välj en modell i listrutan **Local LLMs** eller en leverantör under **API connections**.

**Qwen3.5 4B (Recommended)** är det första lokala alternativet och standardvalet för nya inställningar. Modellen ger hög precision vid kategorisering och rekommenderas för befintliga mappstrukturer. Ditt tidigare sparade modellval behålls. För att använda en egen GGUF-fil väljer du **Custom local LLM (gguf)** och sedan **Add**.

För en lokal modell som appen erbjuder, fäll ut **Downloads**, klicka på **Download** och vänta på **Model ready** innan du bekräftar med **OK**. Med **Model storage** väljer du nedladdningsmapp. **See details** visar nedladdningsadressen, den lokala sökvägen och teknisk information.

För bildanalys väljer du också en **Visual model** under **Image analysis models** och laddar ned alla filer som krävs. Detta val är separat från textmodellen.

Under **API connections** väljer du Gemini, ChatGPT (OpenAI) eller en egen OpenAI-kompatibel ändpunkt och anger nödvändiga anslutningsuppgifter och modellnamn. Moln-API:er kräver internet och skickar analysunderlaget till den konfigurerade tjänsten. Egna ändpunkter kan också köras lokalt.

## Kontrollera lokal prestanda (valfritt)

När modellerna har laddats ned öppnar du **File -> System compatibility check** och klickar på **Run benchmark**. Stäng först andra appar som belastar CPU eller GPU mycket. Kontrollen testar nedladdade lokala modeller, även egna GGUF-modeller.

Låt **Backend** stå på **Automatic (try CPU if GPU fails)** eller välj **All available backends** för att jämföra CPU och GPU-alternativ som stöds. Resultaten sparas separat för varje modell och backend; ett fel på en backend utesluter inte en fungerande kombination på en annan.

Välj ett resultat och använd **Retry selected** för att testa samma kombination igen, **Retry failed** för att testa misslyckade kombinationer igen eller **Try chosen backend** för att testa den valda modellen med en viss backend i listrutan.

Rekommendationerna uppskattar lokal prestanda. Kontrollen testar varken API-tjänster eller träffsäkerheten vid val av befintliga mappar och ändrar inte din valda modell automatiskt.

## 1. Valj en mapp

Anvand **Browse** eller panelen **File Explorer** for att valja mappen du vill sortera.

Typiska exempel:

- `Downloads`
- en mapp for skrivbordsrensning
- en mapp pa en extern disk
- en natverks- eller molnsynkroniserad mapp
- ett projektarkiv

**Destination** avgör var sorterade filer hamnar. Låt **Use analyzed folder** vara aktiverat för att sortera i den valda mappen, eller stäng av det och välj ett annat mål.

## 2. Valj vad appen ska gora

Anvand huvudalternativen for att bestamma om appen ska:

- kategorisera filer i mappar
- analysera bilder
- analysera dokument
- erbjuda namnbytesforslag for filer som stods

Om du bara vill ha namnbytesforslag, aktivera relevant endast-byta-namn-lage.

## 3. Välj hur filerna ska sorteras

Välj ett sorteringsläge:

- **Create category folders** skapar mappar från föreslagna kategorinamn.
- **Use existing folder structure** väljer mappar i mappträdet under **Destination**. Aktivera **Suggest new folders when needed** för att tillåta nya målmappar när ingen passar bra; låt det vara av för att bara använda befintliga mappar.

För befintliga mappar rekommenderar vi **Qwen3.5 4B**. **Gemma 1.1 7B** och modeller som används via API är också lämpliga alternativ. Andra lokala modeller visar ett råd; du kan behålla modellen eller ändra den i **Settings -> Select LLM**.

I läget **Create category folders** väljer du en kategoriseringsstil:

- **More refined** for allman anvandning och finare gruppering
- **More consistent** om du vill ha starkare etikettkonsekvens mellan liknande filer

Du kan ocksa aktivera kategorivitlistor om du vill att appen ska halla sig inom en smalare uppsattning kategorinamn.

## 4. Starta analysen

Klicka pa **Analyze folder**.

Appen skannar den valda mappen, samlar den information den behover och forbereder en granskningslista.

## 5. Granska innan andringar tillampas

Granskningsdialogen later dig kontrollera:

- foreslagna kategorier
- valfria underkategorier
- namnbytesforslag for filer som stods
- slutliga destinationssokvagar
- filforhandsvisningar dar det stods

Du kan justera eller avvisa forslag innan du bekraftar nagot.

Aktivera **Dry run (preview only, do not move files)** för att förhandsgranska planerade flyttningar och namnbyten utan att genomföra dem.

## 6. Tillampa andringarna

Välj raderna du vill behandla och klicka på **Confirm and Process**. Appen skapar nödvändiga mappar och genomför de valda flyttningarna eller namnbytena.

**Auto-approve filename changes** och **Auto-approve categorization** förväljer lämpliga rader. Du måste fortfarande bekräfta innan ändringarna genomförs.

## 7. Angra senaste korningen

Om du tillampar andringar och sedan vill vanda dem, anvand **Edit -> Undo last run** fran menyn.

Angra ar utformat for den senast bekraftade sorteringskorningen. Det anvander appens registrerade korhistorik for att flytta tillbaka filer och angra namnbyten som stods nar det ar mojligt.

For basta resultat, anvand Angra innan du startar en annan stor rensning i samma mapp.

## 8. Lar fran dina granskningar

Nar du godkanner kategorier i granskningsdialogen kan appen komma ihag dessa lokala beslut och anvanda dem som tips for framtida korningar. Detta tranar inte och andrar inte AI-modellen.

De inlarda exemplen lagras i en separat lokal databas, sa att rensa den vanliga kategoriseringscachen tar inte bort dem. For att ta bort dessa lokala inlarningsdata, anvand **Settings -> Reset learned behavior**.

## Bra att veta

- Appen anvander en lokal cache for att undvika att bearbeta samma filer igen och for att forbattre konsekvensen.
- Automatiskt godkannande ar valfritt. Lat det vara av tills du ar bekvam med appens forslag.
- Bild- och dokumentalternativ kan expanderas separat om du behover mer kontroll.

## Om nagot ser fel ut

Kontrollera forst foljande:

- den valda mappen ar den du avsag
- relevanta analysalternativ ar aktiverade
- endast-byta-namn-laget begransar inte resultatet pa ett ovantat satt
- en kategorivitlista begransar inte forslag for mycket

For ytterligare felsokning, oppna **Help -> FAQ**.
