# Schnellstartanleitung

AI File Sorter hilft Ihnen dabei, Dateien nach Ihrer Pruefung und Zustimmung zu organisieren.

Die KI steuert die Analyse und schlaegt Kategorien, Unterkategorien und Namen vor. Sie greift nicht direkt auf Ihre Dateien ein. Die App fuehrt Verschiebungen oder Umbenennungen erst aus, nachdem Sie die geprueften Aenderungen bestaetigt haben.

## 1. Einen Ordner auswaehlen

Verwenden Sie **Browse**, um den Ordner auszuwaehlen, den Sie sortieren moechten.

Typische Beispiele:

- `Downloads`
- ein aufzuraeumender Desktop-Ordner
- ein Ordner auf einem externen Laufwerk
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

Sie koennen Vorschlaege anpassen oder ablehnen, bevor Sie etwas bestaetigen.

## 6. Aenderungen anwenden

Nach der Bestaetigung erstellt die App die benoetigten Ordner und fuehrt die Verschiebungen oder Umbenennungen aus.

## 7. Letzten Durchlauf rueckgaengig machen

Wenn Sie Aenderungen angewendet haben und sie danach zuruecknehmen moechten, verwenden Sie **Undo last run** im Menu.

Die Rueckgaengig-Funktion ist fuer den letzten bestaetigten Sortierdurchlauf gedacht. Sie nutzt den von der App gespeicherten Verlauf, um Dateien soweit moeglich zurueckzuschieben und unterstuetzte Umbenennungen rueckgaengig zu machen.

Am besten verwenden Sie die Funktion, bevor Sie eine weitere groessere Bereinigung im selben Ordner starten.

## 8. Lernen aus Ihren Bestaetigungen

Wenn Sie Kategorien im Pruefdialog bestaetigen, kann die App diese lokalen Entscheidungen merken und bei zukuenftigen Durchlaeufen als Hinweise verwenden. Dadurch wird das KI-Modell nicht trainiert oder veraendert.

Die gelernten Beispiele werden in einer separaten lokalen Datenbank gespeichert. Das Leeren des normalen Kategorisierungs-Caches entfernt sie daher nicht. Um diese lokalen Lerndaten zu entfernen, verwenden Sie **Settings -> Reset learned behavior**.

## Gut zu wissen

- Die App verwendet einen lokalen Cache, um Dateien nicht erneut zu verarbeiten und die Konsistenz zu verbessern.
- Die App wendet keine Aenderungen automatisch an, ohne vorher den Pruefschritt anzuzeigen.
- Bild- und Dokumentoptionen lassen sich separat aufklappen, wenn Sie mehr Kontrolle brauchen.

## Wenn etwas nicht stimmt

Pruefen Sie zuerst Folgendes:

- der ausgewaehlte Ordner ist wirklich der gewuenschte Ordner
- die relevanten Analyseoptionen sind aktiviert
- der Nur-Umbenennen-Modus schraenkt das Ergebnis nicht ungewollt ein
- eine Kategorie-Whitelist schraenkt die Vorschlaege nicht zu stark ein

Fuer weitere Hilfe oeffnen Sie **Help -> FAQ**.
