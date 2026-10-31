# Schnellstartanleitung

AI File Sorter hilft Ihnen, Dateien vor dem Anwenden von Änderungen zu prüfen. Die optionale automatische Genehmigung kann Änderungen in der Prüfliste vorauswählen.

Die KI schlägt Kategorien, Zielordner und Namen vor. Die App verschiebt Dateien oder benennt sie erst um, nachdem Sie die ausgewählten Änderungen im Prüfdialog bestätigt haben.

## Sicherer erster Durchlauf

Wenn Sie AI File Sorter zum ersten Mal verwenden, beginnen Sie mit einem kleinen Testordner, bevor Sie ein grosses Archiv oder Laufwerk auswaehlen.

Gute Ordner fuer den ersten Durchlauf sind:

- eine Kopie von 20-50 Dateien aus `Downloads`
- ein kleiner Screenshot- oder Foto-Aufraeumordner
- ein temporaerer Ordner mit einigen PDFs oder Dokumenten

So bleibt der erste Durchlauf leicht zu pruefen. Ihre Dateien bleiben auf Ihrem Computer, wenn Sie lokale Modelle verwenden, und die KI schlaegt nur Kategorien und Namen vor. Lassen Sie Auto-Genehmigungen beim ersten Durchlauf ausgeschaltet, damit nichts verschoben oder umbenannt wird, bevor Sie die Pruefliste genehmigen.

## Ein LLM einrichten

Öffnen Sie **Settings -> Select LLM**. Wählen Sie ein Modell aus der Dropdown-Liste **Local LLMs** oder einen Anbieter unter **API connections**.

**Qwen3.5 4B (Recommended)** ist die erste lokale Option und der Standard für neue Einrichtungen. Es bietet hohe Präzision bei der Kategorisierung und wird für vorhandene Ordnerstrukturen empfohlen. Ihre zuvor gespeicherte Modellauswahl bleibt erhalten. Für eine eigene GGUF-Datei wählen Sie **Custom local LLM (gguf)** und dann **Add**.

Für ein von der App angebotenes lokales Modell klappen Sie **Downloads** auf, klicken auf **Download** und warten auf **Model ready**, bevor Sie mit **OK** bestätigen. Unter **Model storage** wählen Sie den Download-Ordner. **See details** zeigt die Download-URL, den lokalen Pfad und technische Informationen.

Wählen Sie für die Bildanalyse außerdem ein **Visual model** unter **Image analysis models** und laden Sie alle benötigten Dateien herunter. Diese Auswahl ist unabhängig vom Textmodell.

Wählen Sie unter **API connections** Gemini, ChatGPT (OpenAI) oder einen eigenen OpenAI-kompatiblen Endpunkt und geben Sie die erforderlichen Verbindungsdaten und den Modellnamen ein. Cloud-APIs benötigen Internet und senden die Analyseeingaben an den eingerichteten Dienst. Eigene Endpunkte können auch lokal laufen.

## Lokale Leistung prüfen (optional)

Öffnen Sie nach dem Herunterladen der Modelle **File -> System compatibility check** und klicken Sie auf **Run benchmark**. Schließen Sie vorher andere CPU- oder GPU-intensive Apps. Die Prüfung testet heruntergeladene lokale Modelle einschließlich eigener GGUF-Modelle.

Belassen Sie **Backend** auf **Automatic (try CPU if GPU fails)** oder wählen Sie **All available backends**, um CPU und unterstützte GPU-Optionen zu vergleichen. Ergebnisse werden für jedes Modell und Backend getrennt gespeichert; ein Fehler auf einem Backend schließt eine funktionierende Kombination auf einem anderen nicht aus.

Wählen Sie ein Ergebnis und verwenden Sie **Retry selected** zum Wiederholen dieser Kombination, **Retry failed** zum Wiederholen fehlgeschlagener Kombinationen oder **Try chosen backend**, um das ausgewählte Modell mit einem bestimmten Backend aus der Dropdown-Liste zu testen.

Die Empfehlungen schätzen die lokale Leistung ein. Die Prüfung testet weder API-Dienste noch die Genauigkeit bei der Auswahl vorhandener Ordner und ändert Ihr ausgewähltes Modell nicht automatisch.

## 1. Einen Ordner auswaehlen

Verwenden Sie **Browse** oder den **File Explorer**-Bereich, um den Ordner auszuwaehlen, den Sie sortieren moechten.

Typische Beispiele:

- `Downloads`
- ein aufzuraeumender Desktop-Ordner
- ein Ordner auf einem externen Laufwerk
- ein Netzwerk- oder Cloud-synchronisierter Ordner
- ein Projektarchiv

**Destination** bestimmt, wohin sortierte Dateien gelangen. Lassen Sie **Use analyzed folder** aktiviert, um im ausgewählten Ordner zu sortieren, oder deaktivieren Sie es und wählen Sie ein anderes Ziel.

## 2. Festlegen, was die App tun soll

Mit den Hauptoptionen legen Sie fest, ob die App:

- Dateien in Kategorieordner einsortieren soll
- Bilder analysieren soll
- Dokumente analysieren soll
- Umbenennungsvorschlaege fuer unterstuetzte Dateien anbieten soll

Wenn Sie nur Umbenennungsvorschlaege moechten, aktivieren Sie den entsprechenden Nur-Umbenennen-Modus.

## 3. Die Sortierung wählen

Wählen Sie einen Sortiermodus:

- **Create category folders** erstellt Ordner aus vorgeschlagenen Kategorienamen.
- **Use existing folder structure** wählt Ordner innerhalb des Verzeichnisbaums von **Destination**. Aktivieren Sie **Suggest new folders when needed**, um neue Ziele zuzulassen, wenn keines gut passt; lassen Sie es deaktiviert, um nur vorhandene Ordner zu verwenden.

Für vorhandene Ordner empfehlen wir **Qwen3.5 4B**. **Gemma 1.1 7B** und über eine API verwendete Modelle sind ebenfalls geeignet. Andere lokale Modelle zeigen einen Hinweis; Sie können Ihr Modell beibehalten oder unter **Settings -> Select LLM** ändern.

Wählen Sie im Modus **Create category folders** einen Kategorisierungsstil:

- **More refined** fuer den allgemeinen Einsatz und feinere Gruppierungen
- **More consistent**, wenn Sie eine staerkere Konsistenz bei aehnlichen Dateien wollen

Sie koennen auch Kategorie-Whitelists aktivieren, wenn die App nur innerhalb einer engeren Menge von Kategorienamen bleiben soll.

## 4. Analyse starten

Klicken Sie auf **Analyze folder**.

Die App durchsucht den ausgewaehlten Ordner, sammelt die benoetigten Informationen und erstellt eine Pruefliste.

## 5. Vor dem Anwenden pruefen

Im Pruefdialog koennen Sie Folgendes kontrollieren:

- vorgeschlagene Kategorien
- optionale Unterkategorien
- Umbenennungsvorschlaege fuer unterstuetzte Dateien
- die endgueltigen Zielpfade
- Dateivorschauen, soweit unterstuetzt

Sie koennen Vorschlaege anpassen oder ablehnen, bevor Sie etwas bestaetigen.

Aktivieren Sie **Dry run (preview only, do not move files)**, um geplante Verschiebungen und Umbenennungen anzusehen, ohne sie anzuwenden.

## 6. Aenderungen anwenden

Wählen Sie die zu verarbeitenden Zeilen aus und klicken Sie auf **Confirm and Process**. Die App erstellt erforderliche Ordner und führt die ausgewählten Verschiebungen oder Umbenennungen aus.

**Auto-approve filename changes** und **Auto-approve categorization** wählen geeignete Zeilen vorab aus. Sie müssen weiterhin bestätigen, bevor diese Änderungen angewendet werden.

## 7. Letzten Durchlauf rueckgaengig machen

Wenn Sie Aenderungen angewendet haben und sie danach zuruecknehmen moechten, verwenden Sie **Edit -> Undo last run** im Menu.

Die Rueckgaengig-Funktion ist fuer den letzten bestaetigten Sortierdurchlauf gedacht. Sie nutzt den von der App gespeicherten Verlauf, um Dateien soweit moeglich zurueckzuschieben und unterstuetzte Umbenennungen rueckgaengig zu machen.

Am besten verwenden Sie die Funktion, bevor Sie eine weitere groessere Bereinigung im selben Ordner starten.

## 8. Lernen aus Ihren Bestaetigungen

Wenn Sie Kategorien im Pruefdialog bestaetigen, kann die App diese lokalen Entscheidungen merken und bei zukuenftigen Durchlaeufen als Hinweise verwenden. Dadurch wird das KI-Modell nicht trainiert oder veraendert.

Die gelernten Beispiele werden in einer separaten lokalen Datenbank gespeichert. Das Leeren des normalen Kategorisierungs-Caches entfernt sie daher nicht. Um diese lokalen Lerndaten zu entfernen, verwenden Sie **Settings -> Reset learned behavior**.

## Gut zu wissen

- Die App verwendet einen lokalen Cache, um Dateien nicht erneut zu verarbeiten und die Konsistenz zu verbessern.
- Auto-Genehmigung ist optional. Lassen Sie sie ausgeschaltet, bis Sie den Vorschlaegen der App vertrauen.
- Bild- und Dokumentoptionen lassen sich separat aufklappen, wenn Sie mehr Kontrolle brauchen.

## Wenn etwas nicht stimmt

Pruefen Sie zuerst Folgendes:

- der ausgewaehlte Ordner ist der beabsichtigte Ordner
- die relevanten Analyseoptionen sind aktiviert
- der Nur-Umbenennen-Modus begrenzt das Ergebnis nicht unerwartet
- eine Kategorie-Whitelist schraenkt die Vorschlaege nicht zu stark ein

Fuer weitere Fehlersuche oeffnen Sie **Help -> FAQ**.
