#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hvml-executor.h"
#include "multipart-parser.h"
#include "mpart-body-processor.h"
#include "util/kvlist.h"

typedef struct mpart_body_processor {
    purc_variant_t post;
    purc_variant_t files;

    multipart_parser *parser;

    /* headers of current part */
    struct kvlist *headers;

    char *last_header_name;
    size_t last_header_name_len;
    char *last_header_value;
    size_t last_header_value_len;

    /* fields for current data */
    char *name;
    char *data;
    size_t data_len;

    /* data is a file if fp is not NULL */
    FILE *fp;
} mpart_body_processor;

static int header_field_cb(multipart_parser *p, const char *buf, size_t len)
{
    mpart_body_processor *processor = multipart_parser_get_data(p);
    if (processor->last_header_name && processor->last_header_value) {

        kvlist_set_ex(processor->headers, processor->last_header_name,
            processor->last_header_value);

        free(processor->last_header_name);
        // free(processor->last_header_value);
        processor->last_header_name = NULL;
        processor->last_header_value = NULL;
        processor->last_header_name_len = 0;
        processor->last_header_value_len = 0;
    }
    else if (processor->last_header_name == NULL) {
        processor->last_header_name = strndup(buf, len);
        processor->last_header_name_len = len;
    }
    else {
        char *new_name = malloc(processor->last_header_name_len + len + 1);
        strcpy(new_name, processor->last_header_name);
        strncat(new_name, buf, len);

        char *tmp = processor->last_header_name;
        processor->last_header_name = new_name;
        free(tmp);
        processor->last_header_name_len += len;
    }

    return 0;
}

static int header_value_cb(multipart_parser *p, const char *buf, size_t len)
{
    mpart_body_processor *processor = multipart_parser_get_data(p);
    if (processor->last_header_name == NULL) {
        return -1;
    }
    else if (processor->last_header_value == NULL) {
        processor->last_header_value = strndup(buf, len);
        processor->last_header_value_len = len;
    }
    else {
        char *new_name = malloc(processor->last_header_value_len + len + 1);
        strcpy(new_name, processor->last_header_value);
        strncat(new_name, buf, len);

        char *tmp = processor->last_header_value;
        processor->last_header_value = new_name;
        free(tmp);
        processor->last_header_value_len += len;
    }

    return 0;
}

static int str_starts_with(const char *str, const char *substr)
{
    if ((str == NULL) || (substr == NULL)) return 0;
    return strncmp(str, substr, strlen(substr)) == 0;
}

#if 0
static int str_ends_with(const char *str, const char *substr)
{
    if (!str || !substr) return 0;
    int string_len = strlen(str);
    int substr_len = strlen(substr);

    if (substr_len > string_len) return 0;

    return strncmp(str + string_len - substr_len, substr, substr_len) == 0;
}
#endif

static char *str_trim(char *str)
{
    char *end;

    while (isspace(*str))
        str++;

    if (*str == 0)
        return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace(*end))
        end--;

    *(end + 1) = 0;

    return str;
}

static bool is_quote(char c)
{
    return (c == '"' || c == '\'');
}

static char *str_strip_quotes(char *str)
{
    char *end;

    while (is_quote(*str))
        str++;

    if (*str == 0)
        return str;

    end = str + strlen(str) - 1;
    while (end > str && is_quote(*end))
        end--;

    *(end + 1) = 0;
    return str;
}

#if 0
static void skip_spaces(char **ptr)
{
    while (isspace(**ptr)) {
        *ptr = *ptr + 1;
    }
}
#endif

static void attrs_map_parse(struct kvlist *map, const char *str)
{
    char *pair, *name, *value, *dup_value, *saved_ptr;
    dup_value = strdup(str);
    saved_ptr = dup_value;

    while (isspace(*dup_value))
        dup_value++;

    while ((pair = strsep(&dup_value, ";")) && pair != NULL) {
        name = strsep(&pair, "=");
        value = strsep(&pair, "=");

        kvlist_set(map, str_trim(name),
                strdup(str_trim(str_strip_quotes(value))));
    }

    free(saved_ptr);
}

static void attrs_map_delete(struct kvlist *map)
{
    const char *name;
    void *next, *data;

    kvlist_for_each_safe(map, name, next, data) {
        char *value = *(char **)data;
        free(value);
    }

    kvlist_free(map);
}


purc_variant_t
purc_make_object_from_http_header_value(const char *line)
{
    purc_variant_t obj = purc_variant_make_object_0();

    char *pair, *name, *value, *dup_value, *saved_ptr;
    dup_value = strdup(line);
    saved_ptr = dup_value;

    while (isspace(*dup_value))
        dup_value++;

    while ((pair = strsep(&dup_value, ";")) && pair != NULL) {
        name = strsep(&pair, "=");
        value = strsep(&pair, "=");

        name = str_trim(name);
        value = str_trim(value);
        purc_variant_t k = purc_variant_make_string(name, true);
        purc_variant_t v = purc_variant_make_string(value, true);
        if (k && v) {
            purc_variant_object_set(obj, k, v);
        }

        if (k)
            purc_variant_unref(k);
        if (v)
            purc_variant_unref(v);
    }

    free(saved_ptr);
    return obj;
}

static void str_sanitize(char *str)
{
    for (size_t i = 0; i < strlen(str); i++) {
        if (isspace(str[i])) str[i] = '_';
    }
}

static void headers_map_new(mpart_body_processor *processor)
{
    assert(processor->headers == NULL);
    processor->headers = malloc(sizeof(struct kvlist));
    kvlist_init(processor->headers, NULL);
}

static void headers_map_delete(mpart_body_processor *processor)
{
    assert(processor->headers);

    const char *name;
    void *next, *data;

    kvlist_for_each_safe(processor->headers, name, next, data) {
        char *value = *(char **)data;
        free(value);
    }

    kvlist_free(processor->headers);
    free(processor->headers);
    processor->headers = NULL;
}

static int headers_complete_cb(multipart_parser *p)
{
    mpart_body_processor *processor = multipart_parser_get_data(p);

    char *content_disposition = kvlist_get(processor->headers,
            "Content-Disposition");

    if (str_starts_with(content_disposition, "form-data;")) {
        struct kvlist attrs_map;
        kvlist_init(&attrs_map, NULL);
        attrs_map_parse(&attrs_map,
                content_disposition + sizeof("form-data;") - 1);

        char *name = kvlist_get(&attrs_map, "name");
        char *filename = kvlist_get(&attrs_map, "filename");

        if (filename != NULL) {
            str_sanitize(filename);

            const char *upload_folder_path = HTTP_UPLOAD_PATH;
            mkdir(upload_folder_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

            char template[] = HTTP_UPLOAD_FILE_TEMPLATE;
            int fd = mkstemp(template);
            if (fd >= 0) {
                processor->fp = fdopen(fd, "a");
            }

            if (fd < 0 || processor->fp == NULL) {
                goto failed;
            }

            char *content_type = kvlist_get(processor->headers,
                    "Content-Type");

            purc_variant_t tmp;

            purc_variant_t file = purc_variant_make_object_0();
            tmp = purc_variant_make_string(filename, true);
            purc_variant_object_set_by_ckey(file, "name", tmp);
            purc_variant_unref(tmp);

            tmp = purc_variant_make_string(content_type, true);
            purc_variant_object_set_by_ckey(file, "type", tmp);
            purc_variant_unref(tmp);

            tmp = purc_variant_make_string(template, true);
            purc_variant_object_set_by_ckey(file, "tmp_name", tmp);
            purc_variant_unref(tmp);

            purc_variant_object_set_by_ckey(processor->files, name, file);
            purc_variant_unref(file);

#if 0
            char file_path[MAX_PATH + 1];
            int n = snprintf(file_path, sizeof(file_path), "%s/%s",
                    upload_folder_path, filename);
            if (n < 0 || (size_t)n >= sizeof(file_path))
                goto failed;
#endif
        }
        else {
            purc_variant_t tmp = purc_variant_make_null();
            purc_variant_object_set_by_ckey(processor->post, name, tmp);
            purc_variant_unref(tmp);

            processor->fp = NULL;
        }

        attrs_map_delete(&attrs_map);
        processor->name = strdup(name);
    }
    else {
        goto failed;
    }

    headers_map_delete(processor);
    return 0;

failed:
    return -1;
}

static int part_data_cb(multipart_parser *p, const char *buf, size_t len)
{
    mpart_body_processor *processor = multipart_parser_get_data(p);

    assert(processor->name);

    if (processor->fp) {
        size_t n = fwrite(buf, 1, len, processor->fp);
        if (n < len)
            return -1;
        processor->data_len += len;
    }
    else if (processor->data == NULL) {
        processor->data = malloc(len);
        processor->data_len = len;
        memcpy(processor->data, buf, len);
    }
    else {
        char *new_data = malloc(processor->data_len + len);
        strcpy(new_data, processor->data);
        strncat(new_data, buf, len);

        char *tmp = processor->data;
        processor->data = new_data;
        free(tmp);
        processor->data_len += len;
    }

    return 0;
}

static int part_data_begin_cb(multipart_parser *p)
{
    mpart_body_processor *processor = multipart_parser_get_data(p);

    headers_map_new(processor);
    return 0;
}

static int part_data_end_cb(multipart_parser *p)
{
    mpart_body_processor *processor = multipart_parser_get_data(p);

    assert(processor->name);

    if (processor->fp) {
        purc_variant_t file;
        file = purc_variant_object_get_by_ckey(processor->files,
                processor->name);
        assert(file);

        purc_variant_t tmp = purc_variant_make_ulongint(processor->data_len);
        purc_variant_object_set_by_ckey(file, "size", tmp);
        purc_variant_unref(tmp);

        fclose(processor->fp);
    }
    else if (processor->data) {
        purc_variant_t tmp = purc_variant_make_string(processor->data, true);
        purc_variant_object_set_by_ckey(processor->post, processor->name, tmp);
        purc_variant_unref(tmp);

        free(processor->data);
        processor->data = NULL;
    }

    free(processor->name);
    processor->name = NULL;
    return 0;
}

static int body_end_cb(multipart_parser *p)
{
    (void)p;
    return 0;
}

static multipart_parser_settings settings = {
    .on_header_field = header_field_cb,
    .on_header_value = header_value_cb,
    .on_part_data = part_data_cb,
    .on_part_data_begin = part_data_begin_cb,
    .on_headers_complete = headers_complete_cb,
    .on_part_data_end = part_data_end_cb,
    .on_body_end = body_end_cb
};

int
parse_content_as_multipart_form_data(size_t content_length,
        const char *boundary, purc_variant_t *post, purc_variant_t *files)
{
    mpart_body_processor processor = { };

    processor.parser = multipart_parser_init(boundary, &settings);
    multipart_parser_set_data(processor.parser, &processor);

    size_t nr_bytes = 0;
    do {
        char buf[1024];
        size_t n = fread(buf, 1, sizeof(buf), stdin);

        if (multipart_parser_execute(processor.parser, buf, n) < n)
            break;

        nr_bytes += n;
        if (n < sizeof(buf))
            break;

    } while (true);

    if (nr_bytes < content_length) {
        purc_log_error("%s: Mismatched content length and content got.\n",
                __func__);
    }

    multipart_parser_free(processor.parser);
    *post = processor.post;
    *files = processor.files;
    return 0;
}

