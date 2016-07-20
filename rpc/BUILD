cc_library(
    name = "rpc_server",
    srcs = [
        "session.cc",
        "codec.cc",
        "server.cc",
        "event.cc"
    ],
    deps = [
        "//thirdparty/muduo:muduo_protobuf_codec",
        "//thirdparty/muduo:muduo_net",
        "//pyxis/proto:rpc_proto",
        "//pyxis/common:common",
        "//toft/system/threading:threading"
    ]
)

cc_library(
    name = "rpc_client",
    srcs = [
        "codec.cc",
        "client.cc",
    ],
    deps = [
        "//thirdparty/muduo:muduo_protobuf_codec",
        "//thirdparty/muduo:muduo_net",
        "//pyxis/proto:rpc_proto",
        "//pyxis/common:common",
        "//toft/system/threading:threading"
    ]
)

#cc_test(
 #   name = "rpc_test",
 #   srcs = "rpc_test.cc",
 #   deps = ":rpc"
#)
