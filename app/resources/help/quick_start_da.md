# Hurtig startvejledning

AI File Sorter hjælper dig med at organisere filer med gennemgang, før ændringer udføres. Valgfri automatisk godkendelse kan forhåndsvælge ændringer i gennemgangslisten.

AI'en foreslår kategorier, destinationsmapper og navne. Appen flytter eller omdøber først filer, når du har bekræftet de valgte ændringer i gennemgangsdialogen.

## Sikker forste korsel

Hvis det er forste gang, du bruger AI File Sorter, skal du starte med en lille testmappe, for du bruger en stor samling eller et helt drev.

Gode mapper til forste korsel er:

- en kopi af 20-50 filer fra `Downloads`
- en lille mappe til screenshots eller fotooprydning
- en midlertidig mappe med nogle fa PDF'er eller dokumenter

Det gor den forste korsel nem at gennemga. Dine filer bliver pa computeren, nar du bruger lokale modeller, og AI'en foreslar kun kategorier og navne. Lad auto-godkendelse vaere slaet fra ved forste korsel, sa intet flyttes eller omdoebes, for du godkender gennemgangslisten.

## Opsæt en LLM

Åbn **Settings -> Select LLM**. Vælg en model fra rullelisten **Local LLMs** eller en udbyder under **API connections**.

**Qwen3.5 4B (Recommended)** er den første lokale mulighed og standarden for nye opsætninger. Modellen giver høj præcision ved kategorisering og anbefales til eksisterende mappestrukturer. Dit tidligere gemte modelvalg bevares. Vælg **Custom local LLM (gguf)** og brug **Add** for at bruge din egen GGUF-fil.

For en lokal model, som appen tilbyder, udvider du **Downloads**, klikker på **Download** og venter på **Model ready**, før du bekræfter med **OK**. Med **Model storage** vælger du downloadmappen. **See details** viser downloadadressen, den lokale sti og tekniske oplysninger.

Til billedanalyse skal du også vælge en **Visual model** under **Image analysis models** og downloade alle nødvendige filer. Dette valg er adskilt fra tekstmodellen.

Under **API connections** vælger du Gemini, ChatGPT (OpenAI) eller et brugerdefineret OpenAI-kompatibelt endepunkt og indtaster de nødvendige forbindelsesoplysninger og modelnavnet. Cloud-API'er kræver internet og sender analysegrundlaget til den konfigurerede tjeneste. Brugerdefinerede endepunkter kan også køre lokalt.

## Kontroller lokal ydeevne (valgfrit)

Når modellerne er downloadet, åbner du **File -> System compatibility check** og klikker på **Run benchmark**. Luk først andre apps, der belaster CPU eller GPU meget. Kontrollen tester downloadede lokale modeller, herunder brugerdefinerede GGUF-modeller.

Lad **Backend** stå på **Automatic (try CPU if GPU fails)**, eller vælg **All available backends** for at sammenligne CPU og understøttede GPU-muligheder. Resultater gemmes separat for hver model og backend; en fejl på én backend udelukker ikke en fungerende kombination på en anden.

Vælg et resultat, og brug **Retry selected** til at teste den kombination igen, **Retry failed** til at teste mislykkede kombinationer igen eller **Try chosen backend** til at teste den valgte model med en bestemt backend fra rullelisten.

Anbefalingerne anslår lokal ydeevne. Kontrollen tester hverken API-tjenester eller nøjagtigheden ved valg af eksisterende mapper og ændrer ikke din valgte model automatisk.

## 1. Vaelg en mappe

Brug **Browse** eller **File Explorer**-ruden til at vaelge den mappe, du vil sortere.

Typiske eksempler:

- `Downloads`
- en mappe til skrivebordsoprydning
- en mappe pa et eksternt drev
- en netvaerks- eller cloudsynkroniseret mappe
- et projektarkiv

**Destination** bestemmer, hvor sorterede filer ender. Lad **Use analyzed folder** være aktiveret for at sortere i den valgte mappe, eller slå det fra og vælg en anden destination.

## 2. Vaelg, hvad appen skal gore

Brug hovedmulighederne til at beslutte, om appen skal:

- kategorisere filer i mapper
- analysere billeder
- analysere dokumenter
- tilbyde omdoebningsforslag til understottede filer

Hvis du kun vil have omdoebningsforslag, skal du aktivere den relevante kun-omdoebningstilstand.

## 3. Vælg hvordan filerne skal sorteres

Vælg en sorteringstilstand:

- **Create category folders** opretter mapper ud fra foreslåede kategorinavne.
- **Use existing folder structure** vælger mapper i mappetræet under **Destination**. Aktivér **Suggest new folders when needed** for at tillade nye destinationer, når ingen passer godt; lad det være slået fra for kun at bruge eksisterende mapper.

Til eksisterende mapper anbefaler vi **Qwen3.5 4B**. **Gemma 1.1 7B** og modeller, der bruges via en API, er også egnede muligheder. Andre lokale modeller viser et råd; du kan beholde modellen eller ændre den i **Settings -> Select LLM**.

I tilstanden **Create category folders** vælger du en kategoriseringsstil:

- **More refined** til almindelig brug og finere gruppering
- **More consistent**, hvis du vil have staerkere etiketkonsistens pa tvaers af lignende filer

Du kan ogsa aktivere kategorihvidlister, hvis appen skal holde sig inden for et snaevrere saet kategorinavne.

## 4. Start analysen

Klik pa **Analyze folder**.

Appen scanner den valgte mappe, indsamler de oplysninger, den har brug for, og forbereder en gennemgangsliste.

## 5. Gennemga for aendringer anvendes

Gennemgangsdialogen lader dig kontrollere:

- foreslaede kategorier
- valgfrie underkategorier
- omdoebningsforslag til understottede filer
- de endelige destinationsstier
- filforhandsvisninger hvor det understottes

Du kan justere eller afvise forslag, for du bekraefter noget.

Aktivér **Dry run (preview only, do not move files)** for at se de planlagte flytninger og omdøbninger uden at udføre dem.

## 6. Anvend aendringerne

Vælg de rækker, du vil behandle, og klik på **Confirm and Process**. Appen opretter de nødvendige mapper og udfører de valgte flytninger eller omdøbninger.

**Auto-approve filename changes** og **Auto-approve categorization** forhåndsvælger egnede rækker. Du skal stadig bekræfte, før ændringerne udføres.

## 7. Fortryd den sidste korsel

Hvis du anvender aendringer og derefter vil vende dem, skal du bruge **Edit -> Undo last run** fra menuen.

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
