# Przewodnik szybkiego startu

AI File Sorter pomaga organizować pliki, stosując proces z przeglądem zmian jako pierwszym krokiem oraz opcjonalne automatyczne zatwierdzanie, które wstępnie wybiera zmiany na liście przeglądu.

AI sugeruje kategorie, foldery docelowe i nazwy. Aplikacja wykonuje przeniesienia lub zmiany nazw dopiero po potwierdzeniu wybranych zmian w oknie przeglądu.

## Bezpieczne pierwsze uruchomienie

Jeśli używasz AI File Sorter po raz pierwszy, zacznij od małego testowego folderu, zanim skierujesz go na duży archiwum lub dysk.

Dobre foldery na pierwszy przebieg to:

- kopia 20–50 plików z `Downloads`
- niewielki folder ze zrzutami ekranu lub zdjęciami do oczyszczenia
- tymczasowy folder z kilkoma plikami PDF lub dokumentami

To ułatwia przegląd pierwszego przebiegu. Twoje pliki pozostają na komputerze, gdy używasz lokalnych modeli, a AI tylko sugeruje kategorie i nazwy. Wyłącz opcje automatycznego zatwierdzania podczas pierwszego uruchomienia, aby nic nie zostało przeniesione ani przemianowane, dopóki nie zatwierdzisz listy przeglądu.

Opcjonalne wyszukiwanie lokalizacji online wysyła współrzędne GPS zdjęć zaokrąglone do trzech miejsc po przecinku do usługi Nominatim OpenStreetMap, nawet przy użyciu lokalnych modeli. Dostawca może także zobaczyć Twój adres IP i czas żądania. Pliki zdjęć, nazwy plików i daty wykonania nie są wysyłane; wzbogacanie kategorii tylko o datę pozostaje lokalne.

## Skonfiguruj LLM

Otwórz **Ustawienia -> Wybierz LLM**. Wybierz model z rozwijanego menu **Lokalne LLM** lub dostawcę z **Połączenia API**.

**Qwen3.5 4B (Zalecane)** to pierwsza lokalna opcja i domyślny wybór dla nowych konfiguracji. Zapewnia wysoką precyzję w kategoryzacji i jest zalecany dla istniejących struktur folderów. Twoje wcześniej zapisane ustawienie modelu zostanie zachowane. Aby użyć własnego pliku GGUF, wybierz **Niestandardowy lokalny LLM (gguf)** i użyj **Dodaj**.

Dla wbudowanego lokalnego modelu rozwiń **Pobrania**, kliknij **Pobierz** i poczekaj na komunikat **Model ready** przed potwierdzeniem przyciskiem **OK**. **Przechowywanie modeli** pozwala wybrać folder pobierania. **Zobacz szczegóły** pokazuje URL pobierania, lokalną ścieżkę i informacje techniczne.

Dla lokalnej analizy obrazów wybierz **Model wizualny** w sekcji **Modele analizy obrazów** i pobierz wymagane pliki. Model wizualny wybiera się oddzielnie od modelu tekstowego.

W sekcji **Połączenia API** wybierz Gemini, ChatGPT (OpenAI) lub niestandardowy punkt końcowy zgodny z OpenAI, a następnie wprowadź wymagane dane połączenia i nazwę modelu. API w chmurze wymagają dostępu do internetu i wysyłają dane analizy do skonfigurowanej usługi. Punkty końcowe niestandardowe mogą też działać lokalnie.

Aby analizować obrazy przez punkt końcowy zgodny z OpenAI, włącz dla tego połączenia opcję **Model z obsługą obrazów**, a następnie wybierz je w **Modele analizy obrazów → Model wizualny**. Nie jest wymagany lokalny model ani pobieranie projektora. Włączenie obsługi wizji poprosi o potwierdzenie, że zawartość obrazów zostanie wysłana na skonfigurowany serwer.

## Sprawdź wydajność lokalną (opcjonalnie)

Po pobraniu modeli otwórz **Plik -> Sprawdzenie zgodności systemu** i kliknij **Uruchom benchmark**. Najpierw zamknij inne aplikacje intensywnie korzystające z CPU lub GPU. Sprawdzenie testuje pobrane lokalne modele, w tym niestandardowe modele GGUF.

Pozostaw **Backend** ustawiony na **Automatycznie (użyj CPU, jeśli GPU zawiedzie)** lub wybierz **Wszystkie dostępne backendy**, aby porównać opcje CPU i obsługiwane GPU. Wyniki zapisywane są oddzielnie dla każdego modelu i backendu; niepowodzenie w jednym nie wyklucza działającej kombinacji w innym.

Wybierz wynik i użyj **Ponów wybrane**, aby powtórzyć tę kombinację, **Ponów nieudane**, aby powtórzyć nieudane kombinacje, lub **Wypróbuj wybrany backend**, aby przetestować wybrany model z konkretnym backendem z rozwijanego menu.

Rekomendacje szacują lokalną wydajność. Sprawdzenie nie testuje usług API ani dokładności przy użyciu istniejących folderów i nie zmienia automatycznie wybranego modelu.

## 1. Wybierz folder

Użyj **Przeglądaj** lub panelu **Eksplorator plików**, aby wybrać folder, który chcesz uporządkować.

Typowe przykłady:

- `Downloads`
- folder do oczyszczenia na Pulpicie
- folder na dysku zewnętrznym
- folder na dysku sieciowym lub synchronizowany z chmurą
- archiwum projektu

**Miejsce docelowe** kontroluje, gdzie trafią posortowane pliki. Pozostaw włączone **Użyj analizowanego folderu**, aby sortować wewnątrz wybranego folderu, lub wyłącz tę opcję i przeglądaj inne miejsce docelowe.

## 2. Wybierz, co aplikacja ma zrobić

Użyj głównych opcji, aby zdecydować, czy aplikacja powinna:

- kategoryzować pliki do folderów
- analizować obrazy
- analizować dokumenty
- proponować zmiany nazw dla obsługiwanych plików

Jeśli chcesz tylko sugestii zmiany nazwy, włącz **Proponuj zmianę nazw plików zdjęć** lub **Proponuj zmianę nazw dokumentów**, a następnie włącz **Zmieniaj nazwy zdjęć bez przenoszenia** lub **Zmieniaj nazwy dokumentów bez przenoszenia** dla danego typu pliku.

**Porządkuj foldery** przenosi całe foldery z ich zawartością i wyłącza **Skanuj podfoldery**. Aby skanować pliki wewnątrz podfolderów zamiast przenosić całe foldery, wyłącz **Porządkuj foldery** i włącz **Skanuj podfoldery**.

## 3. Wybierz sposób sortowania

Nowe profile zaczynają od ustawienia **Użyj istniejącej struktury folderów**. Wcześniej zapisane wybory trybu sortowania są zachowane. Wybierz **Miejsce docelowe**, zawierający foldery, których chcesz użyć, lub wybierz **Utwórz foldery kategorii**, aby je wygenerować.

Wybierz tryb sortowania:

- **Utwórz foldery kategorii** tworzy foldery na podstawie sugerowanych nazw kategorii.
- **Użyj istniejącej struktury folderów** wybiera foldery w drzewie Twojego **Miejsce docelowe**. Włącz **Sugeruj nowe foldery w razie potrzeby**, aby pozwolić na tworzenie nowych miejsc docelowych, gdy żadne nie pasuje dobrze; wyłącz tę opcję, aby używać wyłącznie istniejących folderów.

Użyj **Utwórz strukturę folderów**, aby dodać początkowe drzewo do miejsca docelowego. Jego **Język folderów** zaczyna się od języka Twoich kategorii i może być zmieniony tylko dla tej struktury. Nazwy folderów w innych językach są tłumaczone za pomocą wybranego LLM. Przejrzyj przetłumaczony podgląd przed kliknięciem **Utwórz**; numeracja i zagnieżdżenie folderów zostają zachowane.

Dla istniejących folderów zalecamy **Qwen3.5 4B**. **Gemma 1.1 7B** oraz modele dostępne przez API również są odpowiednimi opcjami. Inne lokalne modele wyświetlają ostrzeżenie; możesz zachować swój model lub zmienić go w **Ustawienia -> Wybierz LLM**.

W trybie **Utwórz foldery kategorii** wybierz styl kategoryzacji:

- **Bardziej szczegółowe** do ogólnego użytku i drobniejszego grupowania
- **Bardziej spójne** jeśli chcesz większej spójności etykiet w podobnych plikach

Listy dozwolonych kategorii (whitelisty) ograniczają sugerowane nazwy kategorii i podkategorii w trybie **Utwórz foldery kategorii**. Są wyłączone i ignorowane w trybie **Użyj istniejącej struktury folderów**, który używa drzewa Twojego miejsca docelowego zamiast nich. Twoje ustawienie whitelisty zostanie zachowane przy przełączaniu trybów.

## 4. Rozpocznij analizę

Kliknij **Analizuj folder**.

Aplikacja skanuje wybrany folder, zbiera potrzebne informacje i przygotowuje listę przeglądu.

## 5. Przejrzyj przed zastosowaniem zmian

Okno przeglądu pozwala sprawdzić:

- sugerowane kategorie
- opcjonalne podkategorie
- sugestie zmiany nazw dla obsługiwanych plików
- końcowe ścieżki docelowe
- podglądy plików tam, gdzie są obsługiwane

Możesz dostosować lub odrzucić sugestie przed potwierdzeniem czegokolwiek.

Włącz **Próbne uruchomienie (tylko podgląd, nie przenoś plików)**, aby zobaczyć planowane przeniesienia i zmiany nazw bez ich stosowania.

## 6. Zastosuj zmiany

Zaznacz wiersze, które chcesz przetworzyć, i kliknij **Zastosuj zmiany**. Aplikacja utworzy wymagane foldery i wykona wybrane przeniesienia lub zmiany nazw.

**Podgląd symulacji**: Włącz **Próbne uruchomienie (tylko podgląd, nie przenoś plików)**, aby zobaczyć planowane przeniesienia i zmiany nazw bez ich stosowania.

**Automatycznie zaznacz sugestie zmiany nazw** i **Automatycznie wybieraj sugestie sortowania** wstępnie zaznaczają kwalifikujące się wiersze. Nadal musisz je potwierdzić, zanim zmiany zostaną zastosowane.

## 7. Cofnij ostatnie uruchomienie

Jeśli zastosujesz zmiany, a potem chcesz je cofnąć, użyj **Edycja -> Cofnij ostatnie uruchomienie**.

Funkcja Cofnij jest przeznaczona dla najnowszego potwierdzonego przebiegu sortowania. Korzysta z zapisanego przez aplikację dziennika przebiegów, aby przenieść pliki z powrotem i odwrócić obsługiwane zmiany nazw, gdy to możliwe.

Dla najlepszych rezultatów użyj Cofnij przed rozpoczęciem kolejnego dużego porządkowania w tym samym folderze.

## 8. Nauka na podstawie Twoich przeglądów

Gdy zatwierdzasz kategorie w oknie przeglądu, aplikacja może zapamiętać te lokalne decyzje i używać ich jako wskazówek przy przyszłych przebiegach. Nie trenowało to ani nie modyfikuje modelu AI.

Wyuczone przykłady są przechowywane w oddzielnej lokalnej bazie danych, więc czyszczenie normalnego cache'u kategoryzacji ich nie usuwa. Aby usunąć te lokalne dane uczące się, użyj **Ustawienia -> Resetuj wyuczone zachowanie**.

## Warto wiedzieć

- Aplikacja używa lokalnego cache'u, aby unikać ponownego przetwarzania tych samych plików i poprawiać spójność.
- Auto-zatwierdzanie jest opcjonalne. Wyłącz je, dopóki nie poczujesz się pewnie co do sugestii aplikacji.
- Opcje obrazu i dokumentu można rozbudować osobno, jeśli potrzebujesz większej kontroli.

## Jeśli coś wydaje się nie tak

Najpierw sprawdź:

- czy wybrany folder to ten, który zamierzałeś
- czy odpowiednie opcje analizy są włączone
- czy tryb tylko zmiana nazw nie ogranicza wyniku w nieoczekiwany sposób
- czy lista dozwolonych kategorii (whitelist) nie zawęża sugestii zbyt mocno

W celu dodatkowego rozwiązywania problemów otwórz **Pomoc -> FAQ**.
