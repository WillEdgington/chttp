#ifndef CHTTP_HANDLER_H
#define CHTTP_HANDLER_H

#include "chttp/http.h"

HttpResponse *chttp_handle_request(HttpRequest *req, const char *base_dir);

#endif