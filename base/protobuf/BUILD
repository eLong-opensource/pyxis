proto_library(
    name = "proto",
    srcs = [
        "packet.proto"
    ]
)

cc_library(
    name = "packet",
    srcs = [
        "packet.cc"
    ],
    deps = [
        ":proto"
    ]
)

cc_test(
    name = "packet_test",
    srcs = [
        "packet_test.cc"
    ],
    deps = ":packet"
)
