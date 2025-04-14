

#include "topology.h"
#include "workload.h"
#include "common.h"

#include <vector>
#include <iostream>
#include <queue>
#include <unordered_set>
#include <cstdlib>

using namespace std;

void Node::print() {
    switch (type) {
        case HOST: cout << "Host"; break;
        case TOR: cout << "TOR"; break;
        case AGG: cout << "AGG"; break;
        case CORE: cout << "CORE"; break;
    }
    cout << " " << id << endl;
    cout << "Links: ";
    for (auto link : links) {
        cout << link->id << " ";
    }
    cout << endl;
    if (rank) {
        cout << "Rank ID: " << rank->id << endl;
    } else {
        cout << "No rank assigned" << endl;
    }
}


void Link::print() {
    cout << "Link " << id << ": " << src->id << " -> " << dst->id << ", capacity: " << capacity << endl;
}


void Topology::generateFattree(int switch_radix, int pods, double capacity){
    int numHosts = pods * ( switch_radix / 2 ) * ( switch_radix / 2 );
    int numTOR = pods * ( switch_radix / 2 );
    int numAGG = pods * ( switch_radix / 2 );
    int numCore = ( switch_radix * switch_radix ) / 4; 

    int nodeId = 0;
    // build hosts
    for(int i = 0; i < numHosts; ++i) {
        nodes.push_back(new Node(nodeId++, NodeType::HOST));
    }

    // build TOR
    for(int i = 0; i < numTOR; ++i) {
        nodes.push_back(new Node(nodeId++, NodeType::TOR));
    }

    // build AGG
    for(int i = 0; i < numAGG; ++i) {
        nodes.push_back(new Node(nodeId++, NodeType::AGG));
    }

    // build Core
    for(int i = 0; i < numCore; ++i) {
        nodes.push_back(new Node(nodeId++, NodeType::CORE));
    }

    int linkId = 0;
    // connect host-TOR
    for(int i = 0; i < numHosts; ++i) {
        int torIndex = i / (switch_radix / 2);
        Link* link1 = new Link(linkId++, nodes[i], nodes[torIndex + numHosts], capacity);
        Link* link2 = new Link(linkId++, nodes[torIndex + numHosts], nodes[i], capacity);
        links.push_back(link1);
        links.push_back(link2);
        nodes[i]->links.push_back(link1);
        nodes[torIndex + numHosts]->links.push_back(link2);
    }

    // connect TOR-AGG
    for(int i = 0; i < numTOR; ++i) {
        int pod = i / (switch_radix / 2);
        for(int j = 0; j < (switch_radix / 2); ++j) {
            int aggIndex = pod * (switch_radix / 2) + j;
            Link* link1 = new Link(linkId++, nodes[i + numHosts], nodes[aggIndex + numHosts + numTOR], capacity);
            Link* link2 = new Link(linkId++, nodes[aggIndex + numHosts + numTOR], nodes[i + numHosts], capacity);
            links.push_back(link1);
            links.push_back(link2);
            nodes[i + numHosts]->links.push_back(link1);
            nodes[aggIndex + numHosts + numTOR]->links.push_back(link2);
        }
    }

    // connect AGG-Core
    for(int i = 0; i < numAGG; ++i) {
        for(int j = 0; j < (switch_radix / 2); ++j) {
            int coreIndex = j;
            Link* link1 = new Link(linkId++, nodes[i + numHosts + numTOR], nodes[coreIndex + numHosts + numTOR + numAGG], capacity);
            Link* link2 = new Link(linkId++, nodes[coreIndex + numHosts + numTOR + numAGG], nodes[i + numHosts + numTOR], capacity);
            links.push_back(link1);
            links.push_back(link2);
            nodes[i + numHosts + numTOR]->links.push_back(link1);
            nodes[coreIndex + numHosts + numTOR + numAGG]->links.push_back(link2);
        }
    }

}

void Topology::generateOneBigSwitch(int switch_radix, double capacity) {
    int numHosts = switch_radix;
    int nodeId = 0;

    // build hosts
    for (int i = 0; i < numHosts; ++i) {
        nodes.push_back(new Node(nodeId++, NodeType::HOST));
    }

    // build switch
    nodes.push_back(new Node(nodeId++, NodeType::TOR));

    int linkId = 0;
    // connect hosts to switch
    for (int i = 0; i < numHosts; ++i) {
        Link* link1 = new Link(linkId++, nodes[i], nodes[numHosts], capacity);
        Link* link2 = new Link(linkId++, nodes[numHosts], nodes[i], capacity);
        links.push_back(link1);
        links.push_back(link2);
        nodes[i]->links.push_back(link1);
        nodes[numHosts]->links.push_back(link2);
    }
}


vector<Node*> Topology::ECMP(Node* src, Node* dst) {
    vector<vector<Node*>> allPaths;
    queue<vector<Node*>> q;
    unordered_set<Node*> visited;

    // Start BFS from the source node
    q.push({src});

    while (!q.empty()) {
        vector<Node*> path = q.front();
        q.pop();
        Node* current = path.back();

        // If we reach the destination, store the path
        if (current == dst) {
            allPaths.push_back(path);
            continue;
        }

        // Mark the current node as visited for this path
        visited.insert(current);

        // Explore neighbors
        for (auto link : current->links) {
            Node* neighbor = link->dst;
            if (visited.find(neighbor) == visited.end()) {
                vector<Node*> newPath = path;
                newPath.push_back(neighbor);
                q.push(newPath);
            }
        }
    }

    // If no paths are found, return an empty vector
    if (allPaths.empty()) {
        return {};
    }

    // Randomly select one path from allPaths
    int randomIndex = rand() % allPaths.size();
    return allPaths[randomIndex];
}

void Topology::print() {
    cout << "Topology:" << endl;
    cout << "Nodes:" << endl;
    for (auto node : nodes) {
        node->print();
    }
    cout << "Links:" << endl;
    for (auto link : links) {
        link->print();
    }
}
