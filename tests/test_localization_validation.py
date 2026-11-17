"""Focused tests for the app localization validator; no network or credentials needed."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

SCRIPT = Path(__file__).resolve().parents[1] / 'app/scripts/localize_ai_file_sorter.py'
LOCALIZER = None
if SCRIPT.is_file():
    SPEC = importlib.util.spec_from_file_location('aifs_localization_validator', SCRIPT)
    LOCALIZER = importlib.util.module_from_spec(SPEC)
    sys.modules[SPEC.name] = LOCALIZER
    SPEC.loader.exec_module(LOCALIZER)


@unittest.skipIf(LOCALIZER is None, 'Private localization helper is not available in this checkout.')
class LocalizationValidationTests(unittest.TestCase):
    """Accept valid accents while rejecting replacement characters and broken UTF-8."""

    def test_valid_circumflex_text_is_preserved(self):
        for text in ('Yapay zekâ', 'âge', 'câble', 'hôpital', 'confiance', 'Âdem', 'Âşık', 'Ãngela'):
            with self.subTest(text=text):
                self.assertEqual(LOCALIZER.suspicious_replacement_count(text), 0)
                self.assertEqual(LOCALIZER.validate_translated_text('Example', text), [])

    def test_valid_turkish_markdown_passes(self):
        source = '# Quick Start\n\n- AI File Sorter suggests names.\n'
        translated = '# Hızlı Başlangıç\n\n- AI File Sorter yapay zekâ ile adlar önerir.\n'
        self.assertEqual(LOCALIZER.validate_markdown_translation(source, translated), [])

    def test_replacement_characters_and_mojibake_are_rejected(self):
        for text in ('caf?e', 'broken??', '\ufffd', 'cafÃ©', 'Â\u00a0', 'â€™', 'â€“', 'â€œ', 'â„¢', 'â\x80\x99'):
            with self.subTest(text=text):
                self.assertGreater(LOCALIZER.suspicious_replacement_count(text), 0)
                self.assertIn('translation contains suspicious replacement/mojibake pattern',
                              LOCALIZER.validate_translated_text('Example', text))

    def catalog(self, source='Archive index', target='Archive index', **attributes):
        root = ET.Element('TS', language='fr_FR')
        context = ET.SubElement(root, 'context')
        ET.SubElement(context, 'name').text = 'ExampleDialog'
        message = ET.SubElement(context, 'message')
        ET.SubElement(message, 'source').text = source
        ET.SubElement(message, 'translation', attributes).text = target
        return ET.ElementTree(root)

    def test_unreviewed_english_copies_are_rejected(self):
        for locale in LOCALIZER.SUPPORTED_LOCALES:
            if locale == 'en':
                continue
            for source in ('Archive index', 'Check for Updates', 'Available', '&Search', 'Validation report'):
                with self.subTest(locale=locale, source=source):
                    errors = LOCALIZER.validate_translated_text(source, source, locale_code=locale)
                    self.assertTrue(any('repeats English' in error for error in errors))

    def test_technical_names_and_valid_shared_words_are_allowed(self):
        for source in ('CPU (%1)', '%1 ms', 'AI File Sorter', 'Gemma 3 4B IT', 'PARA', 'sk-...', '[VISION-ERROR] %1 (%2)'):
            with self.subTest(source=source):
                self.assertEqual(LOCALIZER.validate_translated_text(source, source, locale_code='fr'), [])
        for source in ('Version', 'Description', 'Type', 'Images', '&Tests', '&Hindi', '(plugin)', '%1 total'):
            with self.subTest(source=source):
                self.assertEqual(LOCALIZER.validate_translated_text(source, source, locale_code='fr'), [])
        self.assertTrue(LOCALIZER.validate_translated_text('Version', 'Version', locale_code='hi'))
        self.assertTrue(LOCALIZER.validate_translated_text('Plugin version', 'Plugin version', locale_code='fr'))
        self.assertEqual(LOCALIZER.validate_translated_text('Later', 'Later', locale_code='nl'), [])
        self.assertTrue(LOCALIZER.validate_translated_text('Later', 'Later', locale_code='fr'))
        self.assertTrue(LOCALIZER.validate_translated_text('[SCAN] Exploring %1', '[SCAN] Exploring %1', locale_code='fr'))

    def test_identity_approval_is_explicit_and_preserves_placeholder_validation(self):
        marker = LOCALIZER.REVIEWED_IDENTITY_MARKER
        self.assertEqual(LOCALIZER.validate_translated_text('Example', 'Example', locale_code='fr', comment=marker), [])
        self.assertTrue(LOCALIZER.validate_translated_text('Example %1', 'Example %2', locale_code='fr', comment=marker))
        self.assertTrue(LOCALIZER.validate_translated_text('Example', 'Example', locale_code='fr', comment=f'Do not add {marker} here'))

    def test_literal_ampersands_are_not_mistaken_for_keyboard_accelerators(self):
        self.assertEqual(LOCALIZER.accelerator_count('Renamed & Moved'), 0)
        self.assertEqual(LOCALIZER.accelerator_count('Rename && Move'), 0)
        self.assertEqual(LOCALIZER.accelerator_count('&Rename'), 1)
        self.assertEqual(LOCALIZER.validate_translated_text_warnings('Renamed & Moved', 'Renommé et déplacé'), [])
        self.assertTrue(LOCALIZER.validate_translated_text_warnings('&Rename', 'Renommer'))

    def test_skip_existing_repairs_english_copies_and_invalid_finished_translations(self):
        for tree in (self.catalog(), self.catalog('Path %1', 'Chemin %2')):
            candidates, skipped = LOCALIZER.ui_candidates_for_tree(tree, locale=LOCALIZER.LocaleInfo('fr', 'French'), skip_existing=True)
            self.assertEqual(len(candidates), 1)
            self.assertEqual(skipped, 0)
        candidates, skipped = LOCALIZER.ui_candidates_for_tree(self.catalog('Close', 'Fermer'), locale=LOCALIZER.LocaleInfo('fr', 'French'), skip_existing=True)
        self.assertEqual(candidates, [])
        self.assertEqual(skipped, 1)

    def test_missing_translation_elements_are_created_for_repair(self):
        tree = self.catalog()
        message = tree.getroot().find('context/message')
        message.remove(message.find('translation'))
        candidates, _ = LOCALIZER.ui_candidates_for_tree(tree, locale=LOCALIZER.LocaleInfo('fr', 'French'), skip_existing=True)
        self.assertEqual(len(candidates), 1)
        self.assertIsNotNone(message.find('translation'))

    def test_obsolete_and_vanished_entries_are_not_repaired_or_counted(self):
        for status in ('obsolete', 'vanished'):
            tree = self.catalog(type=status)
            candidates, _ = LOCALIZER.ui_candidates_for_tree(tree, locale=LOCALIZER.LocaleInfo('fr', 'French'), skip_existing=True)
            self.assertEqual(candidates, [])
            self.assertEqual(LOCALIZER.active_source_keys(tree), set())

    def test_missing_source_messages_are_reported_without_catalog_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fr.ts'
            self.catalog('Close', 'Fermer').write(path, encoding='utf-8')
            before = path.read_bytes()
            expected = {('ExampleDialog', 'Close', '', False), ('ExampleDialog', 'New action', '', False)}
            with mock.patch.object(LOCALIZER, 'ts_catalog_path', return_value=path):
                issues = LOCALIZER.check_ui_catalogs([LOCALIZER.LocaleInfo('fr', 'French')], expected_sources=expected)
            self.assertTrue(any('UI source message is missing' in issue.message for issue in issues))
            self.assertEqual(before, path.read_bytes())

    def test_check_and_dry_run_do_not_read_credential_files(self):
        with mock.patch.object(LOCALIZER, 'load_env_file') as load:
            LOCALIZER.parse_args(['--check', '--parts', 'ui'])
            LOCALIZER.parse_args(['--dry-run', '--parts', 'ui'])
        load.assert_not_called()

    def test_check_refresh_audits_sources_without_writing_or_building_a_provider(self):
        with mock.patch.object(LOCALIZER, 'refresh_ui_source_catalogs') as refresh, \
             mock.patch.object(LOCALIZER, 'resolve_lupdate_path'), \
             mock.patch.object(LOCALIZER, 'extract_ui_source_keys', return_value=set()) as extract, \
             mock.patch.object(LOCALIZER, 'check_ui_catalogs', return_value=[]) as check, \
             mock.patch.object(LOCALIZER, 'build_translator') as build, \
             mock.patch.object(LOCALIZER, 'load_env_file') as load:
            self.assertEqual(LOCALIZER.main(['--check', '--refresh-ui-sources', '--parts', 'ui', '--locales', 'fr']), 0)
        extract.assert_called_once()
        self.assertEqual(check.call_args.kwargs['expected_sources'], set())
        refresh.assert_not_called()
        build.assert_not_called()
        load.assert_not_called()

    def test_ui_translation_always_refreshes_sources_before_selecting_candidates(self):
        calls = []
        with mock.patch.object(LOCALIZER, 'load_env_file'), \
             mock.patch.object(LOCALIZER, 'resolve_lupdate_path'), \
             mock.patch.object(LOCALIZER, 'build_translator'), \
             mock.patch.object(LOCALIZER, 'refresh_ui_source_catalogs', side_effect=lambda **_: calls.append('refresh')), \
             mock.patch.object(LOCALIZER, 'localize_ui_catalogs', side_effect=lambda **_: calls.append('translate') or []):
            self.assertEqual(LOCALIZER.main(['--parts', 'ui', '--locales', 'fr', '--skip-existing']), 0)
        self.assertEqual(calls, ['refresh', 'translate'])

    def test_dry_run_reports_missing_sources_without_changing_catalogs(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fr.ts'
            self.catalog().write(path, encoding='utf-8')
            before = path.read_bytes()
            expected = {('ExampleDialog', 'Archive index', '', False), ('ExampleDialog', 'New action', '', False)}
            with mock.patch.object(LOCALIZER, 'ts_catalog_path', return_value=path), \
                 mock.patch.object(LOCALIZER, 'resolve_lupdate_path'), \
                 mock.patch.object(LOCALIZER, 'extract_ui_source_keys', return_value=expected), \
                 mock.patch.object(LOCALIZER, 'refresh_ui_source_catalogs') as refresh, \
                 mock.patch.object(LOCALIZER, 'build_translator') as build, \
                 mock.patch.object(LOCALIZER, 'load_env_file') as load:
                self.assertEqual(LOCALIZER.main(['--dry-run', '--refresh-ui-sources', '--parts', 'ui', '--locales', 'fr']), 0)
            self.assertEqual(before, path.read_bytes())
            refresh.assert_not_called()
            build.assert_not_called()
            load.assert_not_called()

    def test_provider_indexes_must_match_once_with_string_values(self):
        valid = {'translations': [{'index': 1, 'text': 'Deux'}, {'index': 0, 'text': 'Un'}]}
        self.assertEqual(LOCALIZER.indexed_ui_translations(valid, 2), {0: 'Un', 1: 'Deux'})
        for rows in ([], [{'index': 0, 'text': 'Un'}],
                     [{'index': 0, 'text': 'Un'}, {'index': 0, 'text': 'Deux'}],
                     [{'index': 0, 'text': 'Un'}, {'index': 2, 'text': 'Deux'}],
                     [{'index': 0, 'text': 1}, {'index': 1, 'text': 'Deux'}],
                     [{'index': False, 'text': 'Un'}, {'index': 1, 'text': 'Deux'}]):
            with self.subTest(rows=rows), self.assertRaises(LOCALIZER.TranslationError):
                LOCALIZER.indexed_ui_translations({'translations': rows}, 2)

    def test_malformed_provider_indexes_have_a_bounded_retry(self):
        translator = LOCALIZER.OpenAITranslator({})
        valid = {'translations': [{'index': 0, 'text': 'Fermer'}]}
        with mock.patch.object(translator, '_request_json_response', side_effect=[{'translations': []}, valid]) as request:
            self.assertEqual(translator._request_ui_translations({}, 1), {0: 'Fermer'})
            self.assertEqual(request.call_count, translator.ui_response_attempts)
        with mock.patch.object(translator, '_request_json_response', return_value={'translations': []}) as request:
            with self.assertRaises(LOCALIZER.TranslationError):
                translator._request_ui_translations({}, 1)
            self.assertEqual(request.call_count, translator.ui_response_attempts)

    def test_invalid_plural_forms_are_reported_instead_of_silently_skipped(self):
        for forms in ([], [''], ['%n fichiers', '%1 fichiers']):
            tree = self.catalog('%n files', '')
            message = tree.getroot().find('context/message')
            message.set('numerus', 'yes')
            translation = message.find('translation')
            for form in forms:
                ET.SubElement(translation, 'numerusform').text = form
            with self.subTest(forms=forms):
                self.assertTrue(LOCALIZER.message_translation_errors(message, 'fr'))
                with self.assertRaisesRegex(LOCALIZER.TranslationError, 'requires Qt Linguist'):
                    LOCALIZER.ui_candidates_for_tree(tree, locale=LOCALIZER.LocaleInfo('fr', 'French'), skip_existing=True)
        translation.clear()
        for form in ('%n fichier', '%n fichiers'):
            ET.SubElement(translation, 'numerusform').text = form
        self.assertEqual(LOCALIZER.message_translation_errors(message, 'fr'), [])
        self.assertEqual(LOCALIZER.ui_candidates_for_tree(tree, locale=LOCALIZER.LocaleInfo('fr', 'French'), skip_existing=True), ([], 1))

    def test_skip_existing_repairs_invalid_markdown_and_preserves_valid_text(self):
        source = '# Help\n\nReview files.\n'
        translated = '# Aide\n\nVérifiez les fichiers.\n'
        with tempfile.TemporaryDirectory() as directory:
            source_path = Path(directory) / 'help.md'
            target_path = Path(directory) / 'help.fr.md'
            source_path.write_text(source, encoding='utf-8')
            target_path.write_text(source, encoding='utf-8')
            translator = mock.Mock()
            translator.translate_markdown.return_value = translated
            args = mock.Mock(skip_existing=True, limit=None, dry_run=False)
            with mock.patch.object(LOCALIZER, 'english_markdown_files', return_value=[source_path]), \
                 mock.patch.object(LOCALIZER, 'localized_markdown_path', return_value=target_path):
                for expected_updates in (1, 0):
                    stats = LOCALIZER.localize_markdown_directory(args=args, part='help', directory=Path(directory),
                        source_locale=LOCALIZER.LocaleInfo('en', 'English'), target_locales=[LOCALIZER.LocaleInfo('fr', 'French')], translator=translator)
                    self.assertEqual(stats[0].updated, expected_updates)
            translator.translate_markdown.assert_called_once()
            self.assertEqual(target_path.read_text(encoding='utf-8'), translated)

    def test_untranslated_markdown_is_rejected(self):
        text = '# Help\n\nReview files before applying changes.\n'
        self.assertIn('translated Markdown repeats English source', LOCALIZER.validate_markdown_translation(text, text))

    def test_ui_run_keeps_valid_repairs_but_never_writes_invalid_provider_output(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fr.ts'
            tree = self.catalog('Close', 'Close')
            context = tree.getroot().find('context')
            message = ET.SubElement(context, 'message')
            ET.SubElement(message, 'source').text = 'Path %1'
            ET.SubElement(message, 'translation', type='unfinished').text = 'Path %1'
            tree.write(path, encoding='utf-8')
            translator = mock.Mock()
            translator.translate_items.return_value = {'ExampleDialog:1': 'Fermer', 'ExampleDialog:2': 'Chemin %2'}
            args = mock.Mock(skip_existing=True, limit=None, dry_run=False)
            with mock.patch.object(LOCALIZER, 'ts_catalog_path', return_value=path):
                stats = LOCALIZER.localize_ui_catalogs(args=args, source_locale=LOCALIZER.LocaleInfo('en', 'English'),
                    target_locales=[LOCALIZER.LocaleInfo('fr', 'French')], translator=translator)
            self.assertEqual(stats[0].updated, 1)
            self.assertEqual(stats[0].validation_errors, 1)
            messages = ET.parse(path).getroot().findall('context/message')
            self.assertEqual(messages[0].findtext('translation'), 'Fermer')
            self.assertEqual(messages[1].findtext('translation'), 'Path %1')
            self.assertEqual(messages[1].find('translation').get('type'), 'unfinished')


if __name__ == '__main__':
    unittest.main()
