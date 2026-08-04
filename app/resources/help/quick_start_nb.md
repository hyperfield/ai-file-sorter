# Hurtigstartguide

AI File Sorter hjelper deg med a organisere filer med en gjennomgang-forst arbeidsflyt og valgfri automatisk godkjenning for endringer du velger a stole pa.

AI-en styrer analysen og foreslar kategorier, underkategorier og navn. Den endrer ikke filene dine direkte. Appen flytter eller gir filer nytt navn etter at du bekrefter de gjennomgatte endringene, eller automatisk bare nar du har aktivert den relevante auto-godkjenningen.

## Sikker forste kjoring

Hvis dette er forste gang du bruker AI File Sorter, start med en liten testmappe for du peker den mot et stort arkiv eller en hel disk.

Gode mapper for forste kjoring er:

- en kopi av 20-50 filer fra `Downloads`
- en liten mappe for skjermbilder eller fotoopprydding
- en midlertidig mappe med noen PDF-er eller dokumenter

Dette gjor den forste kjoringen enkel a gjennomga. Filene dine blir pa datamaskinen nar du bruker lokale modeller, og AI-en foreslar bare kategorier og navn. La auto-godkjenning vaere av ved forste kjoring, slik at ingenting flyttes eller gis nytt navn for du godkjenner gjennomgangslisten.

## 1. Velg en mappe

Bruk **Browse** eller **File Explorer**-panelet til a velge mappen du vil sortere.

Typiske eksempler:

- `Downloads`
- en mappe for skrivebordsopprydding
- en mappe pa en ekstern disk
- en nettverks- eller skysynkronisert mappe
- et prosjektarkiv

## 2. Velg hva appen skal gjore

Bruk hovedalternativene til a bestemme om appen skal:

- kategorisere filer i mapper
- analysere bilder
- analysere dokumenter
- tilby forslag til nytt navn for stottede filer

Hvis du bare vil ha forslag til nytt navn, aktiverer du den relevante bare-gi-nytt-navn-modusen.

## 3. Velg kategoriseringsstil

Velg stilen som passer best til malet ditt:

- **More refined** for generell bruk og finere gruppering
- **More consistent** hvis du vil ha sterkere etikettkonsistens pa tvers av lignende filer

Du kan ogsa aktivere kategorihvitelister hvis du vil at appen skal holde seg innenfor et smalere sett med kategorinavn.

## 4. Start analysen

Klikk **Analyze and categorize files**.

Appen skanner den valgte mappen, samler inn informasjonen den trenger, og forbereder en gjennomgangsliste.

## 5. Gjennomga for du bruker endringer

Gjennomgangsdialogen lar deg kontrollere:

- foreslatte kategorier
- valgfrie underkategorier
- forslag til nytt navn for stottede filer
- endelige malstier
- filforhandsvisninger der det stottes

Du kan justere eller avvise forslag for du bekrefter noe.

## 6. Bruk endringene

Nar du bekrefter, oppretter appen de nodvendige mappene og utforer flyttingene eller navneendringene. Hvis auto-godkjenning er aktivert, kan kvalifiserte kategori- eller filnavnendringer brukes uten a stoppe ved gjennomgangsdialogen.

## 7. Angre siste kjoring

Hvis du bruker endringer og deretter onsker a reversere dem, bruk **Undo last run** fra menyen.

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