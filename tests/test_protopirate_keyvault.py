import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "gen_protopirate_keys_builtin.py"
SPEC = importlib.util.spec_from_file_location("gen_protopirate_keys_builtin", SCRIPT)
KEYVAULT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(KEYVAULT)


class ProtoPirateKeyvaultTests(unittest.TestCase):
    def test_parse_kia_raw_keys(self):
        source = (
            "Filetype: Flipper SubGhz Keystore RAW File\n"
            "Version: 0\n"
            "Encryption: 0\n"
            "0123456789ABCDEFFEDCBA9876543210\n"
        )
        self.assertEqual(
            KEYVAULT.parse_kia_keystore(source),
            (0x0123456789ABCDEF, 0xFEDCBA9876543210),
        )

    def test_parse_kia_typed_keystore(self):
        source = (
            "Filetype: Flipper SubGhz Keystore File\n"
            "Version: 0\n"
            "Encryption: 0\n"
            "0123456789ABCDEF:11:KIA_KEY2\n"
            "FEDCBA9876543210:12:KIA_KEY3\n"
        )
        self.assertEqual(
            KEYVAULT.parse_kia_keystore(source),
            (0x0123456789ABCDEF, 0xFEDCBA9876543210),
        )

    def test_parse_vag_aut64_records(self):
        payload = bytes(range(KEYVAULT.VAG_AUT64_SIZE))
        source = (
            "Filetype: Flipper SubGhz Keystore RAW File\n"
            "Version: 0\n"
            "Encryption: 0\n"
            + payload.hex()
            + "\n"
        )
        self.assertEqual(KEYVAULT.parse_vag_keystore(source), payload)

    def test_encrypted_keystores_are_ignored(self):
        source = (
            "Filetype: Flipper SubGhz Keystore RAW File\n"
            "Version: 0\n"
            "Encryption: 1\n"
            "not-plaintext-key-material\n"
        )
        self.assertIsNone(KEYVAULT.parse_kia_keystore(source))
        self.assertIsNone(KEYVAULT.parse_vag_keystore(source))

    def test_rejects_wrong_plaintext_sizes(self):
        source = (
            "Filetype: Flipper SubGhz Keystore RAW File\n"
            "Version: 0\n"
            "Encryption: 0\n"
            "00\n"
        )
        with self.assertRaises(ValueError):
            KEYVAULT.parse_kia_keystore(source)
        with self.assertRaises(ValueError):
            KEYVAULT.parse_vag_keystore(source)

    def test_rejects_missing_kia_types_and_duplicate_types(self):
        header = "Filetype: Flipper SubGhz Keystore File\nVersion: 0\nEncryption: 0\n"
        with self.assertRaises(ValueError):
            KEYVAULT.parse_kia_keystore(header + "0123456789ABCDEF:11:KIA_KEY2\n")
        with self.assertRaises(ValueError):
            KEYVAULT.parse_kia_keystore(
                header
                + "0123456789ABCDEF:11:KIA_KEY2\n"
                + "0123456789ABCDEE:11:KIA_KEY2_ALT\n"
                + "FEDCBA9876543210:12:KIA_KEY3\n"
            )

    def test_generated_source_contains_only_configured_keys(self):
        source = KEYVAULT.generate_source(
            (0x0123456789ABCDEF, 0xFEDCBA9876543210),
            bytes(range(KEYVAULT.VAG_AUT64_SIZE)),
        )
        self.assertIn("0x0123456789ABCDEFULL", source)
        self.assertIn("0xFEDCBA9876543210ULL", source)
        self.assertIn("m1_kia_v6_keys_builtin_available = true", source)
        self.assertIn("m1_vag_aut64_keys_builtin_available = true", source)

    def test_stub_generation_marks_both_stores_unavailable(self):
        source = KEYVAULT.generate_source()
        self.assertIn("m1_kia_v6_keys_builtin_available = false", source)
        self.assertIn("m1_vag_aut64_keys_builtin_available = false", source)


if __name__ == "__main__":
    unittest.main()
