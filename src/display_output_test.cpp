#include "display.h"
#include <gflags/gflags.h>

DEFINE_bool(isvip, false, "If Is VIP");
DEFINE_string(ip, "127.0.0.1", "connect ip");
DECLARE_int32(port);
DEFINE_int32(port, 80, "listen port");

// DEFINE_bool(cam1, false, "cam1");
// DEFINE_bool(cam2, false, "cam2");
// DECLARE_int32(cam1_output, false, "LCD");
// DECLARE_int32(cam2_output, false, "HDMI");

using namespace google;
using namespace std;

static void print_usage()
{
    cxx_log("usage : ./app -ip=127.0.0.1 -port=90\n");
}

static void print_params()
{
    cout << "ip: \n"
         << FLAGS_ip << endl;
    cxx_log("port: %d\n", FLAGS_port);
    if (FLAGS_isvip)
    {
        cxx_log("isvip: true\n");
    }
    else
    {
        cxx_log("isvip: false\n");
    }
}

int main(int argc, char **argv)
{
    cxx_log("argc: %d\n", argc);
    if (argc < 4)
    {
        print_usage();
        return -1;
    }
    ParseCommandLineFlags(&argc, &argv, true);
    print_params();
    ShutDownCommandLineFlags();
    return 0;
}

int main_test(int argc, char **argv)
{

    int ret = 0;
    int i = 0;
    // 初始化屏幕
    ret = dis_init();
    if (ret != 0)
    {
        cxx_log("failed to exec initialize_screens.\n");
        return -11;
    }
    cxx_log("input something to show ipcs\n");
    getchar();
    cxx_log("change\n");
    read_rgba8888_pic_test();
    getchar();
    release_bo();
    return 0;
}
