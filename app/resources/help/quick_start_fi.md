# Pika-aloitusopas

AI File Sorter auttaa järjestämään tiedostoja tarkistamalla muutokset ensin. Valinnainen automaattinen hyväksyntä voi esivalita muutoksia tarkistuslistasta.

Tekoäly ehdottaa kategorioita, kohdekansioita ja nimiä. Sovellus siirtää tai nimeää tiedostot uudelleen vasta, kun vahvistat valitut muutokset tarkistusikkunassa.

## Turvallinen ensimmainen ajo

Jos kaytat AI File Sorteria ensimmaista kertaa, aloita pienella testikansiolla ennen suuren arkiston tai aseman valitsemista.

Hyvia ensimmaisen ajon kansioita ovat:

- kopio 20-50 tiedostosta kansiosta `Downloads`
- pieni kuvakaappaus- tai valokuvien siivouskansio
- valiaikainen kansio, jossa on muutama PDF tai dokumentti

Tama pitaa ensimmaisen ajon helppona tarkistaa. Tiedostosi pysyvat tietokoneellasi, kun kaytat paikallisia malleja, ja AI vain ehdottaa kategorioita ja nimia. Jata auto-hyvaksynta pois paalta ensimmaisella ajolla, jotta mitaan ei siirreta tai nimeta uudelleen ennen kuin hyvaksytyt tarkistuslistan.

## Ota LLM käyttöön

Avaa **Settings -> Select LLM**. Valitse malli avattavasta **Local LLMs** -luettelosta tai palveluntarjoaja kohdasta **API connections**.

**Qwen3.5 4B (Recommended)** on ensimmäinen paikallinen vaihtoehto ja uusien asetusten oletusmalli. Se tarjoaa tarkkaa kategorisointia, ja sitä suositellaan olemassa oleviin kansiorakenteisiin. Aiemmin tallennettu mallivalintasi säilyy. Jos haluat käyttää omaa GGUF-tiedostoa, valitse **Custom local LLM (gguf)** ja sitten **Add**.

Jos käytät sovelluksen tarjoamaa paikallista mallia, laajenna **Downloads**, napsauta **Download** ja odota ilmoitusta **Model ready** ennen kuin vahvistat painikkeella **OK**. **Model storage** määrittää latauskansion. **See details** näyttää latausosoitteen, paikallisen polun ja tekniset tiedot.

Valitse kuva-analyysiä varten myös **Visual model** kohdasta **Image analysis models** ja lataa kaikki sen tarvitsemat tiedostot. Tämä valinta on erillinen tekstimallista.

Valitse kohdasta **API connections** Gemini, ChatGPT (OpenAI) tai oma OpenAI-yhteensopiva päätepiste ja anna tarvittavat yhteystiedot sekä mallin nimi. Pilvi-API:t vaativat internetyhteyden ja lähettävät analyysin syötteet määritettyyn palveluun. Omat päätepisteet voivat toimia myös paikallisesti.

## Tarkista paikallinen suorituskyky (valinnainen)

Kun mallit on ladattu, avaa **File -> System compatibility check** ja napsauta **Run benchmark**. Sulje ensin muut suoritinta tai grafiikkasuoritinta kuormittavat sovellukset. Tarkistus testaa ladatut paikalliset mallit, myös omat GGUF-mallit.

Jätä **Backend** asetukseen **Automatic (try CPU if GPU fails)** tai valitse **All available backends**, jos haluat vertailla suoritinta ja tuettuja grafiikkasuoritinvaihtoehtoja. Tulokset tallennetaan erikseen kullekin mallille ja taustajärjestelmälle; yhden epäonnistuminen ei sulje pois toimivaa yhdistelmää toisella.

Valitse tulos ja käytä **Retry selected** testataksesi saman yhdistelmän uudelleen, **Retry failed** testataksesi epäonnistuneet yhdistelmät uudelleen tai **Try chosen backend** testataksesi valittua mallia avattavan luettelon tietyllä taustajärjestelmällä.

Suositukset arvioivat paikallista suorituskykyä. Tarkistus ei testaa API-palveluja eikä olemassa olevien kansioiden valinnan tarkkuutta, eikä se vaihda valittua mallia automaattisesti.

## 1. Valitse kansio

Valitse jarjestettava kansio kayttamalla **Browse**-painiketta tai **File Explorer** -paneelia.

Tyypillisia esimerkkeja:

- `Downloads`
- tyopoydan siivouskansio
- ulkoisen aseman kansio
- verkko- tai pilvisynkronoitu kansio
- projektiarkisto

**Destination** määrittää lajiteltujen tiedostojen kohteen. Pidä **Use analyzed folder** käytössä, jos haluat lajitella valitun kansion sisällä, tai poista se käytöstä ja valitse toinen kohde.

## 2. Valitse mita sovelluksen tulee tehda

Paavaihtoehdoilla paatat, tuleeko sovelluksen:

- lajitella tiedostot kategoriakansioihin
- analysoida kuvia
- analysoida dokumentteja
- tarjota uudelleennimeamisehdotuksia tuetuille tiedostoille

Jos haluat vain uudelleennimeamisehdotuksia, ota kayttoon vastaava vain uudelleennimeaminen -tila.

## 3. Valitse lajittelutapa

Valitse lajittelutila:

- **Create category folders** luo kansioita ehdotetuista kategorianimistä.
- **Use existing folder structure** valitsee kansioita **Destination**-kohteen kansiopuusta. Ota **Suggest new folders when needed** käyttöön salliaksesi uudet kohteet, kun mikään kansio ei sovi hyvin; jätä se pois käytöstä käyttääksesi vain olemassa olevia kansioita.

Suosittelemme olemassa oleviin kansioihin mallia **Qwen3.5 4B**. Myös **Gemma 1.1 7B** ja API:n kautta käytetyt mallit sopivat tähän. Muut paikalliset mallit näyttävät ohjeen; voit pitää mallisi tai vaihtaa sen kohdassa **Settings -> Select LLM**.

Valitse **Create category folders** -tilassa kategorisointityyli:

- **More refined** yleiseen kayttoon ja tarkempaan ryhmittelyyn
- **More consistent**, jos haluat vahvempaa nimikkeiden yhdenmukaisuutta samankaltaisille tiedostoille

Voit myos ottaa kayttoon kategoriavalkolistat, jos haluat sovelluksen pysyvan rajatummassa kategorianimien joukossa.

## 4. Aloita analyysi

Napsauta **Analyze folder**.

Sovellus skannaa valitun kansion, keraa tarvitsemansa tiedot ja valmistelee tarkistuslistan.

## 5. Tarkista ennen muutosten kayttoonottoa

Tarkistusikkunassa voit tarkistaa:

- ehdotetut kategoriat
- valinnaiset alakategoriat
- uudelleennimeamisehdotukset tuetuille tiedostoille
- lopulliset kohdepolut
- tiedostojen esikatselut, jos niita tuetaan

Voit saataa tai hylata ehdotuksia ennen minkaan vahvistamista.

Ota **Dry run (preview only, do not move files)** käyttöön esikatsellaksesi suunnitellut siirrot ja uudelleennimeämiset tekemättä niitä.

## 6. Ota muutokset kayttoon

Valitse käsiteltävät rivit ja napsauta **Confirm and Process**. Sovellus luo tarvittavat kansiot ja suorittaa valitut siirrot tai uudelleennimeämiset.

**Auto-approve filename changes** ja **Auto-approve categorization** esivalitsevat sopivat rivit. Sinun on silti vahvistettava ennen muutosten tekemistä.

## 7. Kumoa viimeisin ajo

Jos otat muutokset kayttoon ja haluat peruuttaa ne, kayta valikosta **Edit -> Undo last run**.

Kumoaminen on tarkoitettu viimeisimmalle vahvistetulle lajitteluajolle. Se kayttaa sovelluksen tallentamaa ajohistoriaa tiedostojen palauttamiseen ja tuettujen uudelleennimeamisten peruuttamiseen mahdollisuuksien mukaan.

Parhaan tuloksen saat, kun kaytat Kumoa-toimintoa ennen uuden suuren siivouksen aloittamista samassa kansiossa.

## 8. Oppiminen tarkistuksistasi

Kun hyvaksytyt kategorioita tarkistusikkunassa, sovellus voi muistaa nama paikalliset paatokset ja kayttaa niita vihjeina tulevissa ajoissa. Tama ei kouluta tai muuta AI-mallia.

Opitut esimerkit tallennetaan erilliseen paikalliseen tietokantaan, joten tavallisen kategorisointivalimuistin tyhjentaminen ei poista niita. Poista nama paikalliset oppimistiedot kayttamalla **Settings -> Reset learned behavior**.

## Hyva tietaa

- Sovellus kayttaa paikallista valimuistia, jotta samoja tiedostoja ei tarvitse kasitella uudelleen ja yhdenmukaisuus paranee.
- Auto-hyvaksynta on valinnainen. Jata se pois paalta, kunnes luotat sovelluksen ehdotuksiin.
- Kuva- ja dokumenttiasetukset voi laajentaa erikseen, jos tarvitset enemman hallintaa.

## Jos jokin nayttaa vaaralta

Tarkista ensin seuraavat:

- valittu kansio on se, jonka tarkoitit
- tarvittavat analyysiasetukset ovat kaytossa
- vain uudelleennimeaminen -tila ei rajoita tulosta odottamattomalla tavalla
- kategoriavalkolista ei rajaa ehdotuksia liikaa

Lisatietoja vianmaaritykseen saat avaamalla **Help -> FAQ**.
