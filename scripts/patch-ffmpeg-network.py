#!/usr/bin/env python3
"""FFmpeg 6.1.2 FTP interoperability and PS5 socket portability fixes.

pyftpdlib sends RFC 959 preliminary reply 125 when data is already connected.
WSL Movian's own server supports LIST but rejects NLST and MLSD.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1])
p = root / 'libavutil/random_seed.c'
s = p.read_text()
if 'movian_net_random_bytes' not in s:
    begin = s.index('int av_random_bytes(uint8_t* buf, size_t len)')
    end = s.index('uint32_t av_get_random_seed(void)', begin)
    original = s[begin:end]
    s = s[:begin] + '''#if defined(__PROSPERO__)
int av_random_bytes(uint8_t* buf, size_t len)
{
    extern int movian_net_random_bytes(void *, size_t);
    return movian_net_random_bytes(buf, len) < 0 ? AVERROR(EIO) : 0;
}
#else
''' + original + '#endif\n\n' + s[end:]
    p.write_text(s)
for name in ('tcp.c', 'url.c', 'network.c'):
    p = root / 'libavformat' / name
    s = p.read_text()
    if '#define getaddrinfo movian_net_getaddrinfo' in s:
        if '#define getnameinfo movian_net_getnameinfo' not in s:
            s = s.replace('#define getaddrinfo movian_net_getaddrinfo', '#define getnameinfo movian_net_getnameinfo\n#define getaddrinfo movian_net_getaddrinfo')
    else:
        s = '''#ifdef __PROSPERO__
#define getnameinfo movian_net_getnameinfo
#define getaddrinfo movian_net_getaddrinfo
#define freeaddrinfo movian_net_freeaddrinfo
#define gai_strerror movian_net_gai_strerror
#endif
''' + s
    p.write_text(s)
p = root / 'libavformat/os_support.c'
s = p.read_text()
s = s.replace('int ff_socket_nonblock(int socket, int enable)\n{\n#if HAVE_WINSOCK2_H',
              'int ff_socket_nonblock(int socket, int enable)\n{\n#if defined(__PROSPERO__)\n'
              '    return setsockopt(socket, SOL_SOCKET, 0x1200, &enable, sizeof(enable));\n#elif HAVE_WINSOCK2_H')
p.write_text(s)
p = root / 'libavformat/ftp.c'
s = p.read_text()
start = s.index('static int ftp_list_mlsd(')
end = s.index('static int ftp_list(FTPContext', start)
s = s[:start] + '''static int ftp_list_mlsd(FTPContext *s)
{
    static const int codes[] = {125, 150, 500, 501, 502, 550, 0};
    int status = ftp_send_command(s, "MLSD\\r\\n", codes, NULL);
    if (status != 125 && status != 150) return AVERROR(ENOSYS);
    s->listing_method = MLSD;
    return 0;
}

static int ftp_list_nlst(FTPContext *s)
{
    /* LIST supplies file types when MLSD is unavailable. Read data before 226. */
    static const int codes[] = {125, 150, 425, 426, 451, 450, 500, 501, 502, 550, 0};
    int status = ftp_send_command(s, "LIST\\r\\n", codes, NULL);
    if (status != 125 && status != 150) return AVERROR(ENOSYS);
    s->listing_method = NLST;
    return 0;
}

''' + s[end:]
start = s.index('static int ftp_parse_entry_nlst(')
end = s.index('static int ftp_parse_entry_mlsd(', start)
s = s[:start] + '''static int ftp_parse_entry_nlst(char *line, AVIODirEntry *next)
{
    char mode[11];
    long long size;
    int pos = 0;
    if (sscanf(line, "%10s %*s %*s %*s %lld %*s %*s %*s %n", mode, &size, &pos) != 2 ||
        !pos || !line[pos] || size < 0 || strlen(mode) != 10)
        return 1;
    if (mode[0] != 'd' && mode[0] != '-') return 1;
    next->type = mode[0] == 'd' ? AVIO_ENTRY_DIRECTORY : AVIO_ENTRY_FILE;
    next->size = size;
    next->name = av_strdup(line + pos);
    return next->name ? 0 : AVERROR(ENOMEM);
}

''' + s[end:]
p.write_text(s)
