# Snabbstartguide

AI File Sorter hjalper dig att organisera filer efter din granskning och godkannande.

AI:n driver analysen och foreslar kategorier, underkategorier och namn. Den beror inte dina filer direkt. Appen utfor eventuella rorelser eller byter namn forst efter att du har bekraftat de granskade andringarna.

## 1. Valj en mapp

Anvand **Browse** for att valja den mapp du vill sortera.

Typiska exempel:

- `Downloads`
- en skrivbordsrensningsmapp
- en extern enhetsmapp
- ett projektarkiv

## 2. Valj vad appen ska gora

Anvand huvudalternativen for att bestamma om appen ska:

- kategorisera filer i mappar
- analysera bilder
- analysera dokument
- erbjuda forslag pa byte av filer som stods

Om du bara vill ha forslag pa byte av namn, aktivera det relevanta laget for endast byta namn.

## 3. Valj din kategoriseringsstil

Valj den stil som bast matchar ditt mal:

- **More refined** for allmant bruk och finare gruppering
- **More consistent** om du vill ha starkare etikettkonsistens over liknande filer

Du kan ocksa aktivera kategorivitlistor om du vill att appen ska halla sig inom en smalare uppsattning kategorinamn.

## 4. Starta analysen

Klicka pa **Analyze and categorize files**.

Appen skannar den valda mappen, samlar in den information den behover och forbereder en recensionslista.

## 5. Granska innan du tillampar andringar

Granskningsdialogrutan later dig inspektera:

- foreslagna kategorier
- valfria underkategorier
- byt namn pa forslag for filer som stods
- de slutliga destinationsvagarna

Du kan justera eller avvisa forslag innan du bekraftar nagot.

## 6. Tillampa andringarna

Nar du har bekraftat skapar appen de nodvandiga mapparna och utfor flyttningarna eller byter namn.

## 7. Angra den sista korningen

Om du tillampar andringar och sedan vill vanda dem, anvand **Undo last run** fran menyn.

Angra ar designat for den senaste bekraftade sorteringskorningen. Den anvander appens inspelade korhistorik for att flytta tillbaka filer och vanda om det ar mojligt med stodda namn.

For basta resultat, anvand Angra innan du paborjar en ny stor rensning i samma mapp.

## 8. Lar dig av dina recensioner

Nar du godkanner kategorier i granskningsdialogrutan kan appen komma ihag de lokala besluten och anvanda dem som tips for framtida korningar. Detta tranar eller modifierar inte AI-modellen.

De inlarda exemplen lagras i en separat lokal databas, sa att rensa den normala kategoriseringscachen tar inte bort dem. For att ta bort denna lokala inlarningsdata, anvand **Settings -> Reset learned behavior**.

## Bra att veta

- Appen anvander en lokal cache for att undvika att ombearbeta samma filer och for att forbattra konsistensen.
- Appen tillampar inte automatiskt andringar utan att visa dig granskningssteget forst.
- Bild- och dokumentalternativ kan utokas separat om du behover mer kontroll.

## Om nagot ser fel ut

Kontrollera foljande forst:

- den valda mappen ar den du tankt dig
- de relevanta analysalternativen ar aktiverade
- Rename-only-laget begransar inte resultatet pa ett satt som du inte forvantade dig
- en vitlista for kategorier begransar inte forslagen for mycket

For ytterligare felsokning, oppna **Help -> FAQ**.
