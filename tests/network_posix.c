// SPDX-License-Identifier: GPL-3.0-or-later
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
int movian_net_random_bytes(void *, size_t);
static int entropy_mode, entropy_calls;
int movian_net_test_entropy(void *out, size_t *length) {
    ++entropy_calls;
    if (entropy_mode == 1) { errno=EIO; return -1; }
    if (entropy_mode == 2) { *length=0; return 0; }
    if (entropy_mode == 3 && *length>3) *length=3;
    if (entropy_mode == 4) { ++*length; return 0; }
    memset(out,0xa0+entropy_calls,*length);
    return 0;
}
int movian_net_getaddrinfo(const char *, const char *, const struct addrinfo *, struct addrinfo **);
void movian_net_freeaddrinfo(struct addrinfo *);
int movian_net_getnameinfo(const struct sockaddr *, socklen_t, char *, socklen_t, char *, socklen_t, int);
int main(void) {
    unsigned char bytes[600];
    assert(movian_net_random_bytes(bytes,sizeof bytes)==0 && entropy_calls==3);
    assert(bytes[0]==0xa1 && bytes[256]==0xa2 && bytes[512]==0xa3);
    entropy_mode=3;entropy_calls=0;
    assert(movian_net_random_bytes(bytes,8)==0 && entropy_calls==3);
    for(int mode=1;mode<=4;++mode) if(mode!=3) {
        entropy_mode=mode;memset(bytes,0xff,sizeof bytes);
        assert(movian_net_random_bytes(bytes,sizeof bytes)<0);
        for(unsigned i=0;i<sizeof bytes;++i) assert(bytes[i]==0);
    }
    assert(movian_net_random_bytes(NULL,0)==0);
    assert(movian_net_random_bytes(NULL,1)<0 && errno==EINVAL);
    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_UNSPEC;
    assert(movian_net_getaddrinfo("192.168.10.1","445",&hints,&result)==0);
    assert(result->ai_family==AF_INET && ntohs(((struct sockaddr_in *)result->ai_addr)->sin_port)==445);
    char host[80], service[8];
    assert(movian_net_getnameinfo(result->ai_addr,result->ai_addrlen,host,sizeof host,service,sizeof service,0)==0);
    assert(!strcmp(host,"192.168.10.1") && !strcmp(service,"445"));
    movian_net_freeaddrinfo(result);
    assert(movian_net_getaddrinfo("::1","21",&hints,&result)==0 && result->ai_family==AF_INET6);
    assert(movian_net_getnameinfo(result->ai_addr,result->ai_addrlen,host,sizeof host,service,sizeof service,0)==0);
    assert(!strcmp(host,"::1") && !strcmp(service,"21"));
    movian_net_freeaddrinfo(result);
    hints.ai_family=AF_INET;
    assert(movian_net_getaddrinfo("::1","21",&hints,&result)!=0 && !result);
    assert(movian_net_getaddrinfo("unresolved.local","21",&hints,&result)==EAI_NONAME);
    assert(movian_net_getaddrinfo("127.0.0.1","65536",&hints,&result)==EAI_SERVICE);
    hints.ai_flags=AI_PASSIVE;
    assert(movian_net_getaddrinfo(NULL,"7777",&hints,&result)==0);
    assert(((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr==htonl(INADDR_ANY));
    movian_net_freeaddrinfo(result);
    puts("PS5 numeric IPv4/IPv6 address adapter passed");
}
