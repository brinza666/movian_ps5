#!/usr/bin/env python3
"""Adapt the pinned public client to the title's tested directory reader."""
from pathlib import Path
import sys

source=Path(sys.argv[1]); text=source.read_text()
def replace_once(old,new):
    global text
    if text.count(old)!=1:
        raise SystemExit('Pinned elevation client changed: '+old[:70])
    text=text.replace(old,new,1)

replace_once('#include "../protocol.hpp"', '''#include "../protocol.hpp"
// Movian adaptation: no privileged backend changes. Retain the public protocol.
#include "../../title_access_io.hpp"
#if defined(MOVIAN_TITLE_ACCESS_LAPY) && !defined(MOVIAN_HOST_PREVIEW)
#define ACCESS_STAGE(stage,result) movian_title_access_trace(stage,result)
#else
#define ACCESS_STAGE(stage,result) ((void)0)
#endif''')
begin=text.index('    DIR *folder = opendir("/data");')
end=text.index('\n    return true;',begin)
old=text[begin:end]
new='''#if defined(MOVIAN_TITLE_ACCESS_LAPY) && !defined(MOVIAN_HOST_PREVIEW)
    ACCESS_STAGE("client directory read begin",0);
    const int listed=movian_title_access_list_directory("/data",&open_error);
    ACCESS_STAGE("client directory read returned",listed<0?open_error:0);
    if(listed!=0) return false;
#else
'''+old+'''
#endif'''
replace_once(old,new)
replacements=[
 ('    const pid_t pid = getpid();','    ACCESS_STAGE("client getpid begin",0);\n    const pid_t pid = getpid();\n    ACCESS_STAGE("client getpid returned",static_cast<int>(pid));'),
 ('    (void)unlink(path.data());\n    File file{open(path.data(), O_RDWR | O_CREAT | O_EXCL, 0600)};',
  '    ACCESS_STAGE("client data probe begin",0);\n    (void)unlink(path.data());\n    File file{open(path.data(), O_RDWR | O_CREAT | O_EXCL, 0600)};\n    ACCESS_STAGE("client data probe open",file.get()<0?errno:0);'),
 ('    File result{open(result_path, O_WRONLY | O_CREAT | O_TRUNC, 0644)};',
  '    ACCESS_STAGE("client resident result open begin",0);\n    File result{open(result_path, O_WRONLY | O_CREAT | O_TRUNC, 0644)};\n    ACCESS_STAGE("client resident result open returned",result.get()<0?errno:0);'),
 ('    (void)fchmod(result.get(), 0644);',
  '    ACCESS_STAGE("client resident fchmod begin",0);\n    (void)fchmod(result.get(), 0644);\n    ACCESS_STAGE("client resident fchmod returned",0);'),
 ('    if (seteuid(geteuid()) != 0)\n        return {Status::prepare_failed, false};',
  '    ACCESS_STAGE("client resident prepare begin",0);\n    if (seteuid(geteuid()) != 0)\n        return {Status::prepare_failed, false};\n    ACCESS_STAGE("client resident prepare returned",0);'),
 ('    if (configured && sceNetConnect(socket, &endpoint, sizeof(endpoint)) >= 0)',
  '    ACCESS_STAGE("client helper connect begin",configured?0:-1);\n    if (configured && sceNetConnect(socket, &endpoint, sizeof(endpoint)) >= 0)'),
 ('    (void)sceNetSocketClose(socket);\n    return status;',
  '    ACCESS_STAGE("client helper transport returned",static_cast<int>(status));\n    (void)sceNetSocketClose(socket);\n    return status;'),
]
for old,new in replacements:replace_once(old,new)
text+='''
// Explicit Movian title test: existing file access is not proof of bind rights.
// Use the same pinned one-shot protocol directly, without publishing a resident
// request or changing the helper's privileged implementation/capability.
namespace elevation {
Status request_owned_for_test(Capability capability,const char *helper_path) noexcept {
    selected_path="none";
    if(capability!=Capability::filesystem) return Status::unsupported_capability;
    wire::Message message{}; message.pid=static_cast<std::uint32_t>(getpid());
    message.capability=capability;
    if(wire::validate(message)!=Status::ok) return Status::invalid_request;
    selected_path="helper";
    ACCESS_STAGE("explicit owned helper request",0);
    const Status result=run_helper(helper_path,message);
    if(result!=Status::ok) return result;
    int open_error=0; bool failed=false;
    return wait_for_data(message.pid,cancellation_grace_polls,open_error,failed)?Status::ok:
        failed?Status::apply_failed:Status::timeout;
}
}
'''
source.write_bytes(text.encode())
