#ifndef TOPOLOGY_H
#define TOPOLOGY_H
#include "common.h"
#include "simulator.h"
#include "workload.h"

#include <vector>
#include <iostream>
#include <set>

using namespace std;

class Node;
class Link;
class Topology;
class Rank;
class Flow;

class Node {
public:
    int id;
    NodeType type;
    vector<Link*> links;  // directed links from Node
    Node(int id, NodeType type) : id(id), type(type) { rank = nullptr; }

    // workload
    Rank* rank;

    // simulator related
    double throughput;

    void print();
};

class Link {
public:
    int id;
    Node* src;
    Node* dst;
    double capacity;
    Link(int id, Node* src, Node* dst, double capacity = 0.0) : id(id), src(src), dst(dst), capacity(capacity) {}

    // simulator related 
    double throughput;
    set<Flow*> flows; // flows using this link

    void print() ;
};

class Topology {
public:
    vector<Node*> nodes;
    vector<Link*> links;
    Topology() {}
    ~Topology() {
        for (auto node : nodes) {
            delete node;
        }
        for (auto link : links) {
            delete link;
        }
    }

    void generateFattree(int switch_radix, int pods, double capacity);
    void generateOneBigSwitch(int switch_radix, double capacity);
    // void routing();

    vector<Node*> ECMP(Node* src, Node* dst);

    void print();
};



#endif // TOPOLOGY_H