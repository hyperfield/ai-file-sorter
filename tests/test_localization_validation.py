"""Focused tests for the app localization validator; no network or credentials needed."""
import importlib.util
from pathlib import Path
import sys
import unittest

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


if __name__ == '__main__':
    unittest.main()
