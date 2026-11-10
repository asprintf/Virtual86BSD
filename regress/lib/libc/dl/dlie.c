/* libdlie.so: initial-exec TLS, which a dlopen'ed object cannot have. */
__thread int dlie_x __attribute__((tls_model("initial-exec")));
int dlie_get(void) { return dlie_x++; }
