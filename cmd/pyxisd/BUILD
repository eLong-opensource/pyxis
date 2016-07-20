cc_binary(
    name = "pyxisd",
    srcs = [
        "main.cc"
    ],
    deps = [
        "//pyxis/server:server",
        "//thirdparty/sofa:sofa_rpc",
        "//thirdparty/gflags:gflags",
        "//thirdparty/glog:glog",
        "//thirdparty/muduo:muduo_inspect",
        "//raft:sofa_transporter",
        "//toft/base:binary_version"
    ]
)
