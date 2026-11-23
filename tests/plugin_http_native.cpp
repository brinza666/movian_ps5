// SPDX-License-Identifier: GPL-3.0-or-later
#include "plugin_http.hpp"
#include "diagnostics.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr int refused = static_cast<int>(0x8041013du);
int init_calls=0, live=0, serial=0, read_count=0;
const char *bad="";
std::string diagnostic;
std::vector<std::string> calls;
int call(const char *name, bool resource=false) {
    calls.emplace_back(name);
    if (!std::strcmp(bad,name)) return refused;
    if (resource) {++live;return ++serial;}
    return 0;
}
int drop(int id) {assert(id>0 && live>0);--live;return 0;}
}
void movian::diagnostics::record(const char *,const char *format,...) noexcept {
    char line[512];va_list args;va_start(args,format);vsnprintf(line,sizeof line,format,args);va_end(args);
    diagnostic=line;
}
extern "C" {
int sceNetInit() {++init_calls;return init_calls==1?refused:static_cast<int>(0x80410110u);}
int sceNetPoolCreate(const char *,int,int) {assert(init_calls==2);return call("pool create",true);}
int sceNetPoolDestroy(int id) {return drop(id);}
int sceSslInit(std::size_t) {return call("ssl init",true);}
int sceSslTerm(int id) {return drop(id);}
int sceHttpInit(int,int,std::size_t) {return call("http init",true);}
int sceHttpTerm(int id) {return drop(id);}
int sceHttpCreateTemplate(int,const char *,int,int) {return call("template create",true);}
int sceHttpDeleteTemplate(int id) {return drop(id);}
int sceHttpSetAutoRedirect(int,int) {return call("redirect option");}
int sceHttpSetResolveTimeOut(int,unsigned) {return call("resolve timeout");}
int sceHttpSetConnectTimeOut(int,unsigned) {return call("connect timeout");}
int sceHttpSetSendTimeOut(int,unsigned) {return call("send timeout");}
int sceHttpSetRecvTimeOut(int,unsigned) {return call("receive timeout");}
int sceHttpsEnableOption(int,unsigned) {return call("tls options");}
int sceHttpCreateConnectionWithURL(int,const char *,int) {return call("connection create",true);}
int sceHttpDeleteConnection(int id) {return drop(id);}
int sceHttpCreateRequestWithURL(int,int,const char *,unsigned long) {return call("request create",true);}
int sceHttpDeleteRequest(int id) {return drop(id);}
int sceHttpAddRequestHeader(int,const char *,const char *,int) {return call("request header");}
int sceHttpSendRequest(int,const void *,std::size_t) {read_count=0;return call("send request");}
int sceHttpGetStatusCode(int,int *status) {*status=200;return call("status");}
int sceHttpGetAllResponseHeaders(int,char **out,std::size_t *size) {
    static char headers[]="Content-Type: text/plain\r\n";*out=headers;*size=sizeof headers-1;return 0;
}
int sceHttpReadData(int,void *out,std::size_t size) {
    if(!std::strcmp(bad,"response read")) return refused;
    if(read_count++) return 0;
    assert(size>=3);std::memcpy(out,"abc",3);return 3;
}
}
int main() {
    using namespace movian::plugins;
    char body[16];HttpResponse response{};
    Header header{"Accept","text/plain"};
    auto request=[&] {return http_request("https://fixture.invalid/",false,&header,1,nullptr,0,body,sizeof body,response);};
    assert(request()==refused && init_calls==1 && calls.empty() && live==0);
    assert(diagnostic.find("stage=net init")!=std::string::npos);
    assert(request()==0 && init_calls==2 && live==0 && response.status==200 && response.length==3);
    for(const char *stage:{"pool create","ssl init","http init","template create","redirect option",
        "resolve timeout","connect timeout","send timeout","receive timeout","tls options",
        "connection create","request create","request header","send request","status","response read"}) {
        bad=stage;response.length=9;std::strcpy(response.headers,"stale");calls.clear();
        assert(request()==refused && init_calls==2 && live==0);
        assert(diagnostic.find(std::string("stage=")+stage+" ")!=std::string::npos);
        assert(diagnostic.find("fixture.invalid")==std::string::npos);
        if(std::strcmp(stage,"response read")) assert(response.length==0 && response.headers[0]==0);
    }
    bad="";assert(request()==0 && response.length==3 && live==0);
    assert(http_request(nullptr,false,nullptr,0,nullptr,0,body,sizeof body,response)==-1);
    assert(response.status==0 && response.length==0);
    std::puts("Native HTTP initialization/retry, stage errors, response reset and resource cleanup passed");
}
