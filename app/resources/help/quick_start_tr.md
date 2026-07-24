# Hizli Baslangic Kilavuzu

AI File Sorter, once inceleme yapilan bir is akisi ve guvenmeyi sectiginiz degisiklikler icin istege bagli otomatik onay ile dosyalari duzenlemenize yardim eder.

AI analizi yurutur ve kategoriler, alt kategoriler ve adlar onerir. Dosyalariniza dogrudan dokunmaz. Uygulama, incelediginiz degisiklikleri onayladiktan sonra tasima veya yeniden adlandirma yapar; ya da yalnizca ilgili otomatik onay ayarini etkinlestirdiyseniz otomatik olarak uygular.

## Guvenli Ilk Calistirma

AI File Sorter'i ilk kez kullaniyorsaniz, buyuk bir arsiv veya surucu secmeden once kucuk bir test klasoruyle baslayin.

Ilk calistirma icin iyi klasorler sunlardir:

- `Downloads` icinden 20-50 dosyanin kopyasi
- kucuk bir ekran goruntusu veya fotograf temizleme klasoru
- birkac PDF veya belge iceren gecici klasor

Bu, ilk calistirmayi incelemeyi kolaylastirir. Yerel modeller kullandiginizda dosyalariniz bilgisayarinizda kalir ve AI yalnizca kategoriler ve adlar onerir. Ilk calistirmada otomatik onay seceneklerini kapali birakin; boylece inceleme listesini onaylayana kadar hicbir sey tasinmaz veya yeniden adlandirilmaz.

## 1. Bir klasor secin

Siralayacaginiz klasoru secmek icin **Browse** veya **File Explorer** bolmesini kullanin.

Tipik ornekler:

- `Downloads`
- masaustu temizleme klasoru
- harici surucudeki bir klasor
- ag veya bulutla senkronize edilen klasor
- proje arsivi

## 2. Uygulamanin ne yapacagini secin

Ana secenekleri kullanarak uygulamanin sunlari yapip yapmayacagina karar verin:

- dosyalari klasorlere kategorize etme
- resimleri analiz etme
- belgeleri analiz etme
- desteklenen dosyalar icin yeniden adlandirma onerileri sunma

Yalnizca yeniden adlandirma onerileri istiyorsaniz, ilgili yalnizca-yeniden-adlandirma modunu etkinlestirin.

## 3. Kategorilendirme stilinizi secin

Hedefinize en uygun stili secin:

- **More refined** genel kullanim ve daha ayrintili gruplama icin
- **More consistent** benzer dosyalar arasinda daha guclu etiket tutarliligi istiyorsaniz

Uygulamanin daha dar bir kategori adi kumesi icinde kalmasini istiyorsaniz kategori beyaz listelerini de etkinlestirebilirsiniz.

## 4. Analizi baslatin

**Analyze and categorize files** dugmesine tiklayin.

Uygulama secilen klasoru tarar, ihtiyac duydugu bilgileri toplar ve bir inceleme listesi hazirlar.

## 5. Degisiklikleri uygulamadan once inceleyin

Inceleme penceresi sunlari kontrol etmenizi saglar:

- onerilen kategoriler
- istege bagli alt kategoriler
- desteklenen dosyalar icin yeniden adlandirma onerileri
- son hedef yollari
- desteklenen yerlerde dosya onizlemeleri

Herhangi bir seyi onaylamadan once onerileri duzenleyebilir veya reddedebilirsiniz.

## 6. Degisiklikleri uygulayin

Onayladiginizda uygulama gerekli klasorleri olusturur ve tasima veya yeniden adlandirmalari yapar. Otomatik onay etkinse, uygun kategori veya dosya adi degisiklikleri inceleme penceresinde durmadan uygulanabilir.

## 7. Son calistirmayi geri al

Degisiklikleri uyguladiktan sonra geri almak isterseniz menuden **Undo last run** secenegini kullanin.

Geri alma, en son onaylanmis siralama calistirmasi icin tasarlanmistir. Uygulamanin kaydettigi calistirma gecmisini kullanarak dosyalari geri tasir ve desteklenen yeniden adlandirmalari mumkun oldugunda geri alir.

En iyi sonuc icin, ayni klasorde baska buyuk bir temizlige baslamadan once Geri Al'i kullanin.

## 8. Incelemelerinizden ogrenme

Inceleme penceresinde kategorileri onayladiginizda, uygulama bu yerel kararlari hatirlayabilir ve gelecekteki calistirmalar icin ipucu olarak kullanabilir. Bu, AI modelini egitmez veya degistirmez.

Ogrenilen ornekler ayri bir yerel veritabaninda saklanir; bu nedenle normal kategorilendirme onbellegini temizlemek bunlari silmez. Bu yerel ogrenme verilerini kaldirmak icin **Settings -> Reset learned behavior** secenegini kullanin.

## Bilmekte fayda var

- Uygulama, ayni dosyalari tekrar islememek ve tutarliligi artirmak icin yerel onbellek kullanir.
- Otomatik onay istege baglidir. Uygulamanin onerilerine guvenene kadar kapali birakin.
- Daha fazla kontrol gerekiyorsa resim ve belge secenekleri ayri ayri genisletilebilir.

## Bir sey yanlis gorunuyorsa

Once sunlari kontrol edin:

- secili klasor hedeflediginiz klasor mu
- ilgili analiz secenekleri etkin mi
- yalnizca-yeniden-adlandirma modu sonucu beklemediginiz sekilde sinirlamiyor mu
- kategori beyaz listesi onerileri cok fazla daraltmiyor mu

Ek sorun giderme icin **Help -> FAQ** acin.