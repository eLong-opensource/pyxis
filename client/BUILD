cc_library(
    name = "client",
    srcs = [
        "client_impl.cc",
        "cache.cc",
        "client.cc",
        "watch_manager.cc"
    ],
    deps = [
        "//pyxis/common:common",
        "//pyxis/base:base",
        "//pyxis/rpc:rpc_client",
        "//toft/base/string:string",
        "//toft/system/atomic:atomic",
        "//toft/base:random",
        "//thirdparty/muduo:muduo_base",
        "//thirdparty/leveldb:leveldb"
    ]
)

cc_test(
    name = "client_test",
    srcs = [
        "client_test.cc"
    ],
    deps = [
        "//toft/system/threading:threading",
        "//thirdparty/muduo:muduo_net",
        ":client"
    ]
)

cc_test(
    name = "client_bench",
    srcs = "client_bench.cc",
    deps = [
        "//toft/system/threading:threading",
        "//thirdparty/muduo:muduo_net",
        ":client"
    ]
)
