# Flytileidarvisir

AI File Sorter hjálpar þér að skipuleggja skrár með yfirferð áður en breytingar eru framkvæmdar. Valfrjálst sjálfvirkt samþykki getur forvalið breytingar á yfirferðarlistanum.

Gervigreindin leggur til flokka, áfangamöppur og nöfn. Forritið flytur eða endurnefnir skrár aðeins eftir að þú staðfestir valdar breytingar í yfirferðarglugganum.

## Orugg fyrsta keyrsla

Ef thu ert ad nota AI File Sorter i fyrsta sinn skaltu byrja med litla profunarmoppu adur en thu velur stort safn eda drif.

Godar moppur fyrir fyrstu keyrslu eru:

- afrit af 20-50 skram ur `Downloads`
- litil skjaskota- eda myndahreinsunarmappa
- timabundin mappa med nokkrum PDF-skjolum eda skjolum

Thetta gerir fyrstu keyrsluna audvelt ad yfirfara. Skrarnar thinar verda afram a tolvu thinri thegar thu notar stadbundin likon, og AI leggur adeins til flokka og nofn. Hafdu sjalfvirka samthykkt slokkta i fyrstu keyrslu svo ekkert se flutt eda endurnefnt fyrr en thu samthykkir yfirferdarlistann.

## Settu upp LLM

Opnaðu **Settings -> Select LLM**. Veldu líkan úr fellilistanum **Local LLMs** eða þjónustuaðila undir **API connections**.

**Qwen3.5 4B (Recommended)** er fyrsti staðbundni valkosturinn og sjálfgefið líkan við nýja uppsetningu. Það býður upp á mikla nákvæmni við flokkun og er ráðlagt fyrir fyrirliggjandi möppuskipan. Fyrra vistað líkanaval þitt helst. Til að nota eigin GGUF-skrá skaltu velja **Custom local LLM (gguf)** og síðan **Add**.

Fyrir staðbundið líkan sem forritið býður upp á skaltu opna **Downloads**, smella á **Download** og bíða eftir **Model ready** áður en þú staðfestir með **OK**. **Model storage** gerir þér kleift að velja niðurhalsmöppu. **See details** sýnir niðurhalsslóð, staðbundna skráarslóð og tæknilegar upplýsingar.

Fyrir myndgreiningu skaltu einnig velja **Visual model** undir **Image analysis models** og sækja allar nauðsynlegar skrár þess. Þetta val er óháð textalíkaninu.

Undir **API connections** skaltu velja Gemini, ChatGPT (OpenAI) eða eigin OpenAI-samhæfðan endapunkt og slá inn nauðsynlegar tengingarupplýsingar og líkananafn. Skýja-API krefjast internets og senda greiningarinntak til stilltrar þjónustu. Eigin endapunktar geta líka keyrt staðbundið.

## Athugaðu staðbundin afköst (valfrjálst)

Eftir að líkön hafa verið sótt skaltu opna **File -> System compatibility check** og smella á **Run benchmark**. Lokaðu fyrst öðrum forritum sem nota mikið af örgjörva eða skjákorti. Athugunin prófar sótt staðbundin líkön, þar á meðal eigin GGUF-líkön.

Haltu **Backend** á **Automatic (try CPU if GPU fails)** eða veldu **All available backends** til að bera saman örgjörva og studda skjákortsvalkosti. Niðurstöður eru vistaðar sérstaklega fyrir hvert líkan og bakenda; bilun á einum útilokar ekki samsetningu sem virkar á öðrum.

Veldu niðurstöðu og notaðu **Retry selected** til að endurtaka þá samsetningu, **Retry failed** til að endurtaka samsetningar sem mistókust eða **Try chosen backend** til að prófa valið líkan með tilteknum bakenda úr fellilistanum.

Ráðleggingarnar áætla staðbundin afköst. Athugunin prófar hvorki API-þjónustur né nákvæmni við val á fyrirliggjandi möppum og breytir ekki völdu líkani sjálfkrafa.

## 1. Veldu moppu

Notadu **Browse** eda **File Explorer**-gluggann til ad velja moppuna sem thu vilt rada.

Daeigerd daemi:

- `Downloads`
- mappa fyrir skjabordshreinsun
- mappa a ytra drifi
- net- eda skysamstillt mappa
- verkefnasafn

**Destination** ákveður hvert flokkaðar skrár fara. Hafðu **Use analyzed folder** virkt til að flokka innan valinnar möppu, eða slökktu á því og veldu annan áfangastað.

## 2. Veldu hvad forritid a ad gera

Notadu adalvalkostina til ad akveda hvort forritid eigi ad:

- flokka skrar i moppur
- greina myndir
- greina skjol
- bjoda upp a endurnefningartillogur fyrir studdar skrar

Ef thu vilt adeins endurnefningartillogur skaltu virkja vidkomandi adeins-endurnefna ham.

## 3. Veldu hvernig á að flokka

Veldu flokkunarham:

- **Create category folders** býr til möppur úr tillögum að flokkanöfnum.
- **Use existing folder structure** velur möppur innan möpputrésins undir **Destination**. Virkjaðu **Suggest new folders when needed** til að leyfa nýja áfangastaði þegar enginn passar vel; hafðu það óvirkt til að nota aðeins fyrirliggjandi möppur.

Fyrir fyrirliggjandi möppur mælum við með **Qwen3.5 4B**. **Gemma 1.1 7B** og líkön sem notuð eru í gegnum API eru einnig hentugir valkostir. Önnur staðbundin líkön sýna ábendingu; þú getur haldið líkaninu eða breytt því í **Settings -> Select LLM**.

Í hamnum **Create category folders** skaltu velja flokkunarstíl:

- **More refined** fyrir almenna notkun og finni hopun
- **More consistent** ef thu vilt meiri samraemingu merkja a milli likra skra

Thu getur einnig virkjad flokkaleyfilista ef thu vilt ad forritid haldi sig innan threngra safns flokkanafna.

## 4. Byrja greiningu

Smelltu a **Analyze folder**.

Forritid skannar valda moppu, safnar upplysingunum sem thad tharfnast og undirbyr yfirferdarlista.

## 5. Yfirfaradu adur en breytingar eru notadar

Yfirferdarglugginn leyfir ther ad skoda:

- tillagda flokka
- valfraed undirflokka
- endurnefningartillogur fyrir studdar skrar
- endanlegar avidfangsslodir
- forskodanir skra thar sem thad er stutt

Thu getur lagad eda hafnad tillogum adur en thu stadfestir nokkud.

Virkjaðu **Dry run (preview only, do not move files)** til að forskoða fyrirhugaða flutninga og endurnefningar án þess að framkvæma þær.

## 6. Nota breytingarnar

Veldu línurnar sem þú vilt vinna úr og smelltu á **Confirm and Process**. Forritið býr til nauðsynlegar möppur og framkvæmir valda flutninga eða endurnefningar.

**Auto-approve filename changes** og **Auto-approve categorization** forvelja viðeigandi línur. Þú þarft samt að staðfesta áður en breytingarnar eru framkvæmdar.

## 7. Afturkalla sidustu keyrslu

Ef thu notar breytingar og vilt sidan snua theim vid skaltu nota **Edit -> Undo last run** i valmyndinni.

Afturkollun er aetlad fyrir sidustu stadfestu flokkunarkeyrslu. Hun notar skradan keyrsluferil forritsins til ad flytja skrar til baka og snua studdum endurnefningum vid thar sem thad er mogulegt.

Best er ad nota Afturkalla adur en thu byrjar adra stora hreinsun i somu moppu.

## 8. Laera af yfirferdunum thinum

Thegar thu samthykkir flokka i yfirferdarglugganum getur forritid munad thessar stadbundnu akvardanir og notad thaer sem visbendingar i framtidarkeyrslum. Thetta thaelfir ekki eda breytir AI-likaninu.

Laerd daemi eru geymd i adskildum stadbundnum gagnagrunni, svo hreinsun venjulega flokkunarskymisins fjaerlaegir thau ekki. Til ad fjaerlaegja thessi stadbundnu laerdomu gogn skaltu nota **Settings -> Reset learned behavior**.

## Gott ad vita

- Forritid notar stadbundid skyndiminni til ad fordast ad vinna somu skrar aftur og til ad baeta samraemi.
- Sjalfvirk samthykkt er valfraed. Hafdu hana slokkta thar til thu ert orugg/ur med tillogur forritsins.
- Mynda- og skjalavalkosti er haegt ad stakka ut ser ef thu tharft meiri stjorn.

## Ef eitthvad litur rangt ut

Athugadu fyrst eftirfarandi:

- valda mappan er su sem thu aetladir
- vidkomandi greiningarvalkostir eru virkir
- adeins-endurnefna hamur takmarkar ekki nidurstoduna a ovaentan hatt
- flokkaleyfilisti er ekki ad threngja tillogurnar of mikid

Fyrir frekari bilanaleit skaltu opna **Help -> FAQ**.
