#include "workload.h"
#include "common.h"

#include <iostream>
#include <vector>
#include <map>
#include <tuple>
#include <algorithm>


using namespace std;


void Rank::print(){
    cout << "Rank ID: " << id << ", TP: " << tp << ", DP: " << dp << ", PP: " << pp;
    cout << " TP Group ID: " << (tpGroup ? to_string(tpGroup->id) : "None") ;
    cout << ", DP Group ID: " << (dpGroup ? to_string(dpGroup->id) : "None") ;
    cout << ", PP Fwd Group ID: " << (ppFwdGroup ? to_string(ppFwdGroup->id) : "None") ;
    cout << ", PP Bwd Group ID: " << (ppBwdGroup ? to_string(ppBwdGroup->id) : "None") ;    
    cout << ", Host ID: " << (host ? to_string(host->id) : "None") << endl;
}

void Connection::print(){
    cout << "Connection from Rank " << src->id << " to Rank " << dst->id;
    cout << ", Path: ";
    for(auto node : path) {
        cout << node->id << " ";
    }
    cout << ", Path Links: ";
    for(auto link : pathLinks) {
        cout << link->id << " ";
    }
    cout << endl;
}

void Group::print(){
    cout << "Group ID: " << id << ", Type: ";
    switch(type) {
        case TP: cout << "TP"; break;
        case PP: cout << "PP"; break;
        case DP: cout << "DP"; break;
    }
    cout << endl;
    cout << "Ranks in group: " << endl;
    for(auto rank : ranks) {
        rank->print();
    }
    cout << "Connections: ";
    for(auto conn : connections) {
        conn->print();
    }
}

void Workload::print(){
    cout << "Workload Configuration:" << endl;
    cout << "PP: " << PP << ", DP: " << DP << ", TP: " << TP << endl;
    cout << "Microbatches: " << microbatches << endl;
    cout << "Forward Computation Time: " << fwdCompTime << endl;
    cout << "Backward Computation Time: " << bwdCompTime << endl;
    cout << "Forward TP Size: " << fwdTPSize << endl;
    cout << "Backward TP Size: " << bwdTPSize << endl;
    cout << "Forward PP Size: " << fwdPPSize << endl;
    cout << "Backward PP Size: " << bwdPPSize << endl;
    cout << "DP Size: " << dpSize << endl;

    cout << "--------------------------------" << endl;
    cout << "Ranks:" << endl;
    for(auto rank : ranks) {
        cout << "--------------------------------" << endl;
        rank->print();
    }
    cout << "--------------------------------" << endl;
    
    cout << "Groups:" << endl;
    for(auto group : groups) {
        cout << "--------------------------------" << endl;
        group->print();
    }
}



Workload::Workload(int PP, int DP, int TP, int microbatches, 
    double fwdCompTime, double bwdCompTime, double fwdTPSize, double bwdTPSize, 
    double fwdPPSize, double bwdPPSize, double dpSize) :   
    PP(PP), DP(DP), TP(TP), microbatches(microbatches), fwdCompTime(fwdCompTime), bwdCompTime(bwdCompTime),
    fwdTPSize(fwdTPSize), bwdTPSize(bwdTPSize), fwdPPSize(fwdPPSize), bwdPPSize(bwdPPSize), dpSize(dpSize) {
    
    // create ranks
    map<tuple<int, int, int>, Rank*> rankMap; // PP, DP, TP

    int rankId = 0;
    for(int i = 0; i < PP; ++i) {
        for(int j = 0; j < DP; ++j) {
            for(int k = 0; k < TP; ++k) {
                Rank* rank = new Rank(rankId++, i, j, k);
                ranks.push_back(rank);
                rankMap[make_tuple(i, j, k)] = rank; // PP, DP, TP
            }
        }
    }
    
    // cout << "Ranks created: " << ranks.size() << endl;
    // create groups

    map<tuple<int, int, int>, Group*> groupMap; // PP, DP, TP

    int groupId = 0;
    for(int i = 0; i < PP; ++i) { 
        for(int j = 0; j < DP; ++j) {   // TP
            Group* group = new Group(groupId++, GroupType::TP, i, j, -1);
            groups.push_back(group);
            groupMap[make_tuple(i, j, -1)] = group; // TP group
        }
    }

    for(int i = 0; i < PP; ++i) { 
        for(int j = 0; j < TP; ++j) {   // DP
            Group* group = new Group(groupId++, GroupType::DP, i, -1, j);
            groups.push_back(group);
            groupMap[make_tuple(i, -1, j)] = group; // DP group
        }
    }

    // cout << "TP and DP groups created: " << groups.size() << endl;

    // associate ranks and groups
    for(auto rank : ranks) {
        int pp = rank->pp;
        int dp = rank->dp;
        int tp = rank->tp;

        // TP group
        Group* tpGroup = groupMap[make_tuple(pp, dp, -1)];
        rank->tpGroup = tpGroup;
        tpGroup->ranks.push_back(rank);

        // DP group
        Group* dpGroup = groupMap[make_tuple(pp, -1, tp)];
        rank->dpGroup = dpGroup;
        dpGroup->ranks.push_back(rank);
    }

    // cout << "TP/DP groups and ranks associated: " << endl;
    
    // enumerate nodes 

    // cout << "PP, TP, DP: " << PP << ", " << TP << ", " << DP << endl;

    for(int j = 0; j < DP; ++j) {               // DP
        for(int i = 0; i < TP; ++i) {           // TP
            Rank* first = rankMap[make_tuple(0, j, i)];
            Rank* last = rankMap[make_tuple(PP-1, j, i)];
            first->ppBwdGroup = nullptr;  // Changed from == to =
            last->ppFwdGroup = nullptr;
            for(int k = 0; k < PP - 1; ++k) {  // PP
                // cout << "PP, TP, DP; k: " << k << ", i: " << i << ", j: " << j << endl;
                Group *fwdGroup = new Group(groupId++, GroupType::PP, k, i, j);
                Group *bwdGroup = new Group(groupId++, GroupType::PP, k+1, i, j);
                Rank* r1 = rankMap[make_tuple(k, j, i)];
                Rank* r2 = rankMap[make_tuple(k+1, j, i)];
                fwdGroup->ranks.push_back(r1);
                fwdGroup->ranks.push_back(r2);
                bwdGroup->ranks.push_back(r2);
                bwdGroup->ranks.push_back(r1);
                r1->ppFwdGroup = fwdGroup;
                r2->ppBwdGroup = bwdGroup;
                groups.push_back(fwdGroup);
                groups.push_back(bwdGroup);
            }
        }
    }

    // cout << "Workload Constructor" << endl;
    // cout << rankMap[make_tuple(0,0,0)]->ppBwdGroup->id << endl;

    // cout << "PP group created and associated" << endl;
    
    // create connections

    for(auto group : groups) {
        group->createConnections();
    }

}


void Group::createConnections() {
    // TP or DP
    if(type == TP || type == DP) {
        sort(ranks.begin(), ranks.end(), [](Rank* a, Rank* b) {
            return a->id < b->id;
        });
        for(int i = 0; i < ranks.size(); ++i) {
            Rank* src = ranks[i];
            Rank* dst = ranks[(i + 1) % ranks.size()]; // circular connection
            Connection* conn = new Connection(src, dst);
            connections.push_back(conn);
        }
    } else if(type == PP) { // PP
        Rank* r1 = ranks[0];
        Rank* r2 = ranks[1];
        Connection* conn = new Connection(r1, r2);
        connections.push_back(conn);
    }
}


void Workload::configureParallelism(){
    // generate a graph
    int stages = PP;
    int microbatches = this->microbatches;
    int graph[stages][2 * (microbatches + stages - 1)];
    for(int i = 0; i < stages; ++i) {
        for(int j = 0; j < 2 * (microbatches + stages - 1); ++j) {
            graph[i][j] = 0;
        }
    }

    // configure backward [1, mb]
    for(int mb = 1; mb <= microbatches; ++mb) {
        int row = stages - 1;;
        int col = stages + 2 * (mb-1);
        for(int i=0; i < stages; ++i) {
            graph[row - i][col + i] = -mb;
        }
    }

    // configure forward [s+1, mb]
    for(int mb = stages + 1; mb <= microbatches; ++mb) {
        int row = 0;
        int col = stages * 2 + 2 * (mb - stages - 1);
        for(int i=0; i < stages; ++i) {
            graph[row + i][col + i] = mb;
        }
    }

    // configure forward [1, min(s, mb)]
    for(int row = 0; row < stages; ++row) {
        int col = row; 
        int mb = 1;
        while(mb <= min(stages, microbatches)) {
            if(graph[row][col] == 0) {
                graph[row][col] = mb;
                col++;
                mb++;
            }
            else{
                col++;
            }
        }
    }

    // configure rank
    int s, i, j;
    for(s= 0; s<stages; ++s) {
        for(i = s; i< 2*(stages+microbatches-1); i++){
            for(j=i+1; j<2*(microbatches+stages-1); j++){
                if(graph[s][j]!=0) break;
            }
            // cout << "s: " << s << ", i: " << i << ", j: " << j << endl;
            if(j<2*(microbatches+stages-1)){
                int from = graph[s][i];
                int to = graph[s][j];   
                // cout << "from: " << from << ", to: " << to << endl;  
                nextMicrobatch[make_tuple(s, from)] = to;          
                i=j-1;                
            }
            else{
                break;
            }
        }
    }
}


void Workload::placement(){
    // sort rank 
    sort(ranks.begin(), ranks.end(), [](Rank* a, Rank* b) {
        return a->id < b->id;
    });

    // sort host 
    vector<Node*> hosts;
    for(auto node : topology->nodes) {
        if(node->type == NodeType::HOST) {
            hosts.push_back(node);
        }
    }
    sort(hosts.begin(), hosts.end(), [](Node* a, Node* b) {
        return a->id < b->id;
    });

    // mapping
    for(int i = 0; i < ranks.size(); ++i) {
        Rank* rank = ranks[i];
        Node* host = hosts[i % hosts.size()];
        rank->host = host;
        host->rank = rank;
    }
}
void Workload::routing(){
    // iterate on connections
    for(auto group : groups) {
        for(auto conn : group->connections) {
            Node* src = conn->src->host;
            Node* dst = conn->dst->host;
            vector<Node*> path = topology->ECMP(src, dst);
            conn->path = path;
            for(int i = 0; i < path.size() - 1; ++i) {
                Node* src = path[i];
                Node* dst = path[i + 1];
                for(auto link : src->links) {
                    if(link->src == src && link->dst == dst) {
                        conn->pathLinks.push_back(link);
                        break;
                    }
                }
            }
        }
    }
}


