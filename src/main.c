#include "chttp/server.h"

int main() {
  HttpConfig config = chttp_config_init(-1, "./public");

  return chttp_listen_and_serve(config);
}