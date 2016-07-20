cc_library(
    name = "server",
    srcs = [
        "rpc_service.cc",
        "server.cc",
        "session_manager.cc",
        "watch_manager.cc",
        "session.cc",
        "store.cc",
        "proc_db.cc",
        "watcher.cc"
    ],
    deps = [
        "//pyxis/common:common",
        "//pyxis/base:base",
        "//pyxis/rpc:rpc_server",
        "//raft:raft",
        "//thirdparty/muduo:muduo_base",
        "//thirdparty/leveldb:leveldb",
        "//toft/storage/path:path",
        "//toft/encoding:proto_json_format",
        "//toft/base/string:string",
        "//thirdparty/sofa:sofa_rpc",
    ]
)

cc_test(
    name = "store_test",
    srcs = [
        "store_test.cc",
        "proc_db_test.cc",
    ],
    deps = [
        "//thirdparty/gmock:gmock",
        '//thirdparty/perftools:tcmalloc_and_profiler',
        ":server"
    ]
)


cc_test(
    name = "server_test",
    srcs = [
        "server_test.cc"
    ],
    deps = [
        "//thirdparty/gmock:gmock",
        '//thirdparty/perftools:tcmalloc_and_profiler',
        ":server"
    ]
)
