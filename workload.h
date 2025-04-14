#ifndef WORKLOAD_H
#define WORKLOAD_H

#include "topology.h"
#include "simulator.h"

#include <vector>
#include <iostream>
#include <map>
#include <set>

using namespace std;

class Rank;
class Group;
class Connection;
class Workload;
class Node;
class RankTask;
class GroupTask;
class Collective;
class Flow;
class Link;
class Topology;

class Rank {
public:
    int id;
    int tp, dp, pp;
    Rank(int id, int tp, int dp, int pp) : id(id), tp(tp), dp(dp), pp(pp) {}

    Group *tpGroup, *ppFwdGroup, *ppBwdGroup, *dpGroup;
    Node* host;

    // simualtor related
    RankTask* rankTask;

    void print();
};

class Group {
public:
    int id;
    GroupType type;
    int pp, dp, tp;
    Group(int id, GroupType type, int pp, int dp, int tp) : id(id), type(type), pp(pp), dp(dp), tp(tp) {}
    
    vector<Rank*> ranks;  // directed links from Group
    vector<Connection*> connections;  // directed links from Group
    void createConnections();

    // simulator related
    GroupTask* groupTask;

    void print();
};

class Connection {
public:
    Rank* src;
    Rank* dst;
    Connection(Rank* src, Rank* dst) : src(src), dst(dst) {}

    vector<Node*> path;
    vector<Link*> pathLinks;

    void print();
};


class Workload {
public:
    vector<Rank*> ranks;
    vector<Group*> groups;

    int PP, DP, TP;
    int microbatches;

    double fwdCompTime, bwdCompTime;
    double fwdTPSize, bwdTPSize;
    double fwdPPSize, bwdPPSize;
    double dpSize; 

    Workload(int PP, int DP, int TP, int microbatches, double fwdCompTime, double bwdCompTime,
             double fwdTPSize, double bwdTPSize, double fwdPPSize, double bwdPPSize, double dpSize);
    ~Workload() {
        for (auto rank : ranks) {
            delete rank;
        }
        for (auto group : groups) {
            delete group;
        }
    }
    
    map<tuple<int, int>, int> nextMicrobatch;
    void configurePipeline();

    Topology *topology;
    void placement();
    void routing();
    
    void print();
};


#endif