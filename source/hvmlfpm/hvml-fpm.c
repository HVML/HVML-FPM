/*
 * @file hvml-fpm.c
 * @author Vincent Wei
 * @date 2022/10/16
 * @brief The main entry of hvml-fpm.
 *
 * Copyright (C) 2023 FMSoft <https://www.fmsoft.cn>
 *
 * This file is a part of hvml-fpm, which is an HVML FastCGI implementation.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file is derived from spawn-fcgi (<https://github.com/lighttpd/spawn-fcgi>).
 *
 * Copyright (C) 2004, Jan Kneschke, incremental.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the 'incremental' nor the names of its contributors may
 *   be used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>

#if HAVE(PWD_H)
# include <grp.h>
# include <pwd.h>
#endif

#if HAVE(GETOPT_H)
# include <getopt.h>
#endif

#define FCGI_LISTENSOCK_FILENO 0

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <netdb.h>

#if HAVE(SYS_WAIT_H)
# include <sys/wait.h>
#endif

#include "hvml-fpm.h"
#include "hvml-executor.h"

/* for solaris 2.5 and netbsd 1.3.x */
#if !HAVE(SOCKLEN_T)
typedef unsigned int socklen_t;
#endif

#if !HAVE(ISSETUGID)
static int issetugid() {
    return (geteuid() != getuid() || getegid() != getgid());
}
#endif

#if HAVE(IPV6) && HAVE(INET_PTON)
# define USE_IPV6
#endif

#ifdef USE_IPV6
#define PACKAGE_FEATURES " (ipv6)"
#else
#define PACKAGE_FEATURES ""
#endif

#define PACKAGE_DESC "hvml-fpm v" PACKAGE_VERSION PACKAGE_FEATURES " - the FastCGI process manager for HVML\n"

#define CONST_STR_LEN(s) s, sizeof(s) - 1

static mode_t read_umask(void)
{
    mode_t mask = umask(0);
    umask(mask);
    return mask;
}

static ssize_t
write_all(int fildes, const void *buf, size_t nbyte)
{
    size_t rem;
    for (rem = nbyte; rem > 0;) {
        ssize_t res = write(fildes, buf, rem);
        if (-1 == res) {
            if (EINTR != errno) return res;
        } else {
            buf = res + (char const*) buf;
            rem -= res;
        }
    }
    return nbyte;
}

static int
bind_socket(const char *addr, unsigned short port, const char *unixsocket,
        uid_t uid, gid_t gid, mode_t mode, int backlog)
{
    int fcgi_fd, socket_type, val;

    struct sockaddr_un fcgi_addr_un;
    struct sockaddr_in fcgi_addr_in;
#ifdef USE_IPV6
    struct sockaddr_in6 fcgi_addr_in6;
#endif
    struct sockaddr *fcgi_addr;

    socklen_t servlen;

    if (unixsocket) {
        memset(&fcgi_addr_un, 0, sizeof(fcgi_addr_un));

        fcgi_addr_un.sun_family = AF_UNIX;
        /* already checked in main() */
        if (strlen(unixsocket) > sizeof(fcgi_addr_un.sun_path) - 1) return -1;
        strcpy(fcgi_addr_un.sun_path, unixsocket);

#ifdef SUN_LEN
        servlen = SUN_LEN(&fcgi_addr_un);
#else
        /* stevens says: */
        servlen = strlen(fcgi_addr_un.sun_path) +
            sizeof(fcgi_addr_un.sun_family);
#endif
        socket_type = AF_UNIX;
        fcgi_addr = (struct sockaddr *) &fcgi_addr_un;

        /* check if some backend is listening on the socket
         * as if we delete the socket-file and rebind there will be no
         * "socket already in use" error.
         */
        if (-1 == (fcgi_fd = socket(socket_type, SOCK_STREAM, 0))) {
            fprintf(stderr, "hvml-fpm: couldn't create socket: %s\n",
                    strerror(errno));
            return -1;
        }

        if (0 == connect(fcgi_fd, fcgi_addr, servlen)) {
            fprintf(stderr, "hvml-fpm: socket is already in use, can't spawn\n");
            close(fcgi_fd);
            return -1;
        }

        /* cleanup previous socket if it exists */
        if (-1 == unlink(unixsocket)) {
            switch (errno) {
            case ENOENT:
                break;
            default:
                fprintf(stderr, "hvml-fpm: removing old socket failed: %s\n",
                        strerror(errno));
                close(fcgi_fd);
                return -1;
            }
        }

        close(fcgi_fd);
    } else {
        memset(&fcgi_addr_in, 0, sizeof(fcgi_addr_in));
        fcgi_addr_in.sin_family = AF_INET;
        fcgi_addr_in.sin_port = htons(port);

        servlen = sizeof(fcgi_addr_in);
        socket_type = AF_INET;
        fcgi_addr = (struct sockaddr *) &fcgi_addr_in;

#ifdef USE_IPV6
        memset(&fcgi_addr_in6, 0, sizeof(fcgi_addr_in6));
        fcgi_addr_in6.sin6_family = AF_INET6;
        fcgi_addr_in6.sin6_port = fcgi_addr_in.sin_port;
#endif

        if (addr == NULL) {
            fcgi_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
#if HAVE(INET_PTON)
        } else if (1 == inet_pton(AF_INET, addr, &fcgi_addr_in.sin_addr)) {
            /* nothing to do */
#if HAVE(IPV6)
        } else if (1 == inet_pton(AF_INET6, addr, &fcgi_addr_in6.sin6_addr)) {
            servlen = sizeof(fcgi_addr_in6);
            socket_type = AF_INET6;
            fcgi_addr = (struct sockaddr *) &fcgi_addr_in6;
#endif
        } else {
            fprintf(stderr, "hvml-fpm: '%s' is not a valid IP address\n",
                    addr);
            return -1;
#else
        } else {
            if ((in_addr_t)(-1) == (fcgi_addr_in.sin_addr.s_addr =
                        inet_addr(addr))) {
                fprintf(stderr, "hvml-fpm: '%s' is not a valid IPv4 address\n",
                        addr);
                return -1;
            }
#endif
        }
    }


    if (-1 == (fcgi_fd = socket(socket_type, SOCK_STREAM, 0))) {
        fprintf(stderr, "hvml-fpm: couldn't create socket: %s\n",
                strerror(errno));
        return -1;
    }

    val = 1;
    if (setsockopt(fcgi_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        fprintf(stderr, "hvml-fpm: couldn't set SO_REUSEADDR: %s\n",
                strerror(errno));
        close(fcgi_fd);
        return -1;
    }

    if (-1 == bind(fcgi_fd, fcgi_addr, servlen)) {
        fprintf(stderr, "hvml-fpm: bind failed: %s\n", strerror(errno));
        close(fcgi_fd);
        return -1;
    }

    if (unixsocket) {
        if (-1 == chmod(unixsocket, mode)) {
            fprintf(stderr, "hvml-fpm: couldn't chmod socket: %s\n",
                    strerror(errno));
            close(fcgi_fd);
            unlink(unixsocket);
            return -1;
        }

        if (0 != uid || 0 != gid) {
            if (0 == uid) uid = -1;
            if (0 == gid) gid = -1;
            if (-1 == chown(unixsocket, uid, gid)) {
                fprintf(stderr, "hvml-fpm: couldn't chown socket: %s\n",
                        strerror(errno));
                close(fcgi_fd);
                unlink(unixsocket);
                return -1;
            }
        }
    }

    if (-1 == listen(fcgi_fd, backlog)) {
        fprintf(stderr, "hvml-fpm: listen failed: %s\n", strerror(errno));
        close(fcgi_fd);
        if (unixsocket) unlink(unixsocket);
        return -1;
    }

    return fcgi_fd;
}

static void call_executor(const char *hvmlApp, const char *initScript,
        const char *scriptQuery, int fcgi_fd, int max_executions)
{
    int max_fd = 0;
    int i = 0;

    if (fcgi_fd != FCGI_LISTENSOCK_FILENO) {
        close(FCGI_LISTENSOCK_FILENO);
        dup2(fcgi_fd, FCGI_LISTENSOCK_FILENO);
        close(fcgi_fd);
    }

    /* loose control terminal */
    setsid();

    max_fd = open("/dev/null", O_RDWR);
    if (-1 != max_fd) {
        if (max_fd != STDOUT_FILENO)
            dup2(max_fd, STDOUT_FILENO);
        if (max_fd != STDERR_FILENO)
            dup2(max_fd, STDERR_FILENO);
        if (max_fd != STDOUT_FILENO && max_fd != STDERR_FILENO)
            close(max_fd);
    } else {
        syslog(LOG_ERR, "hvml-fpm: couldn't open and redirect "
                "stdout/stderr to '/dev/null': %s\n",
                strerror(errno));
    }

    /* we don't need the client socket */
    for (i = 3; i < max_fd; i++) {
        if (i != FCGI_LISTENSOCK_FILENO)
            close(i);
    }

    exit(hvml_executor(hvmlApp, initScript, scriptQuery,
                max_executions, true));
}

static int
fcgi_spawn_connection(const char *hvmlApp, const char *initScript,
        const char *scriptQuery, int fcgi_fd, int fork_count, int pid_fd,
        int max_executions)
{
    int status, rc = 0;
    struct timeval tv = { 0, 100 * 1000 };

    pid_t child;

    if (fork_count > 0) {
        while (fork_count-- > 0) {

            // syslog(LOG_INFO, "calling fork(): %d\n", getpid());
            child = fork();

            if (child == 0) {
                call_executor(hvmlApp, initScript, scriptQuery, fcgi_fd,
                        max_executions);
            }
            else if (child > 0) {
                /* father */

                /* wait a moment */
                select(0, NULL, NULL, NULL, &tv);

                switch (waitpid(child, &status, WNOHANG)) {
                case 0:
                    syslog(LOG_INFO, "child spawned successfully: PID: %d\n",
                            child);

                    /* write pid file */
                    if (-1 != pid_fd) {
                        char pidbuf[32];
                        snprintf(pidbuf, sizeof(pidbuf), "%d\n", child);
                        if (-1 == write_all(pid_fd, pidbuf, strlen(pidbuf))) {
                            syslog(LOG_WARNING, "writing pid file failed: %s\n",
                                    strerror(errno));
                            close(pid_fd);
                            pid_fd = -1;
                        }
                        /* avoid eol for the last one
                        if (-1 != pid_fd && fork_count != 0) {
                            if (-1 == write_all(pid_fd, "\n", 1)) {
                                syslog(LOG_WARNING, "writing pid file failed: %s\n",
                                        strerror(errno));
                                close(pid_fd);
                                pid_fd = -1;
                            }
                        }*/
                    }
                    break;

                case -1:
                    syslog(LOG_ERR, "failed waitpid(): %s\n", strerror(errno));
                    break;

                default:
                    if (WIFEXITED(status)) {
                        syslog(LOG_WARNING, "child exited with: %d\n",
                            WEXITSTATUS(status));
                        rc = WEXITSTATUS(status);
                    }
                    else if (WIFSIGNALED(status)) {
                        syslog(LOG_WARNING, "child signaled: %d\n",
                            WTERMSIG(status));
                        rc = 1;
                    }
                    else {
                        syslog(LOG_WARNING, "child died somehow: exit status = %d\n",
                                status);
                        rc = status;
                    }

                    break;
                } /* switch waitpid() */
            }
            else  {
                /* error */
                syslog(LOG_ERR, "fork failed: %s\n", strerror(errno));
                rc = -1;
            }
        }
    }
    else {
        /* no fork */
        call_executor(hvmlApp, initScript, scriptQuery, fcgi_fd,
                max_executions);
    }

    return rc;
}

static int
find_user_group(const char *user, const char *group, uid_t *uid, gid_t *gid,
        const char **username)
{
    uid_t my_uid = 0;
    gid_t my_gid = 0;
    struct passwd *my_pwd = NULL;
    struct group *my_grp = NULL;
    char *endptr = NULL;
    *uid = 0; *gid = 0;
    if (username) *username = NULL;

    if (user) {
        my_uid = strtol(user, &endptr, 10);

        if (my_uid <= 0 || *endptr) {
            if (NULL == (my_pwd = getpwnam(user))) {
                fprintf(stderr, "hvml-fpm: can't find user name %s\n", user);
                return -1;
            }
            my_uid = my_pwd->pw_uid;

            if (my_uid == 0) {
                fprintf(stderr, "hvml-fpm: I will not set uid to 0\n");
                return -1;
            }

            if (username) *username = user;
        } else {
            my_pwd = getpwuid(my_uid);
            if (username && my_pwd) *username = my_pwd->pw_name;
        }
    }

    if (group) {
        my_gid = strtol(group, &endptr, 10);

        if (my_gid <= 0 || *endptr) {
            if (NULL == (my_grp = getgrnam(group))) {
                fprintf(stderr, "hvml-fpm: can't find group name %s\n", group);
                return -1;
            }
            my_gid = my_grp->gr_gid;

            if (my_gid == 0) {
                fprintf(stderr, "hvml-fpm: I will not set gid to 0\n");
                return -1;
            }
        }
    } else if (my_pwd) {
        my_gid = my_pwd->pw_gid;

        if (my_gid == 0) {
            fprintf(stderr, "hvml-fpm: I will not set gid to 0\n");
            return -1;
        }
    }

    *uid = my_uid;
    *gid = my_gid;
    return 0;
}

static void show_version (void)
{
    (void) write_all(1, CONST_STR_LEN(
        PACKAGE_DESC
    ));
}

static void show_help (void)
{
    (void) write_all(1, CONST_STR_LEN(
        "Usage: hvmlfpm [options]\n" \
        "\n" \
        PACKAGE_DESC \
        "\n" \
        "Options:\n" \
        " -A <app_name>     HVML app name\n"
        " -i <init_script>  path of initialization script\n"
        " -q <script_query> query for initialization script\n"
        " -d <directory>    chdir to directory before spawning\n"
        " -a <address>      bind to IPv4/IPv6 address (defaults to 0.0.0.0)\n"
        " -p <port>         bind to TCP-port\n"
        " -s <path>         bind to Unix domain socket\n"
        " -M <mode>         change Unix domain socket mode (octal integer,\n"
        "                       default: allow read+write for user and group\n"
        "                       as far as umask allows it)\n"
        " -F <children>     number of children to fork (default 1)\n"
        " -b <backlog>      backlog to allow on the socket (default 1024)\n"
        " -P <path>         name of PID-file for spawned worker processes\n"
        " -e                the maximum number of total executions\n"
        "                       (default 1000)\n"
        " -v                show version\n"
        " -?, -h            show this help\n"
        "(root only)\n" \
        " -c <directory>    chroot to directory\n"
        " -S                create socket before chroot() (default is\n"
        "                       to create the socket in the chroot)\n"
        " -u <user>         change to user-id\n"
        " -g <group>        change to group-id (default: primary group of\n"
        "                       user if -u is given)\n"
        " -U <user>         change Unix domain socket owner to user-id\n"
        " -G <group>        change Unix domain socket group to group-id\n"
    ));
}

static int daemonize(void)
{
    pid_t pid;

    int fd = open("/dev/null", O_RDWR);
    if (fd < 0)
        return -1;

    if (dup2(fd, 0) < 0 || dup2(fd, 1) < 0 || dup2(fd, 2) < 0) {
        close(fd);
        return -1;
    }

    close(fd);

    pid = fork();
    if (pid < 0)
        return -1;

    if (pid > 0)
        _exit(0);

    if (setsid() < 0)
        return -1;

    return 0;
}

int main(int argc, char **argv)
{
    char *hvml_app = NULL, *init_script = NULL, *script_query = NULL,
         *changeroot = NULL, *username = NULL,
         *groupname = NULL, *unixsocket = NULL, *pid_file = NULL,
         *sockusername = NULL, *sockgroupname = NULL, *fcgi_dir = NULL,
         *addr = NULL;
    char *endptr = NULL;
    unsigned short port = 0;
    mode_t sockmode =  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) & ~read_umask();
    int fork_count = 0;
    int max_executions = 1000;
    int backlog = 1024;
    int i_am_root, o;
    int pid_fd = -1;
    int sockbeforechroot = 0;
    struct sockaddr_un un;
    int fcgi_fd = -1;

    if (argc < 2) { /* no arguments given */
        show_help();
        return -1;
    }

    i_am_root = (getuid() == 0);

    while (-1 != (o = getopt(argc, argv,
                    "c:d:A:i:q:g:?ha:p:b:u:vC:F:e:s:P:U:G:M:S"))) {
        switch(o) {
        case 'A': hvml_app = optarg; break;
        case 'i': init_script = optarg; break;
        case 'q': script_query = optarg; break;
        case 'd': fcgi_dir = optarg; break;
        case 'a': addr = optarg;/* ip addr */ break;
        case 'p': port = strtol(optarg, &endptr, 10);/* port */
            if (*endptr) {
                fprintf(stderr, "hvml-fpm: invalid port: %u\n",
                        (unsigned int)port);
                return -1;
            }
            break;
        case 'F': fork_count = strtol(optarg, NULL, 10); break;
        case 'e': max_executions = strtol(optarg, NULL, 10); break;
        case 'b': backlog = strtol(optarg, NULL, 10); break;
        /* unix-domain socket */
        case 's': unixsocket = optarg; break;
        /* chroot() */
        case 'c': if (i_am_root) { changeroot = optarg; } break;
        /* set user */
        case 'u': if (i_am_root) { username = optarg; } break;
        /* set group */
        case 'g': if (i_am_root) { groupname = optarg; } break;
        /* set socket user */
        case 'U': if (i_am_root) { sockusername = optarg; } break;
        /* set socket group */
        case 'G': if (i_am_root) { sockgroupname = optarg; } break;
        /* open socket before chroot() */
        case 'S': if (i_am_root) { sockbeforechroot = 1; } break;
        /* set socket mode */ 
        case 'M': sockmode = strtol(optarg, NULL, 8); break;
        /* PID file */ 
        case 'P': pid_file = optarg; break;
        case 'v': show_version(); return 0;
        case '?':
        case 'h': show_help(); return 0;
        default:
            show_help();
            return -1;
        }
    }

    if (NULL == hvml_app) {
        fprintf(stderr, "hvml-fpm: no HVML app name given\n");
        return -1;
    }

    if (0 == port && NULL == unixsocket) {
        fprintf(stderr, "hvml-fpm: no socket given (use either -p or -s)\n");
        return -1;
    } else if (0 != port && NULL != unixsocket) {
        fprintf(stderr, "hvml-fpm: either a Unix domain socket or a TCP-port, "
                "but not both\n");
        return -1;
    }

    if (unixsocket && strlen(unixsocket) > sizeof(un.sun_path) - 1) {
        fprintf(stderr, "hvml-fpm: path of the Unix domain socket is "
                "too long\n");
        return -1;
    }

    /* SUID handling */
    if (!i_am_root && issetugid()) {
        fprintf(stderr, "hvml-fpm: Are you nuts? Don't apply a SUID bit to "
                "this binary\n");
        return -1;
    }

    if (pid_file &&
        (-1 == (pid_fd = open(pid_file, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC,
                              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)))) {
        struct stat st;
        if (errno != EEXIST) {
            fprintf(stderr, "hvml-fpm: opening PID-file '%s' failed: %s\n",
                pid_file, strerror(errno));
            return -1;
        }

        /* ok, file exists */

        if (0 != stat(pid_file, &st)) {
            fprintf(stderr, "hvml-fpm: stating PID-file '%s' failed: %s\n",
                pid_file, strerror(errno));
            return -1;
        }

        /* is it a regular file ? */

        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "hvml-fpm: PID-file exists and isn't regular file:"
                    "'%s'\n", pid_file);
            return -1;
        }

        if (-1 == (pid_fd = open(pid_file, O_WRONLY | O_CREAT | O_TRUNC,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) {
            fprintf(stderr, "hvml-fpm: opening PID-file '%s' failed: %s\n",
                pid_file, strerror(errno));
            return -1;
        }
    }

    if (i_am_root) {
        uid_t uid, sockuid;
        gid_t gid, sockgid;
        const char* real_username;

        if (-1 == find_user_group(username, groupname, &uid, &gid,
                    &real_username))
            return -1;

        if (-1 == find_user_group(sockusername, sockgroupname, &sockuid,
                    &sockgid, NULL))
            return -1;

        if (uid != 0 && gid == 0) {
            fprintf(stderr, "hvml-fpm: WARNING: couldn't find the user for "
                    "uid %i and no group was specified, so only the user "
                    "privileges will be dropped\n", (int)uid);
        }

        if (0 == sockuid)
            sockuid = uid;
        if (0 == sockgid)
            sockgid = gid;

        if (sockbeforechroot && -1 == (fcgi_fd = bind_socket(addr, port,
                        unixsocket, sockuid, sockgid, sockmode, backlog)))
            return -1;

        /* Change group before chroot, when we have access
         * to /etc/group
         */
        if (gid != 0) {
            if (-1 == setgid(gid)) {
                fprintf(stderr, "hvml-fpm: setgid(%i) failed: %s\n",
                        (int)gid, strerror(errno));
                return -1;
            }
            if (-1 == setgroups(0, NULL)) {
                fprintf(stderr, "hvml-fpm: setgroups(0, NULL) failed: %s\n",
                        strerror(errno));
                return -1;
            }
            if (real_username) {
                if (-1 == initgroups(real_username, gid)) {
                    fprintf(stderr, "hvml-fpm: initgroups('%s', %i) "
                            "failed: %s\n", real_username, (int)gid,
                            strerror(errno));
                    return -1;
                }
            }
        }

        if (changeroot) {
            if (-1 == chroot(changeroot)) {
                fprintf(stderr, "hvml-fpm: chroot('%s') failed: %s\n",
                        changeroot, strerror(errno));
                return -1;
            }
            if (-1 == chdir("/")) {
                fprintf(stderr, "hvml-fpm: chdir('/') failed: %s\n",
                        strerror(errno));
                return -1;
            }
        }

        if (!sockbeforechroot && -1 == (fcgi_fd = bind_socket(addr, port,
                        unixsocket, sockuid, sockgid, sockmode, backlog)))
            return -1;

        /* drop root privs */
        if (uid != 0) {
            if (-1 == setuid(uid)) {
                fprintf(stderr, "hvml-fpm: setuid(%i) failed: %s\n",
                        (int)uid, strerror(errno));
                return -1;
            }
        }
    } else {
        if (-1 == (fcgi_fd = bind_socket(addr, port, unixsocket, 0, 0,
                        sockmode, backlog)))
            return -1;
    }

    if (fcgi_dir && -1 == chdir(fcgi_dir)) {
        fprintf(stderr, "hvml-fpm: chdir('%s') failed: %s\n",
                fcgi_dir, strerror(errno));
        return -1;
    }

    if (init_script && access(init_script, R_OK)) {
        fprintf(stderr, "hvml-fpm: can not read init script: %s\n",
                init_script);
        return -1;
    }

    if (fork_count > 0) {
        fprintf(stdout, "hvml-fpm: initialization succeed; "
                "going to be a daemon...\n");
        if (daemonize()) {
            fprintf(stderr, "hvml-fpm: failed daemonize(): %s\n",
                    strerror(errno));
            return -1;
        }
    }

    int rc;
    openlog("hvml-fpm", LOG_PID, LOG_USER);
    rc = fcgi_spawn_connection(hvml_app, init_script, script_query,
            fcgi_fd, fork_count, pid_fd, max_executions);
    if (rc) {
        syslog(LOG_ERR, "Failed fcgi_spawn_connection(): %d\n", rc);
        goto done;
    }

    while (true) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid == -1) {
            syslog(LOG_ERR, "Failed waitpid(): %s\n", strerror(errno));
            break;
        }

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            syslog(LOG_ERR, "Child (%d) exited with: %d\n", pid, exit_code);

            if (exit_code != EXIT_FAILURE) {
                // fork a new child
                rc = fcgi_spawn_connection(hvml_app, init_script, script_query,
                        fcgi_fd, 1, pid_fd, max_executions);
                if (rc) {
                    syslog(LOG_ERR, "Failed fcgi_spawn_connection(): %d\n", rc);
                    break;
                }
            }
        }
        else if (WIFSIGNALED(status)) {
            syslog(LOG_ERR, "Child (%d) signaled : %d\n",
                    pid, WTERMSIG(status));
        }
        else {
            syslog(LOG_ERR, "Child (%d) died somehow: exit status = %d\n",
                    pid, status);
        }
    };

done:
    closelog();

    if (-1 != pid_fd) {
        close(pid_fd);
    }

    close(fcgi_fd);
    return rc;
}

