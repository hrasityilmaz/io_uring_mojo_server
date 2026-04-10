from std.ffi import c_int, external_call
from std.sys import argv

def main() raises:
    var args = argv()
    var port: Int = 8080

    if args.__len__() >= 2:
        port = Int(String(args[1]))

    print("mojo -> c io_uring echo server baslatiliyor, port:", port)

    var rc = external_call["start_echo_server", c_int](port)
    print("server cikti kodu:", rc)