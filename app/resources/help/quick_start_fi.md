# Pika-aloitusopas

AI File Sorter auttaa jarjestamaan tiedostoja tarkistus ensin -tyonkululla ja valinnaisella automaattisella hyvaksyntaa varten niille muutoksille, joihin haluat luottaa.

AI ohjaa analyysia ja ehdottaa kategorioita, alakategorioita ja nimia. Se ei muuta tiedostojasi suoraan. Sovellus siirtaa tai nimeaa tiedostoja uudelleen, kun vahvistat tarkistetut muutokset, tai automaattisesti vain silloin, kun vastaava auto-hyvaksynta on otettu kayttoon.

## Turvallinen ensimmainen ajo

Jos kaytat AI File Sorteria ensimmaista kertaa, aloita pienella testikansiolla ennen suuren arkiston tai aseman valitsemista.

Hyvia ensimmaisen ajon kansioita ovat:

- kopio 20-50 tiedostosta kansiosta `Downloads`
- pieni kuvakaappaus- tai valokuvien siivouskansio
- valiaikainen kansio, jossa on muutama PDF tai dokumentti

Tama pitaa ensimmaisen ajon helppona tarkistaa. Tiedostosi pysyvat tietokoneellasi, kun kaytat paikallisia malleja, ja AI vain ehdottaa kategorioita ja nimia. Jata auto-hyvaksynta pois paalta ensimmaisella ajolla, jotta mitaan ei siirreta tai nimeta uudelleen ennen kuin hyvaksytyt tarkistuslistan.

## 1. Valitse kansio

Valitse jarjestettava kansio kayttamalla **Browse**-painiketta tai **File Explorer** -paneelia.

Tyypillisia esimerkkeja:

- `Downloads`
- tyopoydan siivouskansio
- ulkoisen aseman kansio
- verkko- tai pilvisynkronoitu kansio
- projektiarkisto

## 2. Valitse mita sovelluksen tulee tehda

Paavaihtoehdoilla paatat, tuleeko sovelluksen:

- lajitella tiedostot kategoriakansioihin
- analysoida kuvia
- analysoida dokumentteja
- tarjota uudelleennimeamisehdotuksia tuetuille tiedostoille

Jos haluat vain uudelleennimeamisehdotuksia, ota kayttoon vastaava vain uudelleennimeaminen -tila.

## 3. Valitse kategorisointityyli

Valitse tyyli, joka sopii parhaiten tavoitteeseesi:

- **More refined** yleiseen kayttoon ja tarkempaan ryhmittelyyn
- **More consistent**, jos haluat vahvempaa nimikkeiden yhdenmukaisuutta samankaltaisille tiedostoille

Voit myos ottaa kayttoon kategoriavalkolistat, jos haluat sovelluksen pysyvan rajatummassa kategorianimien joukossa.

## 4. Aloita analyysi

Napsauta **Analyze and categorize files**.

Sovellus skannaa valitun kansion, keraa tarvitsemansa tiedot ja valmistelee tarkistuslistan.

## 5. Tarkista ennen muutosten kayttoonottoa

Tarkistusikkunassa voit tarkistaa:

- ehdotetut kategoriat
- valinnaiset alakategoriat
- uudelleennimeamisehdotukset tuetuille tiedostoille
- lopulliset kohdepolut
- tiedostojen esikatselut, jos niita tuetaan

Voit saataa tai hylata ehdotuksia ennen minkaan vahvistamista.

## 6. Ota muutokset kayttoon

Kun vahvistat, sovellus luo tarvittavat kansiot ja tekee siirrot tai uudelleennimeamiset. Jos auto-hyvaksynta on kaytossa, kelvolliset kategoria- tai tiedostonimimuutokset voidaan ottaa kayttoon pysahtymatta tarkistusikkunaan.

## 7. Kumoa viimeisin ajo

Jos otat muutokset kayttoon ja haluat peruuttaa ne, kayta valikosta **Undo last run**.

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