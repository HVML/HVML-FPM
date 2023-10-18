#ifndef _mpart_body_processor_h
#define _mpart_body_processor_h

#include <purc/purc-variant.h>

#ifdef __cplusplus
extern "C" {
#endif

purc_variant_t
purc_make_object_from_http_header_value(const char *value);

int
parse_content_as_multipart_form_data(size_t content_length,
        const char *boundary, purc_variant_t *post, purc_variant_t *files);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
