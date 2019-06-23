
#include <getopt.h>
#include <stdarg.h>
#include <strings.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <string>
#include <sstream>

#include "AdbController.h"
#include "Config.h"
#include "FilePoller.h"
#include "Logger.h"

enum RunMode {
    None, Help, Connect, Disconnect
};

static const std::array<const char * const, 2> AuthTypes({"WEP", "WSA"});
static const char *AdbCmdDefault = "adb";

void usage(char *pname)
{
    std::stringstream ss;
    bool first = true;
    for( auto atype : AuthTypes ) {
        if (first) first = false; else ss << '|';
        ss << atype;
    }

    fprintf(stderr, "Usage:\n%s -s|--ssid <SSID> -k|--key <security key>\n"
                    "\t- connect to WiFi AP\n"
                    "%s -d|--disconnect\n\t- disconnect from AP\n"
                    "Optional switches:\n"
                    " -a|--adbcmd=<adb command>, default is \"adb\"\n"
                    " -h|--help - print usage\n"
                    " -t|--type <%s>, default is WSA\n"
                    " -v|--verbose - noisy logging", pname, pname, ss.str().c_str());
}

__attribute__((__format__ (__printf__, 2, 3)))
void print_err(char *pname, const char *errmsg, ... )
{
    va_list ap;
    va_start( ap, errmsg );
    fprintf(stderr, "Error: ");
    vfprintf(stderr, errmsg, ap);
    fprintf(stderr, "\n");
    va_end( ap );
    usage( pname );
}

bool parseClArgs(int argc, char** argv, Config &cfg, RunMode &rmode)
{
    struct option longopts[] = {
        {"adbcmd", required_argument, nullptr, 'a'},
        {"disconnect", required_argument, nullptr, 'd'},
        {"help", no_argument, nullptr, 'h'},
        {"key", required_argument, nullptr, 'k'},
        {"ssid", required_argument, nullptr, 's'},
        {"type", required_argument, nullptr, 't'},
        {"verbose", required_argument, nullptr, 'v'},
        {nullptr, 0, nullptr, 0},
    };

    bool hflag = false, dflag = false, conn_flag = false;
    rmode = RunMode::None;

    Config::Builder builder;
    builder.setAdbCmd( AdbCmdDefault );
    builder.setAuthType( AuthTypes[0] );

    while (1) {
        int option_index = 0;
        int opt = getopt_long(argc, argv, "a:ds:k:t:v", longopts, &option_index);

        if (opt == -1)
            break;

        switch (opt) {
            case 'a':
                builder.setAdbCmd( optarg );
                break;

            case 'd':
                dflag = true;
                break;

            case 'h':
                hflag = true;
                break;

            case 'k':
                builder.setPassword( optarg );
                conn_flag = true;
                break;

            case 's':
                builder.setSsid( optarg );
                conn_flag = true;
                break;

            case 't':
            {
                bool found = false;
                for (auto atype : AuthTypes) {
                    found = strcasecmp(optarg, atype) == 0;
                    if (found) {
                        builder.setAuthType( atype );
                        break;
                    }
                }
                if (!found) {
                    print_err(*argv, "Unknown auth type %s", optarg);
                    return false;
                }
            }
                break;

            case 'v':
                Logger::instance().verbose(true);
                break;

            default:
                print_err(*argv, "Unknown opttion %s", argv[optind]);
                break;
        }
    }

    if (optind < argc) {
        print_err(*argv, "Extra command line parameters: %s...", argv[optind]);
        return false;
    }

    if (dflag && conn_flag) {
        print_err(*argv, "Connect & disconnect modes are mutually exclusive");
        return false;
    }

    if (conn_flag || dflag) {
        cfg = builder.build();
        if (conn_flag) {
            if (cfg.getSsid().empty()) {
                print_err(*argv, "Empty SSID");
                return false;
            }
            rmode = RunMode::Connect;
        } else if (dflag) {
            rmode = RunMode::Disconnect;
        }
    }

    if (hflag) {
        usage(*argv);
        if (rmode == RunMode::None) rmode = RunMode::Help;
    } else {
        if (rmode == RunMode::None) {
            print_err( *argv, "Should specify run mode" );
            return false;
        }
    }
    return true;
}

namespace {

class StaticConfigDeleter {
public:
    void operator()(Config *) {}
};

}

int main(int argc, char** argv)
{
    Config cfg;
    RunMode rmode;
    if (!parseClArgs( argc, argv, cfg, rmode ))
        return 1;

#ifndef NDEBUG
    Logger::instance().verbose(true);
#endif

    if (rmode == RunMode::Help)
        return 0;

    bool run = false;
    FilePoller fpoll;
    AdbController adb(std::shared_ptr<Config>(&cfg, StaticConfigDeleter()), fpoll);
    
    switch (rmode) {
        case RunMode::Connect:
            run = adb.connectWiFi();
            break;
        case RunMode::Disconnect:
            run = adb.disconnectWiFi();
            break;
        default:
            assert( false );
    }
    
    if (run)
        fpoll.exec();
    
    return run ? adb.exitCode() : 255;
}
