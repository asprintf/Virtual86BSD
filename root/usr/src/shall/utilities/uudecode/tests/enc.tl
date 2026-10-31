/*
 * enc.tl - encoded input for the tests of uudecode, made independently
 * of uuencode (as Python's binascii and base64 write it)
 *
 *     tl enc.tl uu|b64 MODE NAME FILE [bt]
 *
 * bt: the historical format with ` for spaces.
 */
include "../../testlib/uu.tl";

var kind = argv[0], mode = argv[1], name = argv[2];
var d = readfile(argv[3]);
var bt = len(argv) > 4;
if (kind == "uu")
    printf("begin %s %s\n%s%s\nend\n", mode, name, uu_body(d, bt), bt ? "`" : " ");
else
    printf("begin-base64 %s %s\n%s====\n", mode, name, b64_body(d));
