# Hurtigstartguide

AI File Sorter hjelper deg med a organisere filer etter gjennomgang og godkjenning.

AI driver analysen og foreslar kategorier, underkategorier og navn. Den berorer ikke filene dine direkte. Appen utforer bevegelser eller endrer navn forst etter at du har bekreftet de gjennomgatte endringene.

## 1. Velg en mappe

Bruk **Browse** for a velge mappen du vil sortere.

Typiske eksempler:

- `Downloads`
- en skrivebordsoppryddingsmappe
- en ekstern stasjonsmappe
- et prosjektarkiv

## 2. Velg Hva appen skal gjore

Bruk hovedalternativene for a bestemme om appen skal:

- kategorisere filer i mapper
- analysere bilder
- analysere dokumenter
- gi forslag til a endre navn for stottede filer

Hvis du bare vil ha forslag til a endre navn, aktiverer du den relevante modusen for bare a gi nytt navn.

## 3. Velg din kategoriseringsstil

Velg stilen som passer best til malet ditt:

- **More refined** for generell bruk og finere gruppering
- **More consistent** hvis du vil ha sterkere etikettkonsistens pa tvers av lignende filer

Du kan ogsa aktivere kategorihvitelister hvis du vil at appen skal holde seg innenfor et smalere sett med kategorinavn.

## 4. Start analysen

Klikk **Analyze and categorize files**.

Appen skanner den valgte mappen, samler informasjonen den trenger, og utarbeider en anmeldelsesliste.

## 5. Se gjennom for du tar i bruk endringer

Gjennomgangsdialogen lar deg inspisere:

- foreslatte kategorier
- valgfrie underkategorier
- endre navn pa forslag for stottede filer
- de endelige destinasjonsveiene

Du kan justere eller avvise forslag for du bekrefter noe.

## 6. Bruk endringene

Nar du bekrefter, oppretter appen de nodvendige mappene og utforer bevegelsene eller endre navn.

## 7. Angre siste kjoring

Hvis du bruker endringer og deretter onsker a reversere dem, bruk **Undo last run** fra menyen.

Angre er designet for den siste bekreftede sorteringskjoringen. Den bruker appens registrerte kjorehistorikk for a flytte filer tilbake og reversere stottede omdopninger der det er mulig.

For best resultat, bruk Angre for du starter en ny stor opprydding i samme mappe.

## 8. Laer av vurderingene dine

Nar du godkjenner kategorier i gjennomgangsdialogen, kan appen huske de lokale beslutningene og bruke dem som hint for fremtidige kjoringer. Dette trener eller modifiserer ikke AI-modellen.

De laerte eksemplene lagres i en egen lokal database, sa fjerning av den normale kategoriseringsbufferen fjerner dem ikke. For a fjerne disse lokale laeringsdataene, bruk **Settings -> Reset learned behavior**.

## Godt a vite

- Appen bruker en lokal hurtigbuffer for a unnga a behandle de samme filene pa nytt og for a forbedre konsistensen.
- Appen bruker ikke automatisk endringer uten a vise deg gjennomgangstrinnet forst.
- Bilde- og dokumentalternativer kan utvides separat hvis du trenger mer kontroll.

## Hvis noe ser galt ut

Sjekk folgende forst:

- den valgte mappen er den du hadde tenkt
- de relevante analysealternativene er aktivert
- Rename-only-modus begrenser ikke resultatet pa en mate du ikke forventet
- en hviteliste for kategorier begrenser ikke forslagene for mye

For ytterligere feilsoking, apne **Help -> FAQ**.
