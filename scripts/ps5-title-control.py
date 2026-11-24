#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Control only PPSA99456 through the observed PS5Upload Minion 12.6.23 API.

Mounting is separate. This client does not upload files, load payloads, reset
profiles, terminate other titles or infer a process from a filesystem mount.
"""
import argparse
import ipaddress
import json
import time
import urllib.error
import urllib.request

TITLE = 'PPSA99456'


class Minion:
    def __init__(self, host, timeout=5):
        address = ipaddress.ip_address(host)
        if address.version != 4 or not address.is_private or address.is_multicast or address.is_unspecified:
            raise ValueError('Use the verified private IPv4 address of this PS5')
        self.base = f'http://{address}:9180'
        self.timeout = timeout
        self._token = None

    def _request(self, route, body=None):
        headers = {'Content-Type': 'application/json'}
        if body is not None:
            headers['X-PS5U-Token'] = self._token
        request = urllib.request.Request(self.base + route,
            data=None if body is None else json.dumps(body).encode(), headers=headers)
        try:
            with urllib.request.urlopen(request, timeout=self.timeout) as response:
                raw = response.read(2 * 1024 * 1024 + 1)
        except urllib.error.HTTPError as error:
            # Do not print token/error response bodies or private process lists.
            raise RuntimeError(f'Minion HTTP {error.code} at {route}') from None
        if len(raw) > 2 * 1024 * 1024:
            raise RuntimeError('Minion response exceeds the bound')
        value = json.loads(raw)
        if not isinstance(value, dict):
            raise RuntimeError('Invalid Minion response')
        return value

    def post(self, route, body):
        if not self._token:
            result = self._request('/api/token?page=dev')
            token = result.get('token')
            if not isinstance(token, str) or not token or len(token) > 4096:
                raise RuntimeError('Minion did not provide a valid token')
            self._token = token  # Memory only. Never included in results/logs.
        result = self._request(route, body)
        if result.get('ok') is not True:
            raise RuntimeError(f'Minion did not accept {route}')
        return result

    def processes(self):
        result = self.post('/api/proc/list', {})
        procs = result.get('procs')
        if not isinstance(procs, list) or not procs or len(procs) > 4096:
            raise RuntimeError('Cannot establish the console process state')
        for proc in procs:
            if not isinstance(proc, dict) or not isinstance(proc.get('title_id'), str) or not isinstance(proc.get('kind'), str):
                raise RuntimeError('Incomplete process identity; refusing lifecycle action')
        return procs


def own_process(procs):
    own = [p for p in procs if p['title_id'] == TITLE]
    if len(own) > 1:
        raise RuntimeError('Ambiguous Movian test process state')
    return own[0] if own else None


def other_game(procs):
    return any(p['title_id'] != TITLE and
               (p['title_id'].startswith(('PPSA', 'CUSA')) or p['kind'] == 'game') for p in procs)


def control(client, action, wait=30):
    procs = client.processes()
    own = own_process(procs)
    if action == 'status':
        return {'test_running': own is not None, 'other_game_running': other_game(procs)}
    if action == 'launch':
        if own:
            return {'test_running': True, 'changed': False}
        if other_game(procs):
            raise RuntimeError('Another game is running; it was left untouched')
        client.post('/api/apps/launch', {'title_id': TITLE})
        expected = True
    elif action == 'close':
        if not own:
            return {'test_running': False, 'changed': False}
        app_id = own.get('app_id')
        if type(app_id) is not int or app_id <= 0:
            raise RuntimeError('Movian has no confirmed app ID; refusing close')
        # Confirm the association again immediately before issuing the close.
        current = own_process(client.processes())
        if not current or current.get('app_id') != app_id:
            raise RuntimeError('Movian instance changed; no close request was sent')
        client.post('/api/app/close', {'app_id': app_id})
        expected = False
    else:
        raise ValueError('Unknown action')
    deadline = time.monotonic() + wait
    while time.monotonic() < deadline:
        if (own_process(client.processes()) is not None) == expected:
            return {'test_running': expected, 'changed': True, 'process_verified': True}
        time.sleep(.25)
    raise RuntimeError('Request acknowledged but process transition was not observed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('status', 'launch', 'close'))
    parser.add_argument('--host', required=True, help='Current verified PS5 IPv4 address')
    args = parser.parse_args()
    try:
        result = control(Minion(args.host), args.action)
    except (ValueError, RuntimeError, OSError) as error:
        parser.exit(1, str(error) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
