/* Numeric address adapter for the PS5 SMB2 and FFmpeg TCP transports.
 * Hostnames are resolved with sceNetResolver before entering these transports.
 * Pattern follows the public starter's examples/update-check/console_curl.c.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#if defined(__PROSPERO__) || defined(MOVIAN_NETWORK_POSIX_TEST)
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#if defined(__PROSPERO__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

int movian_net_random_bytes(void *buffer, size_t length)
{
    unsigned char *out = buffer;
    size_t done = 0;
    if (!buffer && length) { errno = EINVAL; return -1; }
    while (done < length) {
        size_t count = length - done > 256 ? 256 : length - done;
        const size_t requested = count;
#if defined(__PROSPERO__)
        int result = sysctlbyname("kern.arandom", out + done, &count, NULL, 0);
#else
        extern int movian_net_test_entropy(void *, size_t *);
        int result = movian_net_test_entropy(out + done, &count);
#endif
        if (result != 0 || !count || count > requested) {
            memset(buffer, 0, length);
            if (!result) errno = EIO;
            return -1;
        }
        done += count;
    }
    return 0;
}

int movian_net_getaddrinfo(const char *node, const char *service,
                           const struct addrinfo *hints, struct addrinfo **result)
{
    if (!result) return EAI_FAIL;
    *result = NULL;
    int family = hints ? hints->ai_family : AF_UNSPEC;
    if (family != AF_UNSPEC && family != AF_INET && family != AF_INET6) return EAI_FAMILY;
    char *end = NULL;
    long port = service ? strtol(service, &end, 10) : 0;
    if (service && (!service[0] || *end || port < 0 || port > 65535)) return EAI_SERVICE;
    struct in_addr ipv4;
    struct in6_addr ipv6;
    if (!node) {
        if (family == AF_UNSPEC) family = AF_INET;
        ipv4.s_addr = htonl(hints && (hints->ai_flags & AI_PASSIVE) ? INADDR_ANY : INADDR_LOOPBACK);
        ipv6 = hints && (hints->ai_flags & AI_PASSIVE) ? in6addr_any : in6addr_loopback;
    } else if (family != AF_INET6 && inet_pton(AF_INET, node, &ipv4) == 1) family = AF_INET;
    else if (family != AF_INET && inet_pton(AF_INET6, node, &ipv6) == 1) family = AF_INET6;
    else return EAI_NONAME;
    struct addrinfo *entry = calloc(1, sizeof(*entry) + sizeof(struct sockaddr_storage));
    if (!entry) return EAI_MEMORY;
    entry->ai_addr = (struct sockaddr *)(entry + 1);
    entry->ai_family = family;
    entry->ai_socktype = hints && hints->ai_socktype ? hints->ai_socktype : SOCK_STREAM;
    entry->ai_protocol = hints && hints->ai_protocol ? hints->ai_protocol : IPPROTO_TCP;
    if (family == AF_INET) {
        struct sockaddr_in *in = (struct sockaddr_in *)entry->ai_addr;
#if defined(__FreeBSD__)
        in->sin_len = sizeof(*in);
#endif
        in->sin_family = AF_INET; in->sin_port = htons(port); in->sin_addr = ipv4;
        entry->ai_addrlen = sizeof(*in);
    } else {
        struct sockaddr_in6 *in = (struct sockaddr_in6 *)entry->ai_addr;
#if defined(__FreeBSD__)
        in->sin6_len = sizeof(*in);
#endif
        in->sin6_family = AF_INET6; in->sin6_port = htons(port); in->sin6_addr = ipv6;
        entry->ai_addrlen = sizeof(*in);
    }
    *result = entry;
    return 0;
}
void movian_net_freeaddrinfo(struct addrinfo *entry)
{
    while (entry) { struct addrinfo *next = entry->ai_next; free(entry); entry = next; }
}
const char *movian_net_gai_strerror(int error)
{
    switch (error) {
    case 0: return "no error";
    case EAI_MEMORY: return "out of memory";
    case EAI_FAMILY: return "unsupported address family";
    case EAI_SERVICE: return "invalid port";
    default: return "numeric address required";
    }
}
int movian_net_getnameinfo(const struct sockaddr *address, socklen_t length,
                           char *host, socklen_t host_length,
                           char *service, socklen_t service_length, int flags)
{
    (void)flags;
    const void *bytes;
    unsigned port;
    if (!address) return EAI_FAIL;
    if (address->sa_family == AF_INET && length >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)address;
        bytes = &in->sin_addr; port = ntohs(in->sin_port);
    } else if (address->sa_family == AF_INET6 && length >= sizeof(struct sockaddr_in6)) {
        const struct sockaddr_in6 *in = (const struct sockaddr_in6 *)address;
        bytes = &in->sin6_addr; port = ntohs(in->sin6_port);
    } else return EAI_FAMILY;
    if (host && host_length && !inet_ntop(address->sa_family, bytes, host, host_length)) return EAI_OVERFLOW;
    if (service && service_length && snprintf(service, service_length, "%u", port) >= (int)service_length) return EAI_OVERFLOW;
    return 0;
}
#endif
