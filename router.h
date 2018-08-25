#include <iostream>
#include <string>
#include "def.h"
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <vector>
#include <unordered_map>

using namespace std;

class GenericException {};

class Router{
public:
    unsigned int router_id;
    FILE *routerLogFile;
    struct circuit_DB router_db;

    vector<struct circuit_DB> topology;
    unordered_map<unsigned int, pair<unsigned int, unsigned int>> rib;
    vector<vector<unsigned int>>graph;
    vector<vector<bool>>edgeExist;

    /*
        UDP Stuff
    */
    string nse_hostname;
    unsigned int nse_ip = 0;
    int routerFileDescriptor = 0;
    int nse_port;
    struct sockaddr_in nse_info;
    int router_port;


    Router(unsigned int router_id, string nse_hostname, int nse_port, int router_port);
    ~Router();
    void createLogFile();
    void initializeUDP();
    void initialize();
    void sendInit();
    void receiveCircuitDB();
    void sendHello(unsigned int link);
    void initiateHelloProcess();
    void notifyNeighbours(unsigned int desiredLinkID);
    void receiveLSPDU(struct pkt_LSPDU lspdu);
    void receive();
    void logTopology();
    void logRIB(vector<int>& parent, vector<unsigned int> &dist);
    void initGraph();
    bool update(struct pkt_LSPDU lspdu);
    void shortestPath();

};
