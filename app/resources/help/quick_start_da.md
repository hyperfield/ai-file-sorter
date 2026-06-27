# Hurtig startvejledning

AI File Sorter hjaelper dig med at organisere filer efter din gennemgang og godkendelse.

AI'en driver analysen og foreslar kategorier, underkategorier og navne. Det rorer ikke direkte dine filer. Appen udforer kun bevaegelser eller omdobninger, nar du har bekraeftet de gennemgaede aendringer.

## 1. Vaelg en mappe

Brug **Browse** til at vaelge den mappe, du vil sortere.

Typiske eksempler:

- `Downloads`
- en mappe til oprydning af skrivebordet
- en ekstern drevmappe
- et projektarkiv

## 2. Vaelg, hvad appen skal gore

Brug hovedmulighederne til at beslutte, om appen skal:

- kategorisere filer i mapper
- analysere billeder
- analysere dokumenter
- tilbyde omdobningsforslag til understottede filer

Hvis du kun onsker omdobningsforslag, skal du aktivere den relevante omdobningstilstand.

## 3. Vaelg din kategoriseringsstil

Vaelg den stil, der passer bedst til dit mal:

- **More refined** til generel brug og finere gruppering
- **More consistent**, hvis du onsker staerkere etiketkonsistens pa tvaers af lignende filer

Du kan ogsa aktivere kategorihvidlister, hvis du onsker, at appen skal forblive inden for et snaevrere saet kategorinavne.

## 4. Start analysen

Klik pa **Analyze and categorize files**.

Appen scanner den valgte mappe, samler de oplysninger, den har brug for, og udarbejder en anmeldelsesliste.

## 5. Gennemga, for du anvender aendringer

Gennemgangsdialogen giver dig mulighed for at inspicere:

- foreslaede kategorier
- valgfri underkategorier
- omdob forslag til understottede filer
- de endelige destinationsstier

Du kan justere eller afvise forslag, for du bekraefter noget.

## 6. Anvend aendringerne

Nar du har bekraeftet, opretter appen de nodvendige mapper og udforer flytninger eller omdobninger.

## 7. Fortryd den sidste korsel

Hvis du anvender aendringer og derefter vil vende dem, skal du bruge **Undo last run** fra menuen.

Fortryd er designet til den seneste bekraeftede sorteringskorsel. Den bruger appens registrerede korselshistorik til at flytte filer tilbage og omdobe understottede omdobninger, hvor det er muligt.

For de bedste resultater skal du bruge Fortryd, for du starter en anden stor oprydning i samme mappe.

## 8. Laer af dine anmeldelser

Nar du godkender kategorier i gennemgangsdialogen, kan appen huske disse lokale beslutninger og bruge dem som tip til fremtidige korsler. Dette traener eller modificerer ikke AI-modellen.

De laerte eksempler gemmes i en separat lokal database, sa rydning af den normale kategoriseringscache fjerner dem ikke. For at fjerne disse lokale laeringsdata skal du bruge **Settings -> Reset learned behavior**.

## Godt at vide

- Appen bruger en lokal cache for at undga genbehandling af de samme filer og for at forbedre konsistensen.
- Appen anvender ikke automatisk aendringer uden forst at vise dig gennemgangstrinnet.
- Billed- og dokumentmuligheder kan udvides separat, hvis du har brug for mere kontrol.

## Hvis noget ser forkert ud

Tjek forst folgende:

- den valgte mappe er den, du havde taenkt dig
- de relevante analysemuligheder er aktiveret
- Omdob-kun-tilstand begraenser ikke resultatet pa en made, du ikke havde forventet
- en kategorihvidliste indsnaevrer ikke forslagene for meget

For yderligere fejlfinding skal du abne **Help -> FAQ**.
