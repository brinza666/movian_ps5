// SPDX-License-Identifier: GPL-3.0-or-later
#include "title_access_io.hpp"
#include "fs.hpp"
extern "C" int movian_title_access_list_directory(const char *path,int *error) noexcept {
    // fs::list_dir uses the console's established 64-KiB getdents path. An
    // opendir handle alone does not prove a single directory entry can be read.
    movian::fs::Entry entry[1]; int count=0,reason=0;
    const int result=movian::fs::list_dir(path,entry,1,&count,&reason,true);
    if(error) *error=result==0?0:reason;
    return result;
}
