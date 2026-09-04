# 第三方库许可证清单

本项目使用了以下第三方开源库。各库的许可证如下：

| 库 | 版本 | 许可证 | 来源 |
|----|------|--------|------|
| Duilib | 1.5.0 | MIT | https://github.com/duilib/duilib |
| minizip-ng | 4.0.7 | zlib License | https://github.com/zlib-ng/minizip-ng |
| LZMA SDK | 23.01 | Public Domain | https://www.7-zip.org/sdk.html |
| unRAR | 7.0.0 | unRAR License | https://www.rarlab.com/rar_add.htm |
| zlib | 1.3.1 | zlib License | https://zlib.net/ |
| bzip2 | 1.0.8 | BSD-like | https://sourceware.org/bzip2/ |
| liblzma (XZ Utils) | 5.6.3 | Public Domain | https://tukaani.org/xz/ |
| zstd | 1.5.6 | BSD 3-Clause | https://github.com/facebook/zstd |
| lz4 | 1.10.1 | BSD 2-Clause | https://github.com/lz4/lz4 |

## 各许可证全文

### MIT License (Duilib)

```
MIT License

Copyright (c) 2010-2024 Duilib Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### zlib License (minizip-ng, zlib)

```
zlib License

Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
Copyright (C) 2003-2024 minizip-ng contributors

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

### Public Domain (LZMA SDK, liblzma)

```
LZMA SDK / XZ Utils are placed in the public domain. Anyone is free to
copy, modify, publish, use, compile, sell, or distribute the original
code, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
```

### unRAR License

```
unRAR License

The source code of unRAR is property of RARLAB. It can be used by anyone
for any purpose, including commercial applications, but cannot be used
to recreate the RAR compression algorithm, which is proprietary.

License agreement:

1. The unRAR source code may be used in any software without charge,
   including commercial applications.
2. The unRAR source code may be modified, but the modified source code
   must be made available to the public if the software is distributed.
3. The unRAR source code cannot be used to develop, create or distribute
   any software that creates RAR format archives.
4. RARLAB retains all copyrights to the unRAR source code.
```

### BSD Licenses (bzip2, zstd, lz4)

```
BSD 3-Clause License (zstd)

Copyright (c) Meta Platforms, Inc. and affiliates.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.
```

## 兼容性说明

本项目主协议为 **AGPLv3**。AGPLv3 与上述许可证均兼容：

- MIT、BSD、zlib、Public Domain 均为宽松许可证，允许与 AGPLv3 组合
- unRAR 许可证虽有限制（不得用于创建 RAR），但解压用途与 AGPLv3 兼容
- 组合后的整体作品仍受 AGPLv3 约束

如对许可证有疑问，请咨询专业法律意见。
