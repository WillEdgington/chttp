#include "chttp/server.h"

int main() {
  chttp_config config = {.port = 8080, .public_dir = "./public"};

  return chttp_listen_and_serve(config);
}