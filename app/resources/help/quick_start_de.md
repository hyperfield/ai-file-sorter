# Schnellstartanleitung

AI File Sorter hilft Ihnen, Dateien mit einem Pruefung-zuerst-Ablauf und optionaler automatischer Genehmigung fuer Aenderungen zu organisieren, denen Sie vertrauen.

Die KI steuert die Analyse und schlaegt Kategorien, Unterkategorien und Namen vor. Sie aendert Ihre Dateien nicht direkt. Die App fuehrt Verschiebungen oder Umbenennungen aus, nachdem Sie die geprueften Aenderungen bestaetigt haben, oder automatisch nur dann, wenn Sie die passende Auto-Genehmigung aktiviert haben.

## Sicherer erster Durchlauf

Wenn Sie AI File Sorter zum ersten Mal verwenden, beginnen Sie mit einem kleinen Testordner, bevor Sie ein grosses Archiv oder Laufwerk auswaehlen.

Gute Ordner fuer den ersten Durchlauf sind:

- eine Kopie von 20-50 Dateien aus `Downloads`
- ein kleiner Screenshot- oder Foto-Aufraeumordner
- ein temporaerer Ordner mit einigen PDFs oder Dokumenten

So bleibt der erste Durchlauf leicht zu pruefen. Ihre Dateien bleiben auf Ihrem Computer, wenn Sie lokale Modelle verwenden, und die KI schlaegt nur Kategorien und Namen vor. Lassen Sie Auto-Genehmigungen beim ersten Durchlauf ausgeschaltet, damit nichts verschoben oder umbenannt wird, bevor Sie die Pruefliste genehmigen.

## 1. Einen Ordner auswaehlen

Verwenden Sie **Browse** oder den **File Explorer**-Bereich, um den Ordner auszuwaehlen, den Sie sortieren moechten.

Typische Beispiele:

- `Downloads`
- ein aufzuraeumender Desktop-Ordner
- ein Ordner auf einem externen Laufwerk
- ein Netzwerk- oder Cloud-synchronisierter Ordner
- ein Projektarchiv

## 2. Festlegen, was die App tun soll

Mit den Hauptoptionen legen Sie fest, ob die App:

- Dateien in Kategorieordner einsortieren soll
- Bilder analysieren soll
- Dokumente analysieren soll
- Umbenennungsvorschlaege fuer unterstuetzte Dateien anbieten soll

Wenn Sie nur Umbenennungsvorschlaege moechten, aktivieren Sie den entsprechenden Nur-Umbenennen-Modus.

## 3. Den Kategorisierungsstil waehlen

Waehlen Sie den Stil, der am besten zu Ihrem Ziel passt:

- **More refined** fuer den allgemeinen Einsatz und feinere Gruppierungen
- **More consistent**, wenn Sie eine staerkere Konsistenz bei aehnlichen Dateien wollen

Sie koennen auch Kategorie-Whitelists aktivieren, wenn die App nur innerhalb einer engeren Menge von Kategorienamen bleiben soll.

## 4. Analyse starten

Klicken Sie auf **Analyze and categorize files**.

Die App durchsucht den ausgewaehlten Ordner, sammelt die benoetigten Informationen und erstellt eine Pruefliste.

## 5. Vor dem Anwenden pruefen

Im Pruefdialog koennen Sie Folgendes kontrollieren:

- vorgeschlagene Kategorien
- optionale Unterkategorien
- Umbenennungsvorschlaege fuer unterstuetzte Dateien
- die endgueltigen Zielpfade
- Dateivorschauen, soweit unterstuetzt

Sie koennen Vorschlaege anpassen oder ablehnen, bevor Sie etwas bestaetigen.

## 6. Aenderungen anwenden

Nach der Bestaetigung erstellt die App die benoetigten Ordner und fuehrt die Verschiebungen oder Umbenennungen aus. Wenn Auto-Genehmigung aktiviert ist, koennen geeignete Kategorie- oder Dateinamensaenderungen angewendet werden, ohne beim Pruefdialog anzuhalten.

## 7. Letzten Durchlauf rueckgaengig machen

Wenn Sie Aenderungen angewendet haben und sie danach zuruecknehmen moechten, verwenden Sie **Undo last run** im Menu.

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