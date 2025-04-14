#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "workload.h"
#include "topology.h"

#include <vector>
#include <iostream>
#include <limits>
#include <map>
#include <tuple>
#include <set>

using namespace std;


class Task;
class GroupTask;
class RankTask;
class Collective;
class Flow;
class Simulator;
class Node;
class Link;
class Rank;
class Connection;
class Group;
class Workload;
class Topology;


class Task {
public:
    virtual int handleEvents() = 0;
    virtual double stableTime() = 0;
    virtual void progress(double time) = 0;


    virtual void printStates() = 0;
};

class Flow {
public:
    Node* src;
    Node* dst;
    vector<Node*> path;
    vector<Link*> pathLinks;    
    Flow(Connection* connection);

    double remainingSize;
    double throughput;
    Collective* collective;

    double stableTime();
    void progress(double time);
};

class Collective {
public:
    vector<Flow*> flows;
    Group* group;

    int microbatch;
    int accumulatedInvocations;
    int accumulatedSize;

    double tableTime();
    double stableTime();
    void progress(double time);

    Collective(Group* group, int microbatch, int accumulatedSize);

    void printStates();
};

class GroupTask : public Task {
public:

    Group* group;
    
    GroupTask(Group* group) ;

    vector<RankTask*> senders;
    vector<RankTask*> receivers;

    Collective* activeCollective;
    vector<Collective*> waitingCollectives;
    map<int, Collective*> accumulatingCollectives; // from microbatchId to collective

    vector<tuple<int, int>> events;    // < From, MB >

    int handleEvents();
    double stableTime();
    void progress(double time);

    void printStates() ;

};

class RankTask : public Task {
public:
    Rank* rank;
    RankTask(Rank* rank) ;

    GroupTask* ppFwdGroupTask;
    GroupTask* ppBwdGroupTask;
    GroupTask* dpGroupTask;
    GroupTask* tpGroupTask;

    RankState state;
    int microbatch;
    double remainingTime;

    vector<tuple<int, int, int>> events; // < EP, TYPE, MB >

    int handleEvents();
    double stableTime();
    void progress(double time);

    void printStates() ;
};


class Simulator {
public:
    Workload* workload;
    Topology* topology;

    vector<Task*> tasks;
    double globalTime;

    void initialize();
    void updateStates(); // waiter filling
    void run() ;

    void printStates();
    void print() ;
};


#endif // SIMULATOR_H