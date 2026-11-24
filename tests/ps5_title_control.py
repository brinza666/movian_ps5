# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
from pathlib import Path
import unittest
spec = importlib.util.spec_from_file_location('ps5_control', Path(__file__).resolve().parents[1] / 'scripts/ps5-title-control.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
SYSTEM = {'title_id': '', 'kind': 'system'}
OWN = {'title_id': module.TITLE, 'kind': 'game', 'app_id': 42}
OTHER = {'title_id': 'PPSA12345', 'kind': 'game', 'app_id': 99}


class Fake:
    def __init__(self, *states):
        self.states = list(states)
        self.calls = []

    def processes(self):
        if len(self.states) > 1:
            return self.states.pop(0)
        return self.states[0]

    def post(self, route, body):
        self.calls.append((route, body))
        return {'ok': True}


class Tests(unittest.TestCase):
    def test_launch_refuses_other_game(self):
        client = Fake([SYSTEM, OTHER])
        with self.assertRaisesRegex(RuntimeError, 'Another game'):
            module.control(client, 'launch')
        self.assertEqual(client.calls, [])

    def test_verified_launch_and_existing(self):
        client = Fake([SYSTEM], [SYSTEM, OWN])
        self.assertTrue(module.control(client, 'launch')['process_verified'])
        self.assertEqual(client.calls, [('/api/apps/launch', {'title_id': module.TITLE})])
        client = Fake([SYSTEM, OWN, OTHER])
        self.assertFalse(module.control(client, 'launch')['changed'])
        self.assertEqual(client.calls, [])

    def test_close_own_app_only(self):
        client = Fake([SYSTEM, OWN, OTHER], [SYSTEM, OWN, OTHER], [SYSTEM, OTHER])
        self.assertFalse(module.control(client, 'close')['test_running'])
        self.assertEqual(client.calls, [('/api/app/close', {'app_id': 42})])

    def test_changed_or_invalid_instance_refused(self):
        for updated in (dict(OWN, app_id=43), None):
            client = Fake([SYSTEM, OWN], [SYSTEM] + ([updated] if updated else []))
            with self.assertRaisesRegex(RuntimeError, 'instance changed'):
                module.control(client, 'close')
            self.assertEqual(client.calls, [])
        client = Fake([SYSTEM, dict(OWN, app_id=True)])
        with self.assertRaisesRegex(RuntimeError, 'confirmed app ID'):
            module.control(client, 'close')

    def test_acknowledgement_not_success(self):
        client = Fake([SYSTEM])
        with self.assertRaisesRegex(RuntimeError, 'not observed'):
            module.control(client, 'launch', wait=0)

    def test_malformed_state_rejected(self):
        client = module.Minion('192.168.10.238')
        for result in ({'ok': True, 'procs': []}, {'ok': True, 'procs': [{'pid': 1}]}):
            client.post = lambda *args: result
            with self.assertRaises(RuntimeError): client.processes()
        with self.assertRaises(ValueError): module.Minion('https://example.com/')


if __name__ == '__main__':
    unittest.main()
