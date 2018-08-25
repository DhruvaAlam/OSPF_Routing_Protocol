# About
Like most link‐state algorithms, OSPF uses a graph‐theoretic model of network topology to compute
shortest paths. Each router periodically broadcasts information about the status of its connections.
OSPF floods each status message to all participating routers. A router uses arriving link state
information to assemble a graph. Whenever a router receives information that changes its copy of the
topology graph, it runs Dijkstra's algorithm to compute shortest paths in the graph, and
uses the results to build a new next‐hop routing table.

# How To Run
Open two terminal windows, call them HostX and HostY
    where HostX = ubuntu1604-00X and HostY= ubuntu1604-00Y
1. run: ./runnse Y
2. Record the output port of the NSE Emulator (call it Z)
3. On the other terminal window, run: ./runRouter X Z
4. To kill the router processes, run the following to find their job number: jobs
5. Then run: kill %#  where # is the router process you want to kill


Tested on linux.student.cs.uwaterloo.ca servers using two different machines.
g++ was used to compile c++ code and c++14 was used.
make version is the standard make version used on the UW CS servers.
