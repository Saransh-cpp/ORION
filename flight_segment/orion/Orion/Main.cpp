#include <getopt.h>
#include <signal.h>

#include <Orion/Top/OrionTopology.hpp>
#include <Os/Os.hpp>
#include <cstdlib>

void print_usage(const char* app) {
    (void)printf("Usage: ./%s [options]\n-a\thostname/IP address\n-p\tport_number\n", app);
}

static void signalHandler(int signum) { Orion::stopRateGroups(); }

int main(int argc, char* argv[]) {
    Os::init();
    U16 port_number = 0;
    I32 option = 0;
    char* hostname = nullptr;

    while ((option = getopt(argc, argv, "hp:a:")) != -1) {
        switch (option) {
            case 'a':
                hostname = optarg;
                break;
            case 'p':
                port_number = static_cast<U16>(atoi(optarg));
                break;
            case 'h':
            case '?':
            default:
                print_usage(argv[0]);
                return (option == 'h') ? 0 : 1;
        }
    }

    Orion::TopologyState inputs;
    inputs.hostname = hostname;
    inputs.port = port_number;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    (void)printf("Hit Ctrl-C to quit\n");

    Orion::setupTopology(inputs);
    Orion::startRateGroups(Fw::TimeInterval(1, 0));
    Orion::teardownTopology(inputs);
    (void)printf("Exiting...\n");
    return 0;
}
