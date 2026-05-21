#ifndef CHTTP_ERR_H
#define CHTTP_ERR_H

#include "chttp/http.h"

void chttp_create_error_response(HttpResponse *res, int code, const char *msg,
                                 const char *base_dir);

#endif