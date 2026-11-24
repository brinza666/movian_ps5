#!/usr/bin/env python3
"""Run the pinned public client regressions plus explicit-owned test cases."""
from pathlib import Path
import shutil
import subprocess
import sys

starter=Path(sys.argv[1]); root=Path(sys.argv[2])/'title-access-client'
base=root/'examples/sandbox-elevation'
(base/'src').mkdir(parents=True); (root/'tests').mkdir()
for name in ('elevation.hpp','protocol.hpp'):
    shutil.copyfile(starter/'kit/examples/sandbox-elevation'/name,base/name)
shutil.copyfile(starter/'kit/examples/sandbox-elevation/src/elevation.cpp',base/'src/elevation.cpp')
shutil.copyfile('app/src/title_access_io.hpp',root/'examples/title_access_io.hpp')
subprocess.run([sys.executable,'scripts/adapt-title-access-client.py',str(base/'src/elevation.cpp')],check=True)
text=(starter/'kit/tests/test_elevation.cpp').read_text()
marker='    elevation::wire::Message request{};'
assert text.count(marker)==1
checks='''
    // Existing file/list access must not bypass an explicit owned test request.
    test::reset(); test::helper_replies();
    assert(elevation::request_owned_for_test(Capability::filesystem,"/app0/lapy.elf")==Status::ok);
    assert(std::strcmp(elevation::path(),"helper")==0);
    assert(test::state.socket_calls==1 && test::state.prepare_calls==1);
    assert(test::state.request.empty() && test::state.result.empty());
    assert(test::state.sent.size()==4+2*sizeof(elevation::wire::Message));
    test::reset(); test::helper_replies(Status::target_mismatch);
    assert(elevation::request_owned_for_test(Capability::filesystem,"/app0/lapy.elf")==Status::target_mismatch);
    assert(test::state.socket_calls==1 && test::state.data_open_calls==0);
    test::reset(); test::helper_replies(); test::state.corrupt_read=true;
    assert(elevation::request_owned_for_test(Capability::filesystem,"/app0/lapy.elf")==Status::apply_failed);
    assert(test::state.socket_calls==1);
    test::reset();
    assert(elevation::request_owned_for_test(static_cast<Capability>(2),"/app0/lapy.elf")==Status::unsupported_capability);
    assert(test::state.socket_calls==0);
    assert(elevation::request_owned_for_test(Capability::filesystem,nullptr)==Status::invalid_request);
    assert(test::state.socket_calls==0);
'''
(root/'tests/test_elevation.cpp').write_bytes(text.replace(marker,checks+marker).encode())
binary=root/'client-test'
subprocess.run([sys.argv[3],'-std=c++20','-Wall','-Wextra','-Werror',str(root/'tests/test_elevation.cpp'),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
print('Public elevation client and explicit owned-request/ack/proof regressions passed')
