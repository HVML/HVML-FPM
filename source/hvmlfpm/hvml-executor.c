/*
 * @file hvml-executor.c
 * @author Vincent Wei
 * @date 2022/10/16
 * @brief The HVML executor for requests from server.
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
 */

// #undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>

#include "config.h"
#include "hvml-executor.h"
#include "mpart-body-processor.h"
#define NO_FCGI_DEFINES
#include "libfcgi/fcgi_stdio.h"
#undef NO_FCGI_DEFINES

#define RUNNER_INFO_NAME    "runner-data"

struct runner_info {
    bool verbose;
    purc_coroutine_t main_crtn;
    purc_rwstream_t dump_stm;
};

#define MY_VRT_OPTS \
    (PCVRNT_SERIALIZE_OPT_SPACED | PCVRNT_SERIALIZE_OPT_NOSLASHESCAPE)

static int prog_cond_handler(purc_cond_k event, purc_coroutine_t cor,
        void *data)
{
    if (event == PURC_COND_COR_EXITED) {
        struct runner_info *runner_info = NULL;
        purc_get_local_data(RUNNER_INFO_NAME,
                (uintptr_t *)(void *)&runner_info, NULL);
        assert(runner_info);

        if (runner_info->verbose) {
            if (cor == runner_info->main_crtn) {
                LOG_INFO("The main coroutine exited.\n");
            }
            else {
                LOG_INFO("A child coroutine exited.\n");
                goto done;
            }

            struct purc_cor_exit_info *exit_info = data;
            if (purc_document_type(exit_info->doc) == PCDOC_K_TYPE_HTML) {
                fprintf(stdout, "Content-type: text/html\r\n\r\n");

                unsigned opt = PCDOC_SERIALIZE_OPT_FULL_DOCTYPE;
                opt |= PCDOC_SERIALIZE_OPT_UNDEF;
                opt |= PCDOC_SERIALIZE_OPT_FULL_DOCTYPE;
                opt |= PCDOC_SERIALIZE_OPT_IGNORE_C0CTRLS;

                purc_document_serialize_contents_to_stream(exit_info->doc,
                        opt, runner_info->dump_stm);
            }
            else if (!purc_variant_is_null(exit_info->result)) {
                fprintf(stdout, "Content-type: applicatin/json\r\n\r\n");
                purc_variant_serialize(exit_info->result,
                        runner_info->dump_stm, 0, MY_VRT_OPTS, NULL);
            }
            else {
                /* The HVML script takes the control of the response */
            }
        }
    }
    else if (event == PURC_COND_COR_TERMINATED) {
        struct runner_info *runner_info = NULL;
        purc_get_local_data(RUNNER_INFO_NAME,
                (uintptr_t *)(void *)&runner_info, NULL);
        assert(runner_info);

        struct purc_cor_term_info *term_info = data;
        if (cor == runner_info->main_crtn) {
            LOG_INFO("The main coroutine terminated due to "
                    "an uncaught exception: %s.\n",
                    purc_atom_to_string(term_info->except));
        }
        else {
            LOG_INFO("child coroutine terminated due to "
                    "an uncaught exception: %s.\n",
                    purc_atom_to_string(term_info->except));
        }

        if (purc_document_type(term_info->doc) == PCDOC_K_TYPE_HTML) {
            fprintf(stdout, "Content-type: text/html\r\n\r\n");

            unsigned opt = PCDOC_SERIALIZE_OPT_FULL_DOCTYPE;
            opt |= PCDOC_SERIALIZE_OPT_UNDEF;
            opt |= PCDOC_SERIALIZE_OPT_FULL_DOCTYPE;
            opt |= PCDOC_SERIALIZE_OPT_IGNORE_C0CTRLS;

            purc_document_serialize_contents_to_stream(term_info->doc,
                    opt, runner_info->dump_stm);
        }

        if (runner_info->verbose) {
            fprintf(stdout, ">> The executing stack frame(s):\n");
            purc_coroutine_dump_stack(cor, runner_info->dump_stm);
            fprintf(stdout, "\n");
        }
    }

done:
    return 0;
}

struct request_info {
    purc_variant_t server;
    purc_variant_t get;
    purc_variant_t post;
    purc_variant_t cookie;
    purc_variant_t files;
    purc_variant_t request;
    purc_vdom_t vdom;
};

enum value_type {
    VT_STRING,
    VT_ULONG,
};

enum post_content_type {
    CT_BAD = -1,
    CT_NOT_SUPPORTED = 0,
    CT_FORM_URLENCODED,
    CT_FORM_DATA,
    CT_JSON,
    CT_XML,
    CT_PLAIN,
};

#define strncasecmp2ltr(str, literal, len)      \
    ((len > (sizeof(literal "") - 1)) ? 1 :     \
        (len < (sizeof(literal "") - 1) ? -1 : strncasecmp(str, literal, len)))

static enum post_content_type
check_post_content_type(const char *content_type, const char **boundary)
{
    size_t whole_len = strlen(content_type);
    size_t mime_len = 0;

    enum post_content_type ct = CT_NOT_SUPPORTED;
    const char *mime = pcutils_get_next_token(content_type, ";", &mime_len);
    if (mime_len == 0)
        goto bad;

    *boundary = NULL;
    if (mime_len < whole_len) {
        const char *extkv = mime + mime_len;
        size_t key_len;
        extkv = pcutils_get_next_token(extkv, " =", &key_len);
        if (key_len == 0 || extkv[key_len] != '=')
            goto bad;

        if (strncasecmp2ltr(extkv, "charset", key_len) == 0) {
            const char *value = extkv + key_len + 1;
            size_t value_len = mime_len - key_len - 1;
            if (strncasecmp2ltr(value, "utf-8", value_len))
                goto bad;
        }
        else if (strncasecmp2ltr(extkv, "boundary", key_len) == 0) {
            const char *value = extkv + key_len + 1;
            *boundary = value;
        }
    }

    size_t type_len;
    const char *type = pcutils_get_next_token_len(mime, mime_len, "/", &type_len);
    if (type_len == 0 || type_len == mime_len)
        goto bad;

    const char *subtype = type + type_len + 1;
    size_t subtype_len = mime_len - type_len - 1;
    if (strncasecmp2ltr(type, "application", type_len) == 0) {
        if (strncasecmp2ltr(subtype, "x-www-form-urlencoded", subtype_len) == 0) {
            ct = CT_FORM_URLENCODED;
        }
        else if (strncasecmp2ltr(subtype, "json", subtype_len) == 0) {
            ct = CT_JSON;
        }
        else {
            ct = CT_NOT_SUPPORTED;
        }
    }
    else if (strncasecmp2ltr(type, "multipart", type_len) == 0 &&
            strncasecmp2ltr(subtype, "form-data", subtype_len)) {
        ct = CT_FORM_DATA;
    }
    else if (strncasecmp2ltr(type, "text", type_len) == 0) {
        if (strncasecmp2ltr(subtype, "xml", subtype_len) == 0) {
            ct = CT_XML;
        }
        else if (strncasecmp2ltr(subtype, "plain", subtype_len) == 0) {
            ct = CT_PLAIN;
        }
    }

    return ct;

bad:
    return CT_BAD;
}

static purc_variant_t parse_content_as_form_urlencoded(size_t content_length)
{
    purc_variant_t v = PURC_VARIANT_INVALID;

    char *buf = malloc(content_length + 1);
    if (buf) {
        size_t n = fread(buf, 1, content_length, stdin);
        if (n == content_length) {
            buf[content_length] = 0;

            v = purc_make_object_from_query_string(buf, true);
            if (v == PURC_VARIANT_INVALID) {
                LOG_ERROR("Failed when parsing content.\n");
            }
        }
        else {
            LOG_ERROR("Mismatched content length and content got.\n");
        }

        free(buf);
    }
    else {
        LOG_ERROR("Failed to allocate memory to hold content.\n");
    }

    return v;
}

static ssize_t cb_stdio_write(void *ctxt, const void *buf, size_t count)
{
    FILE *fp = ctxt;
    return fwrite(buf, 1, count, fp);
}

static ssize_t cb_stdio_read(void *ctxt, void *buf, size_t count)
{
    FILE *fp = ctxt;
    return fread(buf, 1, count, fp);
}

static purc_variant_t parse_content_as_json(size_t content_length)
{
    (void)content_length;
    purc_variant_t v = PURC_VARIANT_INVALID;
    purc_rwstream_t stm;
    stm = purc_rwstream_new_for_read(stdin, cb_stdio_read);

    if (stm) {
        v = purc_variant_load_from_json_stream(stm);
        purc_rwstream_destroy(stm);
    }
    else {
        LOG_ERROR("Failed when making stream from stdio\n");
    }

    return v;
}

/* TODO:
static purc_variant_t parse_content_as_xml(size_t content_length)
{
} */

static purc_variant_t parse_content_as_plain(size_t content_length)
{
    purc_variant_t v = PURC_VARIANT_INVALID;

    char *buf = malloc(content_length + 1);
    if (buf) {
        size_t n = fread(buf, 1, content_length, stdin);
        if (n == content_length) {
            buf[content_length] = 0;

            v = purc_variant_make_string_reuse_buff(buf, content_length + 1,
                    true);
            if (v == PURC_VARIANT_INVALID) {
                LOG_ERROR("Failed when make string from content.\n");
                goto failed;
            }
        }
        else {
            LOG_ERROR("Mismatched content length and content got.\n");
            goto failed;
        }
    }
    else {
        LOG_ERROR("Failed to allocate memory to hold content.\n");
    }

    return v;

failed:
    free(buf);
    return v;
}

static int release_request(struct request_info *info)
{
    if (info->server) {
        purc_variant_unref(info->server);
    }

    if (info->get) {
        purc_variant_unref(info->get);
    }

    if (info->post) {
        purc_variant_unref(info->post);
    }

    if (info->cookie) {
        purc_variant_unref(info->cookie);
    }

    if (info->files) {
        purc_variant_unref(info->files);
    }

    if (info->request) {
        purc_variant_unref(info->request);
    }

    memset(info, 0, sizeof(*info));
    return 0;
}

static int make_request(struct request_info *info)
{
    info->server = purc_variant_make_object_0();

    static struct var_type {
        const char *name;
        enum value_type type;
    } meta_vars[] = {

        // This variable identifies any mechanism used by the server to
        // authenticate the user.
        { "AUTH_TYPE", VT_STRING },

        // This variable contains the size of the message-body attached to
        // the request, if any, in decimal number of octets.
        { "CONTENT_LENGTH", VT_ULONG },

        // If the request includes a message-body, this variable is
        // set to the Internet Media Type [6] of the message-body.
        { "CONTENT_TYPE", VT_STRING },

        // This variable MUST be set to the dialect of CGI being used by
        // the server to communicate with the script.
        { "GATEWAY_INTERFACE", VT_STRING },

        // This variable specifies a path to be interpreted by the CGI script.
        { "PATH_INFO", VT_STRING },

        // This variable is derived by taking the PATH_INFO value,
        // parsing it as a local URI in its own right, and performing any
        // virtual-to-physical translation appropriate to map it onto the
        // server's document repository structure.
        { "PATH_TRANSLATED", VT_STRING },

        // This variable contains a URL-encoded search or parameter
        // string; it provides information to the CGI script to affect or refine
        // the document to be returned by the script.
        { "QUERY_STRING", VT_STRING },

        // This variable MUST be set to the network address of the
        // client sending the request to the server.
        { "REMOTE_ADDR", VT_STRING },

        // This variable contains the fully qualified domain name of
        // the client sending the request to the server, if available, otherwise
        // NULL.
        { "REMOTE_HOST", VT_STRING },

        // This variable MAY be used to provide identity information
        // reported about the connection by an RFC 1413 [20] request to the
        // remote agent, if available.
        { "REMOTE_IDENT", VT_STRING },

        // This variable provides a user identification string
        // supplied by client as part of user authentication.
        { "REMOTE_USER", VT_STRING },

        // This variable MUST be set to the method which
        // should be used by the script to process the request.
        { "REQUEST_METHOD", VT_STRING },

        // This variable MUST be set to a URI path (not URL-encoded)
        // which could identify the CGI script (rather than the script's
        // output).
        { "SCRIPT_NAME", VT_STRING },

        // This variable MUST be set to the name of the server host
        // to which the client request is directed.  It is a case-insensitive
        // hostname or network address.
        { "SERVER_NAME", VT_STRING },

        // This variable MUST be set to the TCP/IP port number on
        // which this request is received from the client.
        { "SERVER_PORT", VT_ULONG },

        // This variable MUST be set to the name and version of
        // the application protocol used for this CGI request.
        { "SERVER_PROTOCOL", VT_STRING },

        // This variable MUST be set to the name and version
        // of the information server software making the CGI request (and
        // running the gateway).
        { "SERVER_SOFTWARE", VT_STRING },

        // The visitor's cookie, if one is set.
        { "HTTP_COOKIE", VT_STRING },

        // The hostname of the page being attempted
        { "HTTP_HOST", VT_STRING },

        // The URL of the page that called your program
        { "HTTP_REFERER", VT_STRING },

        // The browser type of the visitor
        { "HTTP_USER_AGENT", VT_STRING },

        /* The following variables were not defined in RFC 3875. */

        // The root directory of the server
        { "DOCUMENT_ROOT", VT_STRING },

        // The port the client is connected to on the server
        { "REMOTE_PORT", VT_ULONG },

        // "on" if the program is being called through a secure server
        { "HTTPS", VT_STRING },

        // The interpreted pathname of the requested document or CGI
        // (relative to the document root)
        { "REQUEST_URI", VT_STRING },

        // The full pathname of the current CGI
        { "SCRIPT_FILENAME", VT_STRING },

        // The email address for your server's webmaster
        { "SERVER_ADMIN", VT_STRING },
    };

    for (size_t i = 0; i < PCA_TABLESIZE(meta_vars); i++) {
        purc_variant_t tmp = PURC_VARIANT_INVALID;

        const char *value = getenv(meta_vars[i].name);
        if (value) {
            if (meta_vars[i].type == VT_ULONG) {
                unsigned long ul = strtoul(value, NULL, 10);
                tmp = purc_variant_make_ulongint(ul);
            }
            else {
                tmp = purc_variant_make_string_static(value, true);
            }

            if (tmp == PURC_VARIANT_INVALID) {
                LOG_ERROR("Failed when making an variant for %s\n",
                        meta_vars[i].name);
                goto failed;
            }
        }
        else if (meta_vars[i].type == VT_ULONG) {
            tmp = purc_variant_make_ulongint(0);
            if (tmp == PURC_VARIANT_INVALID) {
                LOG_ERROR("Failed when making a ulong 0 variant for %s\n",
                        meta_vars[i].name);
                goto failed;
            }
        }

        if (tmp) {
            bool success = purc_variant_object_set_by_static_ckey(info->server,
                    meta_vars[i].name, tmp);
            purc_variant_unref(tmp);
            if (!success) {
                LOG_ERROR("Failed when making a property for %s\n",
                        meta_vars[i].name);
                goto failed;
            }
        }
    }

    const char *method =
        purc_variant_get_string_const(
                purc_variant_object_get_by_ckey(info->server, "REQUEST_METHOD"));

    if (strcasecmp(method, "GET") == 0) {
        const char *query =
            purc_variant_get_string_const(
                    purc_variant_object_get_by_ckey(info->server, "QUERY_STRING"));
        if (query) {
            info->get = purc_make_object_from_query_string(query, true);
            if (info->get == PURC_VARIANT_INVALID) {
                LOG_ERROR("Failed when parsing query string\n");
                goto failed;
            }
        }
    }
    else if (strcasecmp(method, "POST") == 0) {
        uint32_t tmp;
        purc_variant_cast_to_uint32(
                purc_variant_object_get_by_ckey(info->server,
                    "CONTENT_LENGTH"), &tmp, false);
        size_t content_length = (size_t)tmp;

        if (content_length > 0) {
            const char *content_type =
                purc_variant_get_string_const(purc_variant_object_get_by_ckey(
                            info->server, "CONTENT_TYPE"));

            if (content_type) {
                enum post_content_type ct;
                const char *boundary;
                ct = check_post_content_type(content_type, &boundary);

                if (ct <= 0) {
                    LOG_ERROR("not supported content type: %s\n", content_type);
                    goto failed;
                }

                switch (ct) {
                    case CT_FORM_URLENCODED:
                        info->post =
                            parse_content_as_form_urlencoded(content_length);
                        break;

                    case CT_FORM_DATA:
                        if (boundary) {
                            parse_content_as_multipart_form_data(content_length,
                                    boundary, &info->post, &info->files);
                        }
                        else {
                            LOG_ERROR("No boundary defined.\n");
                            goto failed;
                        }
                        break;

                    case CT_JSON:
                        info->post = parse_content_as_json(content_length);
                        break;

                    case CT_XML:
                        /* TODO */
                        info->post = parse_content_as_plain(content_length);
                        break;

                    case CT_PLAIN:
                        info->post = parse_content_as_plain(content_length);
                        break;

                    case CT_BAD:
                        LOG_ERROR("Bad content type.\n");
                        goto failed;
                        break;

                    case CT_NOT_SUPPORTED:
                        LOG_ERROR("Not supported content type.\n");
                        goto failed;
                        break;
                }
            }
        }
    }

    const char *cookie =
        purc_variant_get_string_const(
                purc_variant_object_get_by_ckey(info->server, "HTTP_COOKIE"));
    if (cookie) {
        info->cookie = purc_make_object_from_http_header_value(cookie);
    }

    if (info->get == PURC_VARIANT_INVALID)
        info->get = purc_variant_make_object_0();
    if (info->post == PURC_VARIANT_INVALID) {
        info->post = purc_variant_make_object_0();
    }
    if (info->cookie == PURC_VARIANT_INVALID)
        info->cookie = purc_variant_make_object_0();
    if (info->files == PURC_VARIANT_INVALID) {
        info->files = purc_variant_make_object_0();
    }

    info->request = purc_variant_make_object_0();
    /* merge properties of GET, POST, and COOKIE to request */
    ssize_t nr = purc_variant_object_unite(info->request, info->get,
            PCVRNT_CR_METHOD_OVERWRITE);
    if (nr < 0) {
        LOG_WARN("Failed to unite GET to REQ.\n");
    }

    nr = purc_variant_object_unite(info->request, info->post,
            PCVRNT_CR_METHOD_OVERWRITE);
    if (nr < 0) {
        LOG_WARN("Failed to unite POST to REQ.\n");
    }

    nr = purc_variant_object_unite(info->request, info->cookie,
            PCVRNT_CR_METHOD_OVERWRITE);
    if (nr < 0) {
        LOG_WARN("Failed to unite COOKIE to REQ.\n");
    }

    const char *script_name =
        purc_variant_get_string_const(
                purc_variant_object_get_by_ckey(info->server, "SCRIPT_NAME"));
    info->vdom = purc_load_hvml_from_file(script_name);
    if (info->vdom == NULL) {
        LOG_ERROR("Failed to load vDOM from %s.\n", script_name);
        goto failed;
    }

    return 0;

failed:
    release_request(info);
    return -1;
}

int hvml_executor(const char *app, bool verbose)
{
    unsigned int modules = 0;
    modules = (PURC_MODULE_HVML | PURC_MODULE_PCRDR) | PURC_HAVE_FETCHER_R;

    char runner[PURC_LEN_RUNNER_NAME + 1];
    int n = snprintf(runner, sizeof(runner), HVML_RUN_NAME, getpid());
    if (n < 0 || (size_t)n >= sizeof(runner)) {
        fprintf(stderr, "Failed to make runner name.\n");
        return EXIT_FAILURE;
    }

    purc_rwstream_t dump_stm;
    dump_stm = purc_rwstream_new_for_dump(stdout, cb_stdio_write);
    if (dump_stm == NULL) {
        fprintf(stderr, "Failed to make rwstream on stdout.\n");
        return EXIT_FAILURE;
    }

    purc_instance_extra_info extra_info = {};
    extra_info.renderer_comm = PURC_RDRCOMM_HEADLESS;
    extra_info.renderer_uri = DEF_RDR_URI_HEADLESS;

    int ret = purc_init_ex(modules, app, runner, &extra_info);
    if (ret != PURC_ERROR_OK) {
        fprintf(stderr, "Failed to initialize the PurC instance: %s\n",
            purc_get_error_message(ret));
        return EXIT_FAILURE;
    }

    if (verbose) {
        purc_enable_log_ex(PURC_LOG_MASK_DEFAULT | PURC_LOG_MASK_INFO,
                PURC_LOG_FACILITY_SYSLOG);
    }
    else {
        purc_enable_log_ex(PURC_LOG_MASK_DEFAULT, PURC_LOG_FACILITY_SYSLOG);
    }

    size_t nr_executed = 0;
    while (FCGI_Accept() >= 0) {
        struct request_info request_info = { };

        if ((ret = make_request(&request_info))) {
            LOG_ERROR("Failed to parse the request: %s\n",
                    purc_get_error_message(purc_get_last_error()));
            break;
        }

        purc_coroutine_t cor = purc_schedule_vdom(request_info.vdom, 0,
                request_info.request,
                PCRDR_PAGE_TYPE_NULL, NULL, NULL, NULL,
                NULL, NULL, NULL);

        /* bind _SERVER */
        if (!purc_coroutine_bind_variable(cor, HVML_VAR_SERVER,
                    request_info.server)) {
            LOG_ERROR("Failed to bind " HVML_VAR_SERVER ": %s\n",
                    purc_get_error_message(purc_get_last_error()));
            break;
        }

        /* bind _GET */
        if (!purc_coroutine_bind_variable(cor,
                    HVML_VAR_GET, request_info.get)) {
            LOG_ERROR("Failed to bind " HVML_VAR_GET ": %s\n",
                    purc_get_error_message(purc_get_last_error()));
            break;
        }

        /* bind _POST */
        if (!purc_coroutine_bind_variable(cor,
                    HVML_VAR_POST, request_info.post)) {
            LOG_ERROR("Failed to bind " HVML_VAR_POST ":%s\n",
                    purc_get_error_message(purc_get_last_error()));
            break;
        }

        /* bind _COOKIE */
        if (!purc_coroutine_bind_variable(cor,
                    HVML_VAR_COOKIE, request_info.cookie)) {
            LOG_ERROR("Failed to bind " HVML_VAR_COOKIE ":%s\n",
                    purc_get_error_message(purc_get_last_error()));
            break;
        }

        /* bind _FILES */
        if (!purc_coroutine_bind_variable(cor,
                    HVML_VAR_FILES, request_info.files)) {
            LOG_ERROR("Failed to bind " HVML_VAR_FILES ":%s\n",
                    purc_get_error_message(purc_get_last_error()));
            break;
        }

        struct runner_info runner_info = { verbose, cor, dump_stm };
        purc_set_local_data(RUNNER_INFO_NAME, (uintptr_t)&runner_info, NULL);

        purc_run((purc_cond_handler)prog_cond_handler);

        release_request(&request_info);

        nr_executed++;

    } /* while */

    purc_cleanup();
    purc_rwstream_destroy(dump_stm);
    return 0;
}

