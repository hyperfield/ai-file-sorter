#include <catch2/catch_test_macros.hpp>
#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "TestHelpers.hpp"
#include "TranslationManager.hpp"
#include "Language.hpp"

#include <QCoreApplication>

#include <vector>

#ifndef _WIN32
TEST_CASE("MainApp retranslate reflects language changes") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    Settings settings;
    settings.load();

    MainApp window(settings, /*development_mode=*/false);
    struct ExpectedTranslation {
        Language language;
        QString analyze_label;
        QString folder_label;
    };

    const std::vector<ExpectedTranslation> expected = {
        {Language::English, QStringLiteral("Analyze folder"), QStringLiteral("Folder:")},
        {Language::French, QStringLiteral("Analyser le dossier"), QStringLiteral("Dossier :")},
        {Language::German, QStringLiteral("Ordner analysieren"), QStringLiteral("Ordner:")},
        {Language::Italian, QStringLiteral("Analizza cartella"), QStringLiteral("Cartella:")},
        {Language::Spanish, QStringLiteral("Analizar carpeta"), QStringLiteral("Carpeta:")},
        {Language::Dutch, QStringLiteral("Map analyseren"), QStringLiteral("Map:")},
        {Language::Turkish, QStringLiteral("Klasörü analiz et"), QStringLiteral("Klasör:")},
        {Language::Korean, QStringLiteral("폴더 분석"), QStringLiteral("폴더:")}
    };

    for (const auto& entry : expected) {
        settings.set_language(entry.language);
        TranslationManager::instance().set_language(entry.language);
        MainAppTestAccess::trigger_retranslate(window);

        const auto analyze_text = MainAppTestAccess::analyze_button_text(window);
        const auto folder_text = MainAppTestAccess::path_label_text(window);
        CAPTURE(static_cast<int>(entry.language), analyze_text, folder_text);

        REQUIRE(analyze_text == entry.analyze_label);
        REQUIRE(folder_text == entry.folder_label);
    }
}

TEST_CASE("Updater strings are translated for all supported UI languages") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    struct ExpectedTranslation {
        Language language;
        QString update_failed;
        QString update_manually;
        QString prepare_failed;
        QString installer_launch_failed;
        QString no_download_target;
        QString downloading_update;
        QString downloading_installer;
        QString installer_ready;
        QString quit_and_launch_message;
        QString quit_and_launch_button;
    };

    const std::vector<ExpectedTranslation> expected = {
        {Language::English,
         QStringLiteral("Update Failed"),
         QStringLiteral("Update manually"),
         QStringLiteral("Failed to prepare the update installer.\n%1"),
         QStringLiteral("The installer could not be launched."),
         QStringLiteral("No download target is available for this update."),
         QStringLiteral("Downloading Update"),
         QStringLiteral("Downloading the update installer..."),
         QStringLiteral("Installer Ready"),
         QStringLiteral("Quit the app and launch the installer to update"),
         QStringLiteral("Quit and Launch Installer")},
        {Language::French,
         QStringLiteral("Échec de la mise à jour"),
         QStringLiteral("Mettre à jour manuellement"),
         QStringLiteral("Échec de la préparation du programme d'installation de la mise à jour.\n%1"),
         QStringLiteral("Le programme d'installation n'a pas pu être lancé."),
         QStringLiteral("Aucune cible de téléchargement n'est disponible pour cette mise à jour."),
         QStringLiteral("Téléchargement de la mise à jour"),
         QStringLiteral("Téléchargement du programme d'installation de la mise à jour..."),
         QStringLiteral("Programme d'installation prêt"),
         QStringLiteral("Quittez l'application et lancez le programme d'installation pour effectuer la mise à jour"),
         QStringLiteral("Quitter et lancer le programme d'installation")},
        {Language::German,
         QStringLiteral("Update fehlgeschlagen"),
         QStringLiteral("Manuell aktualisieren"),
         QStringLiteral("Das Update-Installationsprogramm konnte nicht vorbereitet werden.\n%1"),
         QStringLiteral("Das Installationsprogramm konnte nicht gestartet werden."),
         QStringLiteral("Für dieses Update ist kein Download-Ziel verfügbar."),
         QStringLiteral("Update wird heruntergeladen"),
         QStringLiteral("Das Update-Installationsprogramm wird heruntergeladen..."),
         QStringLiteral("Installationsprogramm bereit"),
         QStringLiteral("Beenden Sie die App und starten Sie das Installationsprogramm, um zu aktualisieren"),
         QStringLiteral("Beenden und Installationsprogramm starten")},
        {Language::Italian,
         QStringLiteral("Aggiornamento non riuscito"),
         QStringLiteral("Aggiorna manualmente"),
         QStringLiteral("Impossibile preparare il programma di installazione dell'aggiornamento.\n%1"),
         QStringLiteral("Impossibile avviare il programma di installazione."),
         QStringLiteral("Nessuna destinazione di download disponibile per questo aggiornamento."),
         QStringLiteral("Download aggiornamento"),
         QStringLiteral("Download del programma di installazione dell'aggiornamento..."),
         QStringLiteral("Programma di installazione pronto"),
         QStringLiteral("Chiudi l'app e avvia il programma di installazione per aggiornare"),
         QStringLiteral("Chiudi e avvia il programma di installazione")},
        {Language::Spanish,
         QStringLiteral("La actualización falló"),
         QStringLiteral("Actualizar manualmente"),
         QStringLiteral("No se pudo preparar el instalador de la actualización.\n%1"),
         QStringLiteral("No se pudo ejecutar el instalador."),
         QStringLiteral("No hay un destino de descarga disponible para esta actualización."),
         QStringLiteral("Descargando actualización"),
         QStringLiteral("Descargando el instalador de la actualización..."),
         QStringLiteral("Instalador listo"),
         QStringLiteral("Cierra la aplicación y ejecuta el instalador para actualizar"),
         QStringLiteral("Cerrar y ejecutar instalador")},
        {Language::Dutch,
         QStringLiteral("Bijwerken mislukt"),
         QStringLiteral("Handmatig bijwerken"),
         QStringLiteral("Het installatieprogramma van de update kon niet worden voorbereid.\n%1"),
         QStringLiteral("Het installatieprogramma kon niet worden gestart."),
         QStringLiteral("Er is geen downloaddoel beschikbaar voor deze update."),
         QStringLiteral("Update downloaden"),
         QStringLiteral("Het installatieprogramma van de update wordt gedownload..."),
         QStringLiteral("Installatieprogramma gereed"),
         QStringLiteral("Sluit de app af en start het installatieprogramma om bij te werken"),
         QStringLiteral("Afsluiten en installatieprogramma starten")},
        {Language::Turkish,
         QStringLiteral("Güncelleme başarısız oldu"),
         QStringLiteral("Elle güncelle"),
         QStringLiteral("Güncelleme yükleyicisi hazırlanamadı.\n%1"),
         QStringLiteral("Yükleyici başlatılamadı."),
         QStringLiteral("Bu güncelleme için kullanılabilir bir indirme hedefi yok."),
         QStringLiteral("Güncelleme indiriliyor"),
         QStringLiteral("Güncelleme yükleyicisi indiriliyor..."),
         QStringLiteral("Yükleyici hazır"),
         QStringLiteral("Güncellemek için uygulamadan çıkın ve yükleyiciyi başlatın"),
         QStringLiteral("Çık ve yükleyiciyi başlat")},
        {Language::Korean,
         QStringLiteral("업데이트 실패"),
         QStringLiteral("수동으로 업데이트"),
         QStringLiteral("업데이트 설치 프로그램을 준비하지 못했습니다.\n%1"),
         QStringLiteral("설치 프로그램을 실행할 수 없습니다."),
         QStringLiteral("이 업데이트에 사용할 다운로드 대상이 없습니다."),
         QStringLiteral("업데이트 다운로드 중"),
         QStringLiteral("업데이트 설치 프로그램을 다운로드하는 중..."),
         QStringLiteral("설치 프로그램 준비 완료"),
         QStringLiteral("업데이트하려면 앱을 종료하고 설치 프로그램을 실행하세요"),
         QStringLiteral("종료 후 설치 프로그램 실행")}
    };

    for (const auto& entry : expected) {
        TranslationManager::instance().set_language(entry.language);

        const auto update_failed = QCoreApplication::translate("QObject", "Update Failed");
        const auto update_manually = QCoreApplication::translate("QObject", "Update manually");
        const auto prepare_failed = QCoreApplication::translate("QObject", "Failed to prepare the update installer.\n%1");
        const auto installer_launch_failed = QCoreApplication::translate("QObject", "The installer could not be launched.");
        const auto no_download_target = QCoreApplication::translate("QObject", "No download target is available for this update.");
        const auto downloading_update = QCoreApplication::translate("QObject", "Downloading Update");
        const auto downloading_installer = QCoreApplication::translate("QObject", "Downloading the update installer...");
        const auto installer_ready = QCoreApplication::translate("QObject", "Installer Ready");
        const auto quit_and_launch_message = QCoreApplication::translate("QObject", "Quit the app and launch the installer to update");
        const auto quit_and_launch_button = QCoreApplication::translate("QObject", "Quit and Launch Installer");

        CAPTURE(static_cast<int>(entry.language),
                update_failed,
                update_manually,
                prepare_failed,
                installer_launch_failed,
                no_download_target,
                downloading_update,
                downloading_installer,
                installer_ready,
                quit_and_launch_message,
                quit_and_launch_button);

        REQUIRE(update_failed == entry.update_failed);
        REQUIRE(update_manually == entry.update_manually);
        REQUIRE(prepare_failed == entry.prepare_failed);
        REQUIRE(installer_launch_failed == entry.installer_launch_failed);
        REQUIRE(no_download_target == entry.no_download_target);
        REQUIRE(downloading_update == entry.downloading_update);
        REQUIRE(downloading_installer == entry.downloading_installer);
        REQUIRE(installer_ready == entry.installer_ready);
        REQUIRE(quit_and_launch_message == entry.quit_and_launch_message);
        REQUIRE(quit_and_launch_button == entry.quit_and_launch_button);
    }
}
#endif
