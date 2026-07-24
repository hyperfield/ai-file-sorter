# Snabbstartguide

AI File Sorter hjalper dig att organisera filer med ett granska-forst-arbetsflode och valfritt automatiskt godkannande for andringar du valjer att lita pa.

AI:n styr analysen och foreslar kategorier, underkategorier och namn. Den andrar inte dina filer direkt. Appen flyttar eller byter namn pa filer efter att du bekraftar de granskade andringarna, eller automatiskt endast nar du har aktiverat relevant automatiskt godkannande.

## Saker forsta korning

Om det ar forsta gangen du anvander AI File Sorter, borja med en liten testmapp innan du valjer ett stort arkiv eller en hel disk.

Bra mappar for forsta korningen ar:

- en kopia av 20-50 filer fran `Downloads`
- en liten mapp for skarmbilder eller fotorensning
- en temporar mapp med nagra PDF:er eller dokument

Detta gor den forsta korningen enkel att granska. Dina filer stannar pa datorn nar du anvander lokala modeller, och AI:n foreslar bara kategorier och namn. Lat automatiskt godkannande vara av vid forsta korningen sa att inget flyttas eller byter namn innan du godkanner granskningslistan.

## 1. Valj en mapp

Anvand **Browse** eller panelen **File Explorer** for att valja mappen du vill sortera.

Typiska exempel:

- `Downloads`
- en mapp for skrivbordsrensning
- en mapp pa en extern disk
- en natverks- eller molnsynkroniserad mapp
- ett projektarkiv

## 2. Valj vad appen ska gora

Anvand huvudalternativen for att bestamma om appen ska:

- kategorisera filer i mappar
- analysera bilder
- analysera dokument
- erbjuda namnbytesforslag for filer som stods

Om du bara vill ha namnbytesforslag, aktivera relevant endast-byta-namn-lage.

## 3. Valj kategoriseringsstil

Valj den stil som passar ditt mal bast:

- **More refined** for allman anvandning och finare gruppering
- **More consistent** om du vill ha starkare etikettkonsekvens mellan liknande filer

Du kan ocksa aktivera kategorivitlistor om du vill att appen ska halla sig inom en smalare uppsattning kategorinamn.

## 4. Starta analysen

Klicka pa **Analyze and categorize files**.

Appen skannar den valda mappen, samlar den information den behover och forbereder en granskningslista.

## 5. Granska innan andringar tillampas

Granskningsdialogen later dig kontrollera:

- foreslagna kategorier
- valfria underkategorier
- namnbytesforslag for filer som stods
- slutliga destinationssokvagar
- filforhandsvisningar dar det stods

Du kan justera eller avvisa forslag innan du bekraftar nagot.

## 6. Tillampa andringarna

Nar du bekraftar skapar appen nodvandiga mappar och genomfor flyttningar eller namnbyten. Om automatiskt godkannande ar aktiverat kan giltiga kategori- eller filnamnsandringar tillampas utan att stanna vid granskningsdialogen.

## 7. Angra senaste korningen

Om du tillampar andringar och sedan vill vanda dem, anvand **Undo last run** fran menyn.

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