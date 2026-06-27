# Hizli Baslangic Kilavuzu

AI File Sorter, incelemeniz ve onayinizdan sonra dosyalarinizi duzenlemenize yardim eder.

Yapay zeka analizi yonlendirir ve kategoriler, alt kategoriler ve adlar onerir. Dosyalariniza dogrudan dokunmaz. Tasima veya yeniden adlandirma islemlerini, incelenen degisiklikleri onayladiktan sonra uygulama yapar.

## 1. Bir klasor secin

Siralamak istediginiz klasoru secmek icin **Browse** dugmesini kullanin.

Tipik ornekler:

- `Downloads`
- duzenlenecek bir masaustu klasoru
- harici bir surucudeki klasor
- bir proje arsivi

## 2. Uygulamanin ne yapacagini secin

Ana secenekleri kullanarak uygulamanin sunlari yapip yapmayacagina karar verin:

- dosyalari kategori klasorlerine ayirmak
- gorselleri analiz etmek
- belgeleri analiz etmek
- desteklenen dosyalar icin yeniden adlandirma onerileri sunmak

Yalnizca yeniden adlandirma onerileri istiyorsaniz ilgili sadece-yeniden-adlandir modunu etkinlestirin.

## 3. Kategorilendirme stilini secin

Hedefinize en uygun stili secin:

- **More refined** genel kullanim ve daha ayrintili gruplama icin
- **More consistent** benzer dosyalarda daha guclu tutarlilik icin

Uygulamanin daha dar bir kategori adlari kumesinde kalmasini istiyorsaniz kategori beyaz listelerini de etkinlestirebilirsiniz.

## 4. Analizi baslatin

**Analyze and categorize files** dugmesine tiklayin.

Uygulama secilen klasoru tarar, gereken bilgileri toplar ve bir inceleme listesi hazirlar.

## 5. Uygulamadan once inceleyin

Inceleme penceresinde sunlari kontrol edebilirsiniz:

- onerilen kategoriler
- istege bagli alt kategoriler
- desteklenen dosyalar icin yeniden adlandirma onerileri
- nihai hedef yollar

Herhangi bir seyi onaylamadan once onerileri duzenleyebilir veya reddedebilirsiniz.

## 6. Degisiklikleri uygulayin

Onay verdikten sonra uygulama gerekli klasorleri olusturur ve tasima ya da yeniden adlandirma islemlerini yapar.

## 7. Son calistirmayi geri alin

Degisiklikleri uyguladiktan sonra geri almak isterseniz menuden **Undo last run** secenegini kullanin.

Geri alma ozelligi, en son onaylanan siralama calistirmasi icin tasarlanmistir. Uygulamanin kaydettigi calistirma gecmisini kullanarak dosyalari mumkun oldugunca geri tasir ve desteklenen yeniden adlandirmalari geri alir.

En iyi sonuc icin ayni klasorde baska buyuk bir temizlik baslatmadan once geri alma ozelligini kullanin.

## 8. Incelemelerinizden ogrenme

Inceleme penceresinde kategorileri onayladiginizda uygulama bu yerel kararlari hatirlayabilir ve sonraki calistirmalarda ipucu olarak kullanabilir. Bu, yapay zeka modelini egitmez veya degistirmez.

Ogrenilen ornekler ayri bir yerel veritabaninda saklanir, bu nedenle normal kategorilendirme onbellegini temizlemek bunlari silmez. Bu yerel ogrenme verilerini silmek icin **Settings -> Reset learned behavior** secenegini kullanin.

## Bilmeniz iyi olur

- Uygulama, ayni dosyalari tekrar islememek ve tutarliligi iyilestirmek icin yerel bir onbellek kullanir.
- Uygulama, inceleme adimini gostermeden degisiklikleri otomatik olarak uygulamaz.
- Daha fazla denetim gerekiyorsa gorsel ve belge secenekleri ayri ayri genisletilebilir.

## Bir sey yanlis gorunuyorsa

Once sunlari kontrol edin:

- secilen klasor gercekten istediginiz klasor mu
- ilgili analiz secenekleri etkin mi
- sadece-yeniden-adlandir modu sonucu beklemediginiz sekilde kisitliyor mu
- bir kategori beyaz listesi onerileri fazla daraltiyor mu

Daha fazla yardim icin **Help -> FAQ** menusunu acin.
