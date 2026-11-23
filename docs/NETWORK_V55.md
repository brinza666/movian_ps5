# SMB and FTP in v55

The isolated title is `PPSA99456`. Production `PPSA99455` and installed plugins
are unchanged. This implementation contains no sandbox-lift integration.

## Desktop evidence

The reference is the installed `/usr/bin/showtime`, Movian M7 7.0.273, running
in WSL Ubuntu 24.04.5. REA identified its ELF GNU build ID as
`cecc8ab5765c085d19ffdc4e5c86649186a62cfa` and SHA-256 as
`da133c4a6ee509da8f3b74dd2dfd1b445988381c5b3210d57b15a2e3a96ac9b1`.
The local evidence is under `build/v55-wsl-network`. `elf-notes.json` and
`exports.json` identify the shipped artifact. The small dynamic export table
does not recover its internal implementation. Binary strings include libsmb2
open, directory and service error names and FTP REST commands.

The live property API at port 42000 exposed this Network page order.

| Row | Desktop type and choices | PS5 behavior |
| --- | --- | --- |
| System name | Text, observed `movian` | Saved text, used by the FTP greeting |
| SMB Large Buffer | Toggle, observed On | Next network open uses 256 KiB AVIO buffering and SMB reads up to 1 MiB, capped by negotiation; Off uses 32 KiB buffering and 64 KiB SMB reads |
| SMB Extended Attributes | Off, SMBv1, SMBv2, On | Selection is saved; attribute storage is explicitly pending |
| FTP server | Section | Same position |
| Enable FTP server | Toggle | Starts an authenticated title-local server; failures stay Off and show an error |
| Server TCP port | Text, observed `7777` | Validates 1024–65535 and restarts an enabled listener |
| Username | Text | Saved privately |
| Password | Password text | Masked keyboard and display; saved privately |
| Remote control | Section | Same position |
| Allow remote control | Toggle | Pending, independently of the existing diagnostics service |

`settings.json` records the live labels, types and SMB choices without desktop
credentials. `login.png` captures the desktop FTP authentication prompt with
Username, Password, Remember me, Ok and Cancel. The PS5 login follows this order
and also exposes Domain for SMB. A read-only FTP session against WSL Movian
verified LIST at its virtual root and an FTP 500 response to NLST. Controlled
WSL client navigation exposed the authentication dialog, but the later fixture
directory remained loading. Desktop fixture playback is unverified.

No old andoma Git source was used. The transport implementation uses
[libsmb2](https://github.com/sahlberg/libsmb2/tree/ef32cff6cdda24279eaac802f468d9290a2c8b5d)
and [FFmpeg 6.1.2 FTP](https://github.com/FFmpeg/FFmpeg/blob/n6.1.2/libavformat/ftp.c).

## Use on PS5

Open Home → Local network, or Settings → Network → Saved SMB / FTP servers.
Left and Right choose one of eight saved servers. Enter an address such as
`smb2://192.168.10.10/media` or `ftp://192.168.10.10:2120/videos`.
Enter Username and Password separately, and Domain when needed, then choose
Open. Credentials in an address are rejected. An authentication failure opens
the login prompt. Clear Remember me to keep that login in memory for the session.

Folders use the existing browser, list/grid preference, sorting and folder
navigation. Triangle or Reload page fetches a fresh listing. Listings run on a
worker and retain at most 256 sorted entries. File paths retain long names while
their display labels may be shortened. Read and seek operations use 64-bit
positions and the existing FFmpeg and Videodec2 playback path. The network
transport has bounded I/O timeouts. Reads and seeks run outside the UI lock.

Network settings and logins live in `/download0/network-private.bin`, created
with mode 0600. The package includes no login data. The title FTP server hides
and rejects access to this file, including a tested symlink alias. Its virtual
root exposes Title data, Movian library and USB sources visible to the sandbox.
It handles one control session at a time and supports passive connections, LIST/NLST, SIZE, REST/RETR and STOR. Enter a
nonempty server Username and Password before enabling it. Its default port
7777 is separate from the console's external upload service on port 2120.

The dependency build scripts pin the libsmb2 archive and its checksum. They
disable Kerberos, GSSAPI, QUIC, RDMA, examples and utilities. PS5 socket handling
uses SO_NBIO and a numeric address adapter based on the public starter's
`examples/update-check/console_curl.c`. Hostnames use the existing native
sceNetResolver pattern before entering either transport. Numeric IPv4/IPv6
adapter behavior is covered by the default self-check. PS5 IPv6 transport and
hostname resolution still need console evidence.

FFmpeg FTP is patched to accept the RFC preliminary reply 125 as well as 150
for directory transfers. A Unix LIST parser is used when MLSD is rejected.
The fallback supplies file types and sizes; LIST timestamps are not parsed.

## Verification

The full `scripts/self-check.sh` passed, including diagnostics, settings, RAM
piece cache, multi-peer, tracker, DHT, private torrent and Matroska EOF/seek
regressions. Optional network fixtures are included in that script.

```sh
MOVIAN_HOST_NETWORK_FFMPEG_DIR=/tmp/movian-ffmpeg-v53 \
MOVIAN_HOST_SMB2_DIR=/tmp/movian-smb2-host \
MOVIAN_NETWORK_TEST_PYTHON=/tmp/movian-network-tests/bin/python \
bash scripts/self-check.sh
```

These paths refer to host-built FFmpeg with file/FTP/TCP and host-built static
libsmb2. The fixture Python environment needs pyftpdlib and impacket. Stop WSL
Movian first because the diagnostics test also binds port 42000.

| Check | Result |
| --- | --- |
| SMB2 and FTP login, wrong password, typed folders, seek/read and long filenames | Passed against disposable servers |
| Media filter, bounded result count and saved password reload | Passed |
| Remember me off and private file permissions | Passed |
| FTP server login, roots, listing, resumed read, upload and path protection | Passed |
| FFmpeg FTP LIST fallback and distant REST read against our own server | Passed |
| Shared PS5 UI/player source with H.264/AAC Matroska over SMB2 and FTP | Decoded playback and forward/backward seek exercised on host |
| Login keyboard, Remember me and Network setting interactions | Passed in host UI harness |
| Signed PS5 archive, identity and executable permissions | Passed; REA package inventory retained locally |
| Actual PS5 launch, server bind, hardware video, audible audio and controller input | Pending console access |

The console's ShadowMount API was reachable during this work, but FTP port 2120
refused connections and diagnostics port 42000 was unavailable. v55 has not been
uploaded, mounted, or run on the console. Host software decode is not evidence
of PS5 hardware playback. No release was published for this untested package.

## Remaining work

- Test v55 launch and SMB2/FTP hardware playback, seeks and reconnects on PS5.
- Check whether its sandbox permits the FTP control and passive listener binds.
- Add automatic WSD/NetBIOS/UPnP discovery and SMB server share enumeration.
- Add SMB1, WebDAV/WebDAVS, FTPS and any unobserved desktop network options.
- Implement SMB extended-attribute storage, remote control and network file
  management actions. Their pending UI state remains visible.
- Test other server implementations, SMB3 encryption, IPv6 and network loss.

The next acceptance check is a console SMB2 fixture open followed by hardware
first-frame and seek evidence. Then repeat over FTP and verify audible output.

Verified v55 archive SHA-256 is `2959ab59c5c6062bda94414783a2a97e6f5524ba6539dee533ba2e54e33390b5`. The final REA package bundle is
`build/v55-wsl-network/package-verified-rea.json`. Earlier package bundles in
that evidence folder precede the final socket/address dependency rebuild.
