# Hurtig startvejledning

AI File Sorter hjaelper dig med at organisere filer med en gennemgang-forst-arbejdsgang og valgfri automatisk godkendelse af aendringer, du vaelger at stole paa.

AI'en styrer analysen og foreslar kategorier, underkategorier og navne. Den aendrer ikke dine filer direkte. Appen flytter eller omdoeber filer, nar du bekraefter de gennemgaede aendringer, eller automatisk kun nar du har slaet den relevante auto-godkendelse til.

## Sikker forste korsel

Hvis det er forste gang, du bruger AI File Sorter, skal du starte med en lille testmappe, for du bruger en stor samling eller et helt drev.

Gode mapper til forste korsel er:

- en kopi af 20-50 filer fra `Downloads`
- en lille mappe til screenshots eller fotooprydning
- en midlertidig mappe med nogle fa PDF'er eller dokumenter

Det gor den forste korsel nem at gennemga. Dine filer bliver pa computeren, nar du bruger lokale modeller, og AI'en foreslar kun kategorier og navne. Lad auto-godkendelse vaere slaet fra ved forste korsel, sa intet flyttes eller omdoebes, for du godkender gennemgangslisten.

## 1. Vaelg en mappe

Brug **Browse** eller **File Explorer**-ruden til at vaelge den mappe, du vil sortere.

Typiske eksempler:

- `Downloads`
- en mappe til skrivebordsoprydning
- en mappe pa et eksternt drev
- en netvaerks- eller cloudsynkroniseret mappe
- et projektarkiv

## 2. Vaelg, hvad appen skal gore

Brug hovedmulighederne til at beslutte, om appen skal:

- kategorisere filer i mapper
- analysere billeder
- analysere dokumenter
- tilbyde omdoebningsforslag til understottede filer

Hvis du kun vil have omdoebningsforslag, skal du aktivere den relevante kun-omdoebningstilstand.

## 3. Vaelg din kategoriseringsstil

Vaelg den stil, der passer bedst til dit mal:

- **More refined** til almindelig brug og finere gruppering
- **More consistent**, hvis du vil have staerkere etiketkonsistens pa tvaers af lignende filer

Du kan ogsa aktivere kategorihvidlister, hvis appen skal holde sig inden for et snaevrere saet kategorinavne.

## 4. Start analysen

Klik pa **Analyze and categorize files**.

Appen scanner den valgte mappe, indsamler de oplysninger, den har brug for, og forbereder en gennemgangsliste.

## 5. Gennemga for aendringer anvendes

Gennemgangsdialogen lader dig kontrollere:

- foreslaede kategorier
- valgfrie underkategorier
- omdoebningsforslag til understottede filer
- de endelige destinationsstier
- filforhandsvisninger hvor det understottes

Du kan justere eller afvise forslag, for du bekraefter noget.

## 6. Anvend aendringerne

Nar du bekraefter, opretter appen de nodvendige mapper og udforer flytninger eller omdoebninger. Hvis auto-godkendelse er aktiveret, kan egnede kategori- eller filnavnsaendringer anvendes uden at stoppe ved gennemgangsdialogen.

## 7. Fortryd den sidste korsel

Hvis du anvender aendringer og derefter vil vende dem, skal du bruge **Undo last run** fra menuen.

Fortryd er beregnet til den seneste bekraeftede sorteringskorsel. Den bruger appens registrerede korselshistorik til at flytte filer tilbage og fortryde understottede omdoebninger, hvor det er muligt.

For de bedste resultater skal du bruge Fortryd, for du starter en anden stor oprydning i samme mappe.

## 8. Laer af dine gennemgange

Nar du godkender kategorier i gennemgangsdialogen, kan appen huske disse lokale beslutninger og bruge dem som tip til fremtidige korsler. Dette traener eller aendrer ikke AI-modellen.

De laerte eksempler gemmes i en separat lokal database, sa rydning af den normale kategoriseringscache fjerner dem ikke. Brug **Settings -> Reset learned behavior** for at fjerne disse lokale laeringsdata.

## Godt at vide

- Appen bruger en lokal cache for at undga at behandle de samme filer igen og for at forbedre konsistensen.
- Auto-godkendelse er valgfri. Lad den vaere slaet fra, indtil du er tryg ved appens forslag.
- Billed- og dokumentmuligheder kan udvides separat, hvis du har brug for mere kontrol.

## Hvis noget ser forkert ud

Tjek forst folgende:

- den valgte mappe er den, du havde taenkt dig
- de relevante analysemuligheder er aktiveret
- kun-omdoebningstilstand begraenser ikke resultatet pa en made, du ikke forventede
- en kategorihvidliste indsnaevrer ikke forslagene for meget

For yderligere fejlfinding skal du abne **Help -> FAQ**.