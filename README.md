# The FastCGI Process Manager for HVML

## Introduction

This program, `hvml-fpm`, is a primary HVML FastCGI implementation like `php-fpm`.
It can run as a gateway program of a Web server, e.g., lighttpd or Ngnix.

This program uses source code from

- [spawn-fcgi](https://github.com/lighttpd/spawn-fcgi)
- [libfcgi](https://github.com/FastCGI-Archives/fcgi2)
- [Multipart form data parser](https://github.com/iafonov/multipart-parser-c)

## Usage

```console
$ hvmlfpm -h
Usage: hvmlfpm [options]

hvml-fpm v0.0.6 (ipv6) - the FastCGI process manager for HVML

Options:
 -A <app_name>     HVML app name
 -i <init_script>  path of initialization script
 -q <script_query> query for initialization script
 -d <directory>    chdir to directory before spawning
 -a <address>      bind to IPv4/IPv6 address (defaults to 0.0.0.0)
 -p <port>         bind to TCP-port
 -s <path>         bind to Unix domain socket
 -M <mode>         change Unix domain socket mode (octal integer,
                       default: allow read+write for user and group
                       as far as umask allows it)
 -F <children>     number of children to fork (default 1)
 -b <backlog>      backlog to allow on the socket (default 1024)
 -P <path>         name of PID-file for spawned worker processes
 -e                the maximum number of total executions
                       (default 1000)
 -v                show version
 -?, -h            show this help
(root only)
 -c <directory>    chroot to directory
 -S                create socket before chroot() (default is
                       to create the socket in the chroot)
 -u <user>         change to user-id
 -g <group>        change to group-id (default: primary group of
                       user if -u is given)
 -U <user>         change Unix domain socket owner to user-id
 -G <group>        change Unix domain socket group to group-id

```

For example, you can start an HVML-FPM daemon for HVML app `cn.fmsoft.hvml.purc` with the following command line:

```console
$ sudo hvmlfpm -A cn.fmsoft.hvml.purc -s /var/run/hvmlfpm-cn.fmsoft.hvml.purc.sock -U www-data -G www-data -F 10
```

## Copying

Copyright (C) 2023 ~ 2025 [FMSoft Technologies]  

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

### spawn-fcgi

Copyright (c) 2004, Jan Kneschke, incremental.  
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

- Neither the name of the 'incremental' nor the names of its contributors may
  be used to endorse or promote products derived from this software without
  specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.

### libfcgi

This FastCGI application library source and object code (the
"Software") and its documentation (the "Documentation") are
copyrighted by Open Market, Inc ("Open Market").  The following terms
apply to all files associated with the Software and Documentation
unless explicitly disclaimed in individual files.

Open Market permits you to use, copy, modify, distribute, and license
this Software and the Documentation for any purpose, provided that
existing copyright notices are retained in all copies and that this
notice is included verbatim in any distributions.  No written
agreement, license, or royalty fee is required for any of the
authorized uses.  Modifications to this Software and Documentation may
be copyrighted by their authors and need not follow the licensing
terms described here.  If modifications to this Software and
Documentation have new licensing terms, the new terms must be clearly
indicated on the first page of each file where they apply.

OPEN MARKET MAKES NO EXPRESS OR IMPLIED WARRANTY WITH RESPECT TO THE
SOFTWARE OR THE DOCUMENTATION, INCLUDING WITHOUT LIMITATION ANY
WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  IN
NO EVENT SHALL OPEN MARKET BE LIABLE TO YOU OR ANY THIRD PARTY FOR ANY
DAMAGES ARISING FROM OR RELATING TO THIS SOFTWARE OR THE
DOCUMENTATION, INCLUDING, WITHOUT LIMITATION, ANY INDIRECT, SPECIAL OR
CONSEQUENTIAL DAMAGES OR SIMILAR DAMAGES, INCLUDING LOST PROFITS OR
LOST DATA, EVEN IF OPEN MARKET HAS BEEN ADVISED OF THE POSSIBILITY OF
SUCH DAMAGES.  THE SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS".
OPEN MARKET HAS NO LIABILITY IN CONTRACT, TORT, NEGLIGENCE OR
OTHERWISE ARISING OUT OF THIS SOFTWARE OR THE DOCUMENTATION.

### Multipart form data parser

Copyright (C) 2012, Igor Afonov.  
All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the “Software”), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Tradmarks

1) 飛漫

![飛漫](https://www.fmsoft.cn/application/files/cache/thumbnails/87f47bb9aeef9d6ecd8e2ffa2f0e2cb6.jpg)

2) FMSoft

![FMSoft](https://www.fmsoft.cn/application/files/cache/thumbnails/44a50f4b2a07e2aef4140a23d33f164e.jpg)

3) 合璧

![合璧](https://www.fmsoft.cn/application/files/4716/1180/1904/256132.jpg)
![合璧](https://www.fmsoft.cn/application/files/cache/thumbnails/9c57dee9df8a6d93de1c6f3abe784229.jpg)
![合壁](https://www.fmsoft.cn/application/files/cache/thumbnails/f59f58830eccd57e931f3cb61c4330ed.jpg)

4) HybridOS

![HybridOS](https://www.fmsoft.cn/application/files/cache/thumbnails/5a85507f3d48cbfd0fad645b4a6622ad.jpg)

5) HybridRun

![HybridRun](https://www.fmsoft.cn/application/files/cache/thumbnails/84934542340ed662ef99963a14cf31c0.jpg)

6) MiniGUI

![MiniGUI](https://www.fmsoft.cn/application/files/cache/thumbnails/54e87b0c49d659be3380e207922fff63.jpg)

7) xGUI

![xGUI](https://www.fmsoft.cn/application/files/cache/thumbnails/7fbcb150d7d0747e702fd2d63f20017e.jpg)

8) miniStudio

![miniStudio](https://www.fmsoft.cn/application/files/cache/thumbnails/82c3be63f19c587c489deb928111bfe2.jpg)

9) HVML

![HVML](https://www.fmsoft.cn/application/files/8116/1931/8777/HVML256132.jpg)

10) 呼噜猫

![呼噜猫](https://www.fmsoft.cn/application/files/8416/1931/8781/256132.jpg)

11) Purring Cat

![Purring Cat](https://www.fmsoft.cn/application/files/2816/1931/9258/PurringCat256132.jpg)

12) PurC

![PurC](https://www.fmsoft.cn/application/files/5716/2813/0470/PurC256132.jpg)

[Beijing FMSoft Technologies Co., Ltd.]: https://www.fmsoft.cn
[FMSoft Technologies]: https://www.fmsoft.cn
[FMSoft]: https://www.fmsoft.cn
[HybridOS Official Site]: https://hybridos.fmsoft.cn
[HybridOS]: https://hybridos.fmsoft.cn

[HVML]: https://github.com/HVML
[Vincent Wei]: https://github.com/VincentWei
[MiniGUI]: https://github.com/VincentWei/minigui

