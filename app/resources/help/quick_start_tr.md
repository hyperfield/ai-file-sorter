# Hizli Baslangic Kilavuzu

AI File Sorter, değişiklikleri önce inceleyerek dosyaları düzenlemenize yardımcı olur. İsteğe bağlı otomatik onay, inceleme listesindeki değişiklikleri önceden seçebilir.

Yapay zekâ kategoriler, hedef klasörler ve adlar önerir. Uygulama, dosyaları yalnızca inceleme penceresindeki seçili değişiklikleri onayladıktan sonra taşır veya yeniden adlandırır.

## Guvenli Ilk Calistirma

AI File Sorter'i ilk kez kullaniyorsaniz, buyuk bir arsiv veya surucu secmeden once kucuk bir test klasoruyle baslayin.

Ilk calistirma icin iyi klasorler sunlardir:

- `Downloads` icinden 20-50 dosyanin kopyasi
- kucuk bir ekran goruntusu veya fotograf temizleme klasoru
- birkac PDF veya belge iceren gecici klasor

Bu, ilk calistirmayi incelemeyi kolaylastirir. Yerel modeller kullandiginizda dosyalariniz bilgisayarinizda kalir ve AI yalnizca kategoriler ve adlar onerir. Ilk calistirmada otomatik onay seceneklerini kapali birakin; boylece inceleme listesini onaylayana kadar hicbir sey tasinmaz veya yeniden adlandirilmaz.

## Bir LLM ayarlayın

**Settings -> Select LLM** menüsünü açın. **Local LLMs** açılır listesinden bir model veya **API connections** bölümünden bir sağlayıcı seçin.

**Qwen3.5 4B (Recommended)** ilk yerel seçenektir ve yeni kurulumlarda varsayılandır. Kategorilendirmede yüksek hassasiyet sunar ve mevcut klasör yapıları için önerilir. Önceden kaydedilmiş model seçiminiz korunur. Kendi GGUF dosyanızı kullanmak için **Custom local LLM (gguf)** seçeneğini ve ardından **Add** düğmesini kullanın.

Uygulamanın sunduğu bir yerel model için **Downloads** bölümünü genişletin, **Download** düğmesine tıklayın ve **OK** ile onaylamadan önce **Model ready** durumunu bekleyin. **Model storage**, indirme klasörünü seçmenizi sağlar. **See details**, indirme URL'sini, yerel yolu ve teknik bilgileri gösterir.

Görüntü analizi için ayrıca **Image analysis models** altında bir **Visual model** seçin ve gerekli tüm dosyalarını indirin. Bu seçim, metin modelinden ayrıdır.

**API connections** altında Gemini, ChatGPT (OpenAI) veya özel bir OpenAI uyumlu uç nokta seçip gerekli bağlantı bilgilerini ve model adını girin. Bulut API'leri internet gerektirir ve analiz girdilerini yapılandırılmış hizmete gönderir. Özel uç noktalar yerel olarak da çalışabilir.

## Yerel performansı kontrol edin (isteğe bağlı)

Modelleri indirdikten sonra **File -> System compatibility check** menüsünü açıp **Run benchmark** düğmesine tıklayın. Önce CPU veya GPU'yu yoğun kullanan diğer uygulamaları kapatın. Kontrol, özel GGUF modelleri de dahil olmak üzere indirilmiş yerel modelleri test eder.

**Backend** seçeneğini **Automatic (try CPU if GPU fails)** olarak bırakın veya CPU ile desteklenen GPU seçeneklerini karşılaştırmak için **All available backends** seçin. Sonuçlar her model ve arka uç için ayrı kaydedilir; birindeki hata, diğerindeki çalışan bir birleşimi dışlamaz.

Bir sonuç seçip aynı birleşimi tekrarlamak için **Retry selected**, başarısız birleşimleri tekrarlamak için **Retry failed** veya seçili modeli açılır listedeki belirli bir arka uçla denemek için **Try chosen backend** kullanın.

Öneriler yerel performansı tahmin eder. Kontrol, API hizmetlerini veya mevcut klasörleri seçme doğruluğunu test etmez ve seçili modelinizi otomatik olarak değiştirmez.

## 1. Bir klasor secin

Siralayacaginiz klasoru secmek icin **Browse** veya **File Explorer** bolmesini kullanin.

Tipik ornekler:

- `Downloads`
- masaustu temizleme klasoru
- harici surucudeki bir klasor
- ag veya bulutla senkronize edilen klasor
- proje arsivi

**Destination**, sıralanan dosyaların nereye gideceğini belirler. Seçili klasör içinde sıralamak için **Use analyzed folder** seçeneğini açık bırakın veya kapatıp başka bir hedef seçin.

## 2. Uygulamanin ne yapacagini secin

Ana secenekleri kullanarak uygulamanin sunlari yapip yapmayacagina karar verin:

- dosyalari klasorlere kategorize etme
- resimleri analiz etme
- belgeleri analiz etme
- desteklenen dosyalar icin yeniden adlandirma onerileri sunma

Yalnizca yeniden adlandirma onerileri istiyorsaniz, ilgili yalnizca-yeniden-adlandirma modunu etkinlestirin.

## 3. Nasıl sıralanacağını seçin

Bir sıralama modu seçin:

- **Create category folders**, önerilen kategori adlarından klasörler oluşturur.
- **Use existing folder structure**, **Destination** altındaki klasör ağacından klasörler seçer. Hiçbiri uygun değilse yeni hedeflere izin vermek için **Suggest new folders when needed** seçeneğini açın; yalnızca mevcut klasörleri kullanmak için kapalı bırakın.

Mevcut klasörler için **Qwen3.5 4B** öneriyoruz. **Gemma 1.1 7B** ve API üzerinden kullanılan modeller de uygun seçeneklerdir. Diğer yerel modeller bir öneri penceresi gösterir; modelinizi koruyabilir veya **Settings -> Select LLM** bölümünden değiştirebilirsiniz.

**Create category folders** modunda bir kategorilendirme stili seçin:

- **More refined** genel kullanim ve daha ayrintili gruplama icin
- **More consistent** benzer dosyalar arasinda daha guclu etiket tutarliligi istiyorsaniz

Uygulamanin daha dar bir kategori adi kumesi icinde kalmasini istiyorsaniz kategori beyaz listelerini de etkinlestirebilirsiniz.

## 4. Analizi baslatin

**Analyze folder** dugmesine tiklayin.

Uygulama secilen klasoru tarar, ihtiyac duydugu bilgileri toplar ve bir inceleme listesi hazirlar.

## 5. Degisiklikleri uygulamadan once inceleyin

Inceleme penceresi sunlari kontrol etmenizi saglar:

- onerilen kategoriler
- istege bagli alt kategoriler
- desteklenen dosyalar icin yeniden adlandirma onerileri
- son hedef yollari
- desteklenen yerlerde dosya onizlemeleri

Herhangi bir seyi onaylamadan once onerileri duzenleyebilir veya reddedebilirsiniz.

Planlanan taşıma ve yeniden adlandırmaları uygulamadan önizlemek için **Dry run (preview only, do not move files)** seçeneğini açın.

## 6. Degisiklikleri uygulayin

İşlenecek satırları seçip **Confirm and Process** düğmesine tıklayın. Uygulama gerekli klasörleri oluşturur ve seçilen taşıma veya yeniden adlandırmaları gerçekleştirir.

**Auto-approve filename changes** ve **Auto-approve categorization**, uygun satırları önceden seçer. Bu değişiklikler uygulanmadan önce yine de onay vermeniz gerekir.

## 7. Son calistirmayi geri al

Degisiklikleri uyguladiktan sonra geri almak isterseniz menuden **Edit -> Undo last run** secenegini kullanin.

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
