#include <iostream>
#include <string>
#include "def.h"
#include "router.h"

using namespace std;
int main(int argc, char **argv) {
    if (argc != 5){
        cout << "Not Enough Arguments" << endl;
        return -1;
    }
    unsigned int router_id;
    int nse_port, router_port;
    string nse_hostname;

    try{
        router_id = (unsigned) atoi(argv[1]);
        nse_hostname = argv[2];
        nse_port = atoi(argv[3]);
		router_port = atoi(argv[4]);
    } catch (...){
        cout << "Invalid arguments" << endl;
        return -1;
    }

    Router router {router_id, nse_hostname, nse_port, router_port};
    router.initialize();
    router.sendInit();
    router.receiveCircuitDB();
    router.initiateHelloProcess();
    while (true){
         router.receive();
      }
}
