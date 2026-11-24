// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
extern "C" int movian_title_access_list_directory(const char *path,int *error) noexcept;
extern "C" void movian_title_access_trace(const char *stage,int result) noexcept;
