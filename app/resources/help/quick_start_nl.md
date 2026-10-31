# Snelstartgids

AI File Sorter helpt u bestanden te organiseren door wijzigingen eerst te controleren. Optionele automatische goedkeuring kan wijzigingen vooraf selecteren in de controlelijst.

De AI stelt categorieën, doelmappen en namen voor. De app verplaatst of hernoemt bestanden pas nadat u de geselecteerde wijzigingen in het controledialoogvenster bevestigt.

## Veilige eerste run

Als u AI File Sorter voor het eerst gebruikt, begin dan met een kleine testmap voordat u een groot archief of een hele schijf kiest.

Goede mappen voor een eerste run zijn:

- een kopie van 20-50 bestanden uit `Downloads`
- een kleine map met screenshots of foto's om op te ruimen
- een tijdelijke map met enkele PDF's of documenten

Zo blijft de eerste run makkelijk te controleren. Uw bestanden blijven op uw computer wanneer u lokale modellen gebruikt, en de AI stelt alleen categorieen en namen voor. Laat automatische goedkeuring uit bij de eerste run, zodat niets wordt verplaatst of hernoemd voordat u de controlelijst goedkeurt.

## Een LLM instellen

Open **Settings -> Select LLM**. Kies een model uit de vervolgkeuzelijst **Local LLMs** of een aanbieder onder **API connections**.

**Qwen3.5 4B (Recommended)** is de eerste lokale optie en de standaard voor nieuwe installaties. Het biedt hoge nauwkeurigheid bij categorisatie en wordt aanbevolen voor bestaande mappenstructuren. Uw eerder opgeslagen modelkeuze blijft behouden. Kies voor uw eigen GGUF-bestand **Custom local LLM (gguf)** en gebruik **Add**.

Voor een lokaal model dat de app aanbiedt, vouwt u **Downloads** uit, klikt u op **Download** en wacht u op **Model ready** voordat u bevestigt met **OK**. Met **Model storage** kiest u de downloadmap. **See details** toont de download-URL, het lokale pad en technische informatie.

Kies voor beeldanalyse ook een **Visual model** onder **Image analysis models** en download alle benodigde bestanden. Deze keuze staat los van het tekstmodel.

Kies onder **API connections** Gemini, ChatGPT (OpenAI) of een eigen OpenAI-compatibel eindpunt en voer de vereiste verbindingsgegevens en modelnaam in. Cloud-API's vereisen internet en sturen analysegegevens naar de ingestelde dienst. Eigen eindpunten kunnen ook lokaal draaien.

## Lokale prestaties controleren (optioneel)

Open na het downloaden van modellen **File -> System compatibility check** en klik op **Run benchmark**. Sluit eerst andere apps die de CPU of GPU zwaar belasten. De controle test gedownloade lokale modellen, inclusief eigen GGUF-modellen.

Laat **Backend** op **Automatic (try CPU if GPU fails)** staan of kies **All available backends** om de CPU en ondersteunde GPU-opties te vergelijken. Resultaten worden apart opgeslagen voor elk model en elke backend; een fout op de ene sluit een werkende combinatie op een andere niet uit.

Selecteer een resultaat en gebruik **Retry selected** om die combinatie opnieuw te testen, **Retry failed** om mislukte combinaties opnieuw te testen of **Try chosen backend** om het geselecteerde model met een bepaalde backend uit de vervolgkeuzelijst te testen.

Aanbevelingen schatten de lokale prestaties. De controle test geen API-diensten of nauwkeurigheid bij het kiezen van bestaande mappen en wijzigt uw gekozen model niet automatisch.

## 1. Kies een map

Gebruik **Browse** of het paneel **File Explorer** om de map te kiezen die u wilt sorteren.

Typische voorbeelden:

- `Downloads`
- een map om het bureaublad op te ruimen
- een map op een externe schijf
- een netwerkmap of cloud-gesynchroniseerde map
- een projectarchief

**Destination** bepaalt waar gesorteerde bestanden terechtkomen. Laat **Use analyzed folder** ingeschakeld om binnen de geselecteerde map te sorteren, of schakel het uit en kies een andere bestemming.

## 2. Kies wat de app moet doen

Gebruik de hoofdopties om te bepalen of de app moet:

- bestanden in categoriemappen plaatsen
- afbeeldingen analyseren
- documenten analyseren
- hernoemsuggesties geven voor ondersteunde bestanden

Als u alleen hernoemsuggesties wilt, schakel dan de relevante alleen-hernoemenmodus in.

## 3. Kiezen hoe u wilt sorteren

Kies een sorteermodus:

- **Create category folders** maakt mappen op basis van voorgestelde categorienamen.
- **Use existing folder structure** kiest mappen binnen de mappenstructuur van **Destination**. Schakel **Suggest new folders when needed** in om nieuwe bestemmingen toe te staan als geen bestaande map goed past; laat het uit om alleen bestaande mappen te gebruiken.

Voor bestaande mappen bevelen we **Qwen3.5 4B** aan. **Gemma 1.1 7B** en modellen die via een API worden gebruikt, zijn ook geschikte opties. Andere lokale modellen tonen een advies; u kunt uw model behouden of wijzigen in **Settings -> Select LLM**.

Kies in de modus **Create category folders** een categorisatiestijl:

- **More refined** voor algemeen gebruik en fijnere groepering
- **More consistent** als u sterkere labelconsistentie wilt tussen vergelijkbare bestanden

U kunt ook categorie-whitelists inschakelen als u wilt dat de app binnen een smallere set categorienamen blijft.

## 4. Start de analyse

Klik op **Analyze folder**.

De app scant de geselecteerde map, verzamelt de benodigde informatie en bereidt een controlelijst voor.

## 5. Controleer voordat u wijzigingen toepast

In het controledialoogvenster kunt u bekijken:

- voorgestelde categorieen
- optionele subcategorieen
- hernoemsuggesties voor ondersteunde bestanden
- de uiteindelijke doelpaden
- bestandsvoorbeelden waar ondersteund

U kunt suggesties aanpassen of afwijzen voordat u iets bevestigt.

Schakel **Dry run (preview only, do not move files)** in om geplande verplaatsingen en hernoemingen te bekijken zonder ze toe te passen.

## 6. Pas de wijzigingen toe

Selecteer de rijen die u wilt verwerken en klik op **Confirm and Process**. De app maakt de benodigde mappen en voert de geselecteerde verplaatsingen of hernoemingen uit.

**Auto-approve filename changes** en **Auto-approve categorization** selecteren geschikte rijen vooraf. U moet nog steeds bevestigen voordat die wijzigingen worden toegepast.

## 7. Maak de laatste run ongedaan

Als u wijzigingen toepast en ze daarna wilt terugdraaien, gebruikt u **Edit -> Undo last run** in het menu.

Ongedaan maken is bedoeld voor de meest recente bevestigde sorteerrun. Het gebruikt de door de app opgeslagen runhistorie om bestanden terug te plaatsen en ondersteunde hernoemingen waar mogelijk terug te draaien.

Gebruik Ongedaan maken bij voorkeur voordat u een andere grote opruiming in dezelfde map start.

## 8. Leren van uw controles

Wanneer u categorieen goedkeurt in het controledialoogvenster, kan de app deze lokale beslissingen onthouden en gebruiken als hints voor toekomstige runs. Dit traint of wijzigt het AI-model niet.

De geleerde voorbeelden worden opgeslagen in een aparte lokale database, dus het wissen van de normale categorisatiecache verwijdert ze niet. Gebruik **Settings -> Reset learned behavior** om deze lokale leergegevens te verwijderen.

## Goed om te weten

- De app gebruikt een lokale cache om dezelfde bestanden niet opnieuw te verwerken en de consistentie te verbeteren.
- Automatische goedkeuring is optioneel. Laat dit uit totdat u vertrouwd bent met de suggesties van de app.
- Afbeeldings- en documentopties kunnen afzonderlijk worden uitgeklapt als u meer controle nodig hebt.

## Als iets er verkeerd uitziet

Controleer eerst het volgende:

- de geselecteerde map is de map die u bedoelde
- de relevante analyseopties zijn ingeschakeld
- de alleen-hernoemenmodus beperkt het resultaat niet onverwacht
- een categorie-whitelist beperkt de suggesties niet te veel

Open **Help -> FAQ** voor extra probleemoplossing.
