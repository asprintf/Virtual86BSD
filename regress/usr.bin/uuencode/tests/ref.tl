/*
 * ref.tl - the lines of the encoding of FILE, as the historical uuencode
 * (with ` for spaces) and base64 (RFC 4648) write them, for the tests of uuencode
 *
 *     tl ref.tl uu|b64 FILE
 */
include "../../../lib/testlib/uu.tl";

var d = readfile(argv[1]);
puts(argv[0] == "uu" ? uu_body(d, true) : b64_body(d));
