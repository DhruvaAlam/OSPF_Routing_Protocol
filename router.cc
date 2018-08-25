#include <iostream>
#include <string>
#include "def.h"
#include "router.h"
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <cstring>
#include <unistd.h>
#include <limits.h>
#include <unordered_map>


using namespace std;


Router::Router(unsigned int router_id, string nse_hostname,
    int nse_port, int router_port): router_id{router_id},
    nse_hostname{nse_hostname},nse_port{nse_port}, router_port{router_port}{}

Router::~Router(){
    fclose(this->routerLogFile);
}

void Router::initializeUDP(){
    //convert hostname to ip
    {
        struct hostent *he;
        he = gethostbyname( this->nse_hostname.c_str());
        if (not he){
            cout << "B" << endl;
            throw GenericException {};
        }
        memcpy(&this->nse_ip, he->h_addr_list[0], he->h_length);
    }
    //make socket then bind
    {
        if ((this->routerFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            cout << "Socket creation error" << endl;
            throw GenericException {};
        }
        struct sockaddr_in server_information = {0};
        server_information.sin_family = AF_INET;
        server_information.sin_addr.s_addr = htonl(INADDR_ANY);
        server_information.sin_port = htons(this->router_port);

        //bind
        if (bind(routerFileDescriptor, (struct sockaddr*) &server_information, sizeof(server_information)) < 0) {
    		close(routerFileDescriptor);
            cout << "Failed to bind" << endl;
    		throw GenericException {};
	   }
    }

    //set up server address structure
    {
        memset(&this->nse_info, 0, sizeof(struct sockaddr_in));
        this->nse_info.sin_family = AF_INET;
	    this->nse_info.sin_port = htons(this->nse_port);
	    memcpy(&this->nse_info.sin_addr, &this->nse_ip, sizeof(int));
    }
}
void Router::createLogFile(){
    //open file with write priveleges
    char buffer[256]= {};
    snprintf(buffer, sizeof(buffer), "router%u.log", this->router_id);
    this->routerLogFile = fopen(buffer, "w");
    //make unbuffered, so it instantly writes to the file
    setbuf(this->routerLogFile, NULL);
}

void Router::initialize(){
    this->createLogFile();
    this->initializeUDP();
    this->topology = vector<struct circuit_DB>();
    for (int i = 0; i <= NBR_ROUTER; ++i){
        struct circuit_DB temp;
        temp.nbr_link = 0;
        this->topology.push_back(temp);
    }
    //send to router NBR_ROUTER + 1 is invalid, UINT_MAX cost implies no path
    for (unsigned int i = 1; i <= NBR_ROUTER; ++i){
        rib[i] = make_pair(NBR_ROUTER + 1, UINT_MAX);
    }
    //to send to yourself, the cost shouuld be 0
    rib[router_id].first = router_id;
    rib[router_id].second = 0;

}

void Router::sendInit(){
    struct pkt_INIT init {this->router_id};
	fprintf(this->routerLogFile, "Router %d: INIT message sent\n", this->router_id);
    int temp = sendto(this->routerFileDescriptor, &init, sizeof(init), 0,
        (struct sockaddr*) &this->nse_info, sizeof(struct sockaddr_in));
    if (temp < 0){
        cout << "C" << endl;
        throw GenericException{};
    }
}
void Router::sendHello(unsigned int link) {
    struct pkt_HELLO hello {this->router_id, link};
    //cout << "Router " << router_id << " HELLO message sent to" << link << endl;
    fprintf(this->routerLogFile, "Router %d: HELLO message sent to link %d\n", this->router_id, link);
	sendto(this->routerFileDescriptor, &hello, sizeof(hello), 0, (struct sockaddr*) &this->nse_info, sizeof(struct sockaddr_in));
}

void Router::initiateHelloProcess(){
    for (unsigned int i = 0; i < this->router_db.nbr_link; ++i) {
        //cout << "Linked to " << this->router_db.linkcost[i].link << endl;
		this->sendHello(this->router_db.linkcost[i].link);
	}
    fprintf(this->routerLogFile, "\n\n");
    //usleep(1000000);
}

void Router::logTopology(){
    fprintf(this->routerLogFile, "# Topology Database\n");
    for (unsigned int i = 1; i <= NBR_ROUTER; ++i){
        if (this->topology[i].nbr_link <=0){
            //not initialized yet
            continue;
        }
        //record the number of links for this router
        fprintf(this->routerLogFile, "R%d -> R%d nbr link %d\n",
            this->router_id, i, this->topology[i].nbr_link);
        for (unsigned int n = 0; n < this->topology[i].nbr_link; ++n){
            fprintf(this->routerLogFile, "R%d -> R%d link %d cost %d\n",
                router_id, i, this->topology[i].linkcost[n].link,
                 this->topology[i].linkcost[n].cost);
        }
    }
    //fprintf(this->routerLogFile, "\n\n");
}

void Router::receiveCircuitDB(){
    int sizePacket =  recvfrom(this->routerFileDescriptor,
        &this->router_db, sizeof(this->router_db), 0, NULL, NULL);
    if (sizePacket != sizeof(this->router_db) || sizePacket == -1){
        cout << "A" << endl;
        throw GenericException{};
    }
    fprintf(this->routerLogFile, "Router %d: Received CircuitDB from NSE\n", this->router_id);
    this->topology[this->router_id] = this->router_db;
    this->logTopology();
}

void Router::notifyNeighbours(unsigned int desiredLinkID){
    //send LSPDU to links directly attached to router
    fprintf(routerLogFile, "\n\n");
    for (unsigned int i = 0 ; i < topology[router_id].nbr_link; ++i){
        if (topology[router_id].linkcost[i].link != desiredLinkID){
            continue;
        }

        for (unsigned int n = 1 ; n <= NBR_ROUTER; ++n ){
            for (unsigned int m = 0; m < topology[n].nbr_link; ++m){
                struct pkt_LSPDU temp;
                temp.sender = router_id;
                temp.via = topology[router_id].linkcost[i].link;
                temp.router_id = n;

                temp.link_id = topology[n].linkcost[m].link;
				temp.cost = topology[n].linkcost[m].cost;
				fprintf(routerLogFile,
                    "Router%d: Sent LSPDU: sender R%d, router_id %d, link_id %d, cost %d, via %d\n",
                    router_id, temp.sender, temp.router_id, temp.link_id, temp.cost, temp.via);
				sendto(routerFileDescriptor,
                    &temp, sizeof(temp), 0, (struct sockaddr*) &nse_info,
                    sizeof(struct sockaddr_in));

            }
        }


    }
}
//Update the topology
bool Router::update(struct pkt_LSPDU lspdu){
    //check if we already have this link
    for (unsigned int i = 0; i < topology[lspdu.router_id].nbr_link; ++i){
        //we already have info of this link
        if (lspdu.link_id == topology[lspdu.router_id].linkcost[i].link){
            return false;
        }
        //else loop through rest for the router
    }
    int pos = topology[lspdu.router_id].nbr_link;
    ++topology[lspdu.router_id].nbr_link;;
    topology[lspdu.router_id].linkcost[pos].link = lspdu.link_id;
    topology[lspdu.router_id].linkcost[pos].cost = lspdu.cost;
    //log these changes
    logTopology();

    return true;
}
void Router::logRIB(vector<int>& parent, vector<unsigned int> &dist){
    fprintf(routerLogFile, "# RIB\n");
    for (unsigned int i = 1; i <= NBR_ROUTER; ++i){
        if (i != router_id){
            if (dist[i] == UINT_MAX){
                continue;
            }
            unsigned int prevprev= i;
            unsigned int prev= i;
            while (true) {
				prev = parent[prev];
				if (prev == router_id || prev == 0) {
					break;
				}
				prevprev = prev;
			}
            fprintf(routerLogFile, "R%d -> R%d -> R%d, %d\n", router_id, i,
                prevprev, dist[i]);
        } else {
            fprintf(routerLogFile, "R%d -> R%d -> Local, 0\n", i, i);
        }
    }
}

void Router::initGraph(){
    graph = vector<vector<unsigned int>>(NBR_ROUTER + 1, vector<unsigned int>(NBR_ROUTER + 1, 0));
    edgeExist =  vector<vector<bool>>(NBR_ROUTER + 1, vector<bool>(NBR_ROUTER + 1, false));

    //go through the topology graph and connect the links
    vector<int> edges(50,0);

    for (unsigned int i = 1; i <= NBR_ROUTER; ++i){
        //cout << i << "   " << topology[i].nbr_link << endl;
        for (unsigned int j = 0; j < topology[i].nbr_link; ++j){
            int linkNum = topology[i].linkcost[j].link;
            int linkCost = topology[i].linkcost[j].cost;
            if (edges[linkNum] == 0){ //first time we've seen this edge
                edges[linkNum] = i; //router i is connected to this edge
            } else {
                //cout << "Edge Between " << i << " and  " << edges[linkNum] << endl;
                graph[i][edges[linkNum]] = linkCost;
                graph[edges[linkNum]][i] = linkCost;
                edgeExist[i][edges[linkNum]] = true;
                edgeExist[edges[linkNum]][i] = true;

            }
        }
    }
}

void computePath(vector<int>&parent, unsigned int routerNum, vector<int>& path, int router_id){
    if (parent[routerNum] == 0 || parent[routerNum] == router_id){
        return;
    }
    computePath(parent, parent[routerNum], path, router_id);
    path.push_back(routerNum);
}

void printPath(vector<int>& path){
    for (unsigned int i = 0; i < path.size(); ++i){
        cout << path[i] << " ";
    }
    cout << endl;
}

void Router::shortestPath(){
    this->initGraph();
    //obtained from https://www.geeksforgeeks.org/printing-paths-dijkstras-shortest-path-algorithm/
    // adapted to this question


    // The output array. dist[i]
    // will hold the shortest
    // distance from src to i
    vector<unsigned int> dist(NBR_ROUTER + 1, UINT_MAX);

    // Parent array to store
    // shortest path tree
    vector<int> parent(NBR_ROUTER + 1, 0);

    // sptSet[i] will true if vertex
    // i is included / in shortest
    // path tree or shortest distance
    // from src to i is finalized
    bool sptSet[NBR_ROUTER + 1];


    // Distance of source vertex
    // from itself is always 0
    dist[router_id] = 0;

    for (int i = 1; i < NBR_ROUTER; ++i){
        if (edgeExist[router_id][i]){
            dist[i] = graph[router_id][i];
            parent[i] = router_id;
        }
    }

    // Find shortest path
    // for all vertices
    for (int count = 1; count <= NBR_ROUTER; count++)
    {
        // Pick the minimum distance
        // vertex from the set of
        // vertices not yet processed.
        // u is always equal to src
        // in first iteration.
        int u = 0;
        // Initialize min value
        unsigned int minVal = UINT_MAX;
        unsigned int min_index = 0;

        for (int v = 1; v <= NBR_ROUTER; v++){
            if (sptSet[v] == false && dist[v] <= minVal){
                minVal = dist[v];
                min_index = v;
            }
        }
        u = min_index;
        if (u == 0){
            break;
        }


        // Mark the picked vertex
        // as processed
        sptSet[u] = true;

        // Update dist value of the
        // adjacent vertices of the
        // picked vertex.
        for (int v = 1; v <= NBR_ROUTER; v++)

            // Update dist[v] only if is
            // not in sptSet, there is
            // an edge from u to v, and
            // total weight of path from
            // src to v through u is smaller
            // than current value of
            // dist[v]
            if (!sptSet[v] && edgeExist[u][v] &&
                dist[u] + graph[u][v] < dist[v])
            {
                parent[v] = u;
                dist[v] = dist[u] + graph[u][v];
            }
    }


    logRIB(parent, dist);

}

void Router::receiveLSPDU(struct pkt_LSPDU lspdu){
    if (lspdu.router_id == this->router_id){
        return;
    }
    //printf("R%d: Received LSPDU: sender R%d, router_id %d, link_id %d, cost %d, via %d\n",
        //router_id, lspdu.sender, lspdu.router_id, lspdu.link_id, lspdu.cost, lspdu.via);
    //fprintf(routerLogFile, "\n\n");
    fprintf(routerLogFile,
        "R%d: Received LSPDU: sender R%d, router_id %d, link_id %d, cost %d, via %d\n",
        router_id, lspdu.sender, lspdu.router_id, lspdu.link_id, lspdu.cost, lspdu.via);

    //if we receive info we already have, just continue
    if (!update(lspdu)) {
        return;
    }
    //calculate new shortest path
    shortestPath();

    // update notifyNeighbours
    for (unsigned int i = 0; i < topology[router_id].nbr_link; ++i) {
        notifyNeighbours(topology[router_id].linkcost[i].link);
    }
}

void Router::receive(){
    int packetLength = 0;
	char buffer[100] = {};
    while (true){
        packetLength = recvfrom(routerFileDescriptor, &buffer, sizeof(buffer), 0, NULL, NULL);
        if (packetLength == sizeof(struct pkt_LSPDU)){
            fprintf(routerLogFile, "\n\n");
            struct pkt_LSPDU *temp = (struct pkt_LSPDU *) buffer;
            struct pkt_LSPDU lspdu = *temp;
            this->receiveLSPDU(lspdu);
        } else { // hello packet
            fprintf(routerLogFile, "\n\n");
            struct pkt_HELLO *hello = (struct pkt_HELLO *) buffer;
			this->notifyNeighbours(hello->link_id);
        }
    }
}
