#!/usr/bin/env python3
"""Use the console's existing kern.arandom adapter in the pinned SMB client.

The v56 console crash occurred at the first arc4random_buf call in init.c.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1])
p = root / 'lib/init.c'
s = p.read_text()
if 'return movian_net_random_bytes(buf, len);' not in s:
    begin = s.index('int\nsmb2_random_bytes(void *buf, size_t len)')
    end = s.index('struct smb2_context *smb2_init_context(void)', begin)
    original = s[begin:end]
    s = s[:begin] + '''#if defined(__PROSPERO__)
int smb2_random_bytes(void *buf, size_t len)
{
        extern int movian_net_random_bytes(void *, size_t);
        return movian_net_random_bytes(buf, len);
}
#else
''' + original + '#endif\n\n' + s[end:]
    original = '''        smb2_random_bytes(smb2->client_challenge,
                          sizeof(smb2->client_challenge));
        smb2_random_bytes(smb2->salt, sizeof(smb2->salt));
        smb2_random_bytes(smb2->client_guid, sizeof(smb2->client_guid));'''
    assert original in s
    s = s.replace(original, '''#if defined(__PROSPERO__)
        if (smb2_random_bytes(smb2->client_challenge, sizeof(smb2->client_challenge)) < 0 ||
            smb2_random_bytes(smb2->salt, sizeof(smb2->salt)) < 0 ||
            smb2_random_bytes(smb2->client_guid, sizeof(smb2->client_guid)) < 0) {
                smb2_destroy_context(smb2);
                return NULL;
        }
#else
''' + original + '\n#endif')
    p.write_text(s)
p = root / 'lib/smb3-seal.c'
s = p.read_text()
if 'PS5 entropy source failed' not in s:
    original = '        smb2_random_bytes(&pdu->crypt[20], SMB3_GCM(smb2) ? 12 : 11);'
    assert original in s
    s = s.replace(original, '''#if defined(__PROSPERO__)
        if (smb2_random_bytes(&pdu->crypt[20], SMB3_GCM(smb2) ? 12 : 11) < 0) {
                smb2_set_error(smb2, "PS5 entropy source failed");
                free(pdu->crypt);
                pdu->crypt = NULL;
                return -1;
        }
#else
''' + original + '\n#endif')
    p.write_text(s)
