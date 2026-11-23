"""Wire vectors derived from the observed native M7 client and controlled TV test."""
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import struct
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location('movian_stpp', Path(__file__).resolve().parents[1] / 'scripts/movian_stpp.py')
stpp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stpp)


class WireTests(unittest.TestCase):
    def test_native_subscribe_vector(self):
        self.assertEqual(stpp.subscribe_message(1, 0, ['app', 'systemname']),
                         bytes.fromhex('0101000000000000000000036170700a73797374656d6e616d65'))

    def test_setting_write_address_terminates_before_type(self):
        self.assertEqual(stpp.set_int_message(12, ['value'], 0),
                         bytes.fromhex('030c0000000576616c7565000100000000'))

    def test_open_and_back_event_vectors(self):
        self.assertTrue(stpp.open_url_message('settings:').endswith(b'\x00\x0e\x01\x09settings:'))
        self.assertEqual(stpp.action_message(0, [], ['Back']), b'\x05\0\0\0\0\0\x01\x04Back')

    def test_long_event_string_uses_utf8_byte_length(self):
        value = '\u044f' * 128
        self.assertEqual(stpp.event_string(value)[:5], b'\xff\0\x01\0\0')

    def test_real_notification_shapes(self):
        self.assertEqual(stpp.decode_notification(bytes.fromhex('04010100000001000000'))['value'], 1)
        self.assertEqual(stpp.decode_notification(b'\x04\x03\x01\0\0\0\0matv')['value'], 'matv')
        self.assertEqual(stpp.decode_notification(b'\x04\x06\x02\0\0\0' + struct.pack('<II', 12, 13))['refs'], [12, 13])

    def test_rejects_truncated_and_invalid_inputs(self):
        for value in (b'', b'\x04', b'\x04\x01\x01\0\0\0', b'\x04\x08\x01\0\0\0\0',
                      b'\x04\x03\x01\0\0\0', b'\x04\x07\x01\0\0\0'):
            with self.assertRaises(stpp.ProtocolError):
                stpp.decode_notification(value)
        for path in ([''], ['a\0b'], ['a' * 256], ['a'] * 65):
            with self.assertRaises(stpp.ProtocolError):
                stpp.path_bytes(path)
        with self.assertRaises(stpp.ProtocolError):
            stpp.set_int_message(0, [], 2 ** 31)


if __name__ == '__main__':
    unittest.main()
