#include "common.h"
#include "simulator.h"


#include <limits>
#include <vector>
#include <iostream>
#include <cassert>


using namespace std;

extern Topology* topology;
extern Workload* workload;


Flow::Flow(Connection* connection){
    src = connection->src->host;
    dst = connection->dst->host;
    path = connection->path;
    pathLinks = connection->pathLinks;
}

Collective::Collective(Group* group, int microbatch, int accumulatedSize) : 
        group(group), microbatch(microbatch), accumulatedSize(accumulatedSize) {    
    accumulatedInvocations = 1;
    // build flows     
    this->flows.clear();
    if(group->type == GroupType::TP || group->type == GroupType::DP) { // all connections
        for(auto connection : group->connections) {
            Flow* flow = new Flow(connection);
            if(group->type == GroupType::TP) {
                flow->remainingSize = microbatch > 0 ? workload->fwdTPSize : workload->bwdTPSize;
            }
            else{
                flow->remainingSize = workload->dpSize;
            }            
            double factor = 2.0 * (group->ranks.size()-1) / group->ranks.size();
            flow->remainingSize *= factor;
            this->flows.push_back(flow);
            flow->collective = this;
        }
    }
    else { // PP, generate one connection
        Flow* flow = new Flow(group->connections[0]);
        flow->remainingSize = microbatch > 0 ? workload->fwdPPSize : workload->bwdPPSize;
        this->flows.push_back(flow);
        flow->collective = this;
    }
}




RankTask::RankTask(Rank* rank) : rank(rank) {
    rank->rankTask = this;
    this->rank = rank;
    this->dpGroupTask = nullptr; // corrected assignment
    this->tpGroupTask = nullptr;  // corrected assignment
    this->ppFwdGroupTask = nullptr; // corrected assignment
    this->ppBwdGroupTask = nullptr; // corrected assignment
}

GroupTask::GroupTask(Group* group) : group(group) {
    group->groupTask = this;
    this->group = group;
    activeCollective = nullptr;
    this->senders.clear();
    this->receivers.clear();
}




void Simulator::print(){
    cout << "--------------------------" << endl;
    cout << "Simulator:" << endl;
    cout << "Tasks: " << tasks.size() << endl;
    for(auto task : tasks) {
        task->printStates();
    }
    cout << "--------------------------" << endl;

}

void RankTask::printStates(){
    cout << "---------------------------" << endl;
    cout << "RankTask:" ;
    cout << " Rank: " << rank->id ;
    cout << ", State: " ;
    switch(state) {
        case PP_WAIT:
            cout << "PP_WAIT";
            break;
        case COMPUTE:
            cout << "COMPUTE";
            break;
        case TP_COMM:
            cout << "TP_COMM";
            break;
        case DP_WAIT:
            cout << "DP_WAIT";
            break;
        case DP_COMM:
            cout << "DP_COMM";
            break;
        case DONE:
            cout << "DONE";
            break;
    }
    cout << ", Microbatch: " << microbatch ;
    cout << ", Remaining time: " << remainingTime ;
    cout << ", Events: " << events.size() << ": ";
    for(auto event : events) {
        string event_str = "<";
        switch(get<0>(event)) {
            case EndpointType::SENT:
                event_str += "SENT, ";
                break;
            case EndpointType::RECV:
                event_str += "RECV, ";
                break;
        }
        switch(get<1>(event)) {
            case GroupType::TP:
                event_str += "TP, ";
                break;
            case GroupType::PP:
                event_str += "PP, ";
                break;
            case GroupType::DP:
                event_str += "DP, ";
                break;
        }
        event_str += to_string(get<2>(event)) + ">";
        cout << event_str << " ";
    }
    cout << endl;
}

void Collective::printStates(){
    cout << "Collective:" ;
    cout << " Group: " << group->id <<", Group Size: " << group->ranks.size() ;
    cout << " Microbatch: " << microbatch ;
    cout << " Accumulated invocations: " << accumulatedInvocations ;
    cout << endl;
    for(auto flow : flows) {
        cout << "Flow: " ;
        cout << flow->src->id << "->" << flow->dst->id ;
        cout << ", Remaining size: " << flow->remainingSize ;
        cout << ", Throughput: " << flow->throughput ;
        cout << endl;
    }
}

void GroupTask::printStates(){
    cout << "---------------------------" << endl;
    cout << "GroupTask:" ;
    cout << " Group: " << group->id ;
    cout << ", GroupType: " ;
    switch(group->type) {
        case GroupType::TP:
            cout << "TP";
            break;
        case GroupType::PP:
            cout << "PP";
            break;
        case GroupType::DP:
            cout << "DP";
            break;
    }
    cout << ", Group Senders: ";
    for(auto rankTask : senders) {
        cout << rankTask->rank->id << " ";
    }
    cout << ", Group Receivers: ";
    for(auto rankTask : receivers) {
        cout << rankTask->rank->id << " ";
    }

    cout << ", Waiting collectives: ";
    for(auto collective : waitingCollectives) {
        cout << collective->microbatch << " ";
    }
    cout << "; Accumulating collectives: ";
    for(auto it : accumulatingCollectives) {
        cout << "microbatch: " << it.first <<  ", accmSize: " << it.second->accumulatedInvocations << "; ";
    }

    cout << endl;
    cout << "Active collective: " ;
    if(activeCollective != nullptr) {
        activeCollective->printStates();
    }
    else {
        cout << "None" ;
    }
    cout << endl;
    cout << "Events: " << events.size() << ": ";
    for(auto event : events) {
        string event_str = "<From: " + to_string(get<0>(event)) + ", ";
        event_str += "microbatch: " + to_string(get<1>(event)) + ">";
        cout << event_str << " ";
    }
    cout << endl;
}

void Simulator::printStates(){
    cout << "---------------------------" << endl;
    cout << "Simulator:" << endl;
    for(auto task : tasks) {
        task->printStates();
    }
    cout << "---------------------------" << endl;
}



int RankTask::handleEvents(){   // < EP, TYPE, MB >
    int countEvents = events.size();
    for(auto it = events.begin(); it != events.end(); ) {
        int ep = get<0>(*it);
        int type = get<1>(*it);
        int mb = get<2>(*it);
        if ( type == GroupType::TP ) {
            if(ep == EndpointType::SENT) {
                it = events.erase(it); continue;
            }
            else {  // RECV
                // start PP
                if(state != RankState::TP_COMM){
                    it++; continue;
                }

                assert(mb == microbatch);  // otherwise, error
 
                if(microbatch > 0 && ppFwdGroupTask != nullptr){ // forward
                    ppFwdGroupTask->events.push_back(make_tuple(rank->id, microbatch));
                }
                else if(microbatch < 0 && ppBwdGroupTask != nullptr){ // backward
                    ppBwdGroupTask->events.push_back(make_tuple(rank->id, microbatch));
                }
                // transit to next MB; 
                if(workload->nextMicrobatch.find(make_tuple(rank->pp, microbatch)) != workload->nextMicrobatch.end()){
                    microbatch = workload->nextMicrobatch[make_tuple(rank->pp, microbatch)];
                    state = RankState::PP_WAIT;
                }
                else {
                    state = RankState::DP_WAIT;
                }
                it = events.erase(it);
            }

        } 
        else if( type == GroupType::DP ) {
            if(ep == EndpointType::SENT) {
                it = events.erase(it); continue;
            }
            // RECV
            // transit to complete
            if(state == RankState::DP_COMM){
                state = RankState::DONE;
                it = events.erase(it); continue;
            }
            else{
                it++; continue;
            }            
        }
        else { // PP
            if(ep == EndpointType::SENT) {
                // if last mb, check to transit to DP_COMM
                // else ignore
                if( mb == -workload->microbatches ) {
                    if(state == RankState::DP_WAIT) {
                        state = RankState::DP_COMM;
                        dpGroupTask->events.push_back(make_tuple(rank->id, 0));
                        it = events.erase(it); continue;
                    }
                    else{
                        it++; continue;
                    }
                }
                else{
                    it = events.erase(it); continue;
                }
            }
            else {  // RECV
                // transit to compute
                if(state == RankState::PP_WAIT && mb == microbatch){
                    state = RankState::COMPUTE;
                    remainingTime = microbatch > 0 ? workload->fwdCompTime : workload->bwdCompTime;
                    it = events.erase(it); continue;
                }
                else{
                    it++; continue;
                }
            }
        }
    }
    return countEvents - events.size();
}

int GroupTask::handleEvents(){  // < From, MB >
    // get from, mb
    int countEvents = events.size();
    for (auto it = events.begin(); it != events.end(); ) {
        int from = get<0>(*it);  // 事件来源
        int mb = get<1>(*it);    // 微批次
        // 检查是否有等待的集合操作
        if (accumulatingCollectives.find(mb) == accumulatingCollectives.end()) {
            Collective* collective = new Collective(group, mb, group->type == GroupType::PP ? 1 : group->ranks.size());
            accumulatingCollectives[mb] = collective;
        }
        else {
            accumulatingCollectives[mb]->accumulatedInvocations++;
        }
        // 移除已处理的事件
        it = events.erase(it);
    }
    for(auto it = accumulatingCollectives.begin(); it != accumulatingCollectives.end(); ) {
        int mb = it->first;
        Collective* collective = it->second;

        // 如果集合操作已完成，移动到等待队列
        if (collective->accumulatedInvocations == collective->accumulatedSize) {
            waitingCollectives.push_back(collective);
            it = accumulatingCollectives.erase(it);
        }
        else {
            ++it;
        }
    }
    // 如果没有活动的集合操作，尝试激活一个等待的集合操作
    if (activeCollective == nullptr && !waitingCollectives.empty()) {
        activeCollective = waitingCollectives.front();
        waitingCollectives.erase(waitingCollectives.begin());
    }
    return countEvents - events.size();
}


double Flow::stableTime(){
    return remainingSize/throughput;
}

double Collective::stableTime(){
    double time = numeric_limits<double>::infinity();
    for(auto flow : flows){
        double t = flow->stableTime();
        if(t < time) time = t;
    }
    return time;
}

double GroupTask::stableTime(){
    if (activeCollective==nullptr) {
        if(waitingCollectives.empty()) {
            return numeric_limits<double>::infinity();
        }
        else{
            return 0;
        }
    }
    else{
        return activeCollective->stableTime();
    }
}

double RankTask::stableTime(){
    switch(state) {
        case COMPUTE:
            return remainingTime;
        default:
            return numeric_limits<double>::infinity();
    }
}

void Flow::progress(double time){
    if(remainingSize<1e-6) {
        remainingSize = 0;
    }
    else{
        remainingSize -= throughput * time;
    }
}

void Collective::progress(double time){
    for(auto flow : flows){
        flow->progress(time);
    }
}

void GroupTask::progress(double time){
    // move waiting to active
    if(activeCollective == nullptr) {
        if(!waitingCollectives.empty()) {
            activeCollective = waitingCollectives.front();
            waitingCollectives.erase(waitingCollectives.begin());
        }
    }
    if(activeCollective == nullptr) 
        return ;

    activeCollective->progress(time);
    if(activeCollective->flows[0]->remainingSize <= 1e-6 ) {   // EP TYPE MB
        // notify senders
        tuple<int, int, int> event = make_tuple(EndpointType::SENT, group->type, activeCollective->microbatch);
        for(auto rankTask : senders){
            rankTask->events.push_back(event);
        }

        // notify receivers
        tuple<int, int, int> event2 = make_tuple(EndpointType::RECV, group->type, activeCollective->microbatch);
        for(auto task: receivers){
            task->events.push_back(event2);
        }

        delete activeCollective;  
        activeCollective = nullptr;
        if(!waitingCollectives.empty()) {
            activeCollective = waitingCollectives.front();
            waitingCollectives.erase(waitingCollectives.begin());
        }
    }

}


void RankTask::progress(double time){
    switch(state) {
        case COMPUTE:
            remainingTime -= time;
            if(remainingTime <= 1e-6) {
                state = TP_COMM;
                remainingTime = 0;
                tpGroupTask->events.push_back(make_tuple(rank->id, microbatch));
            }
            break;
        default:
            break;
    }
}

void Simulator::initialize(){

    // create tasks, 
    for(auto group : workload->groups) {
        GroupTask* task = new GroupTask(group);
        tasks.push_back(task);
    }
    for(auto rank : workload->ranks) {
        RankTask* task = new RankTask(rank);
        task->microbatch = 1;
        tasks.push_back(task);
    }

    // associate tasks;
    for(auto rank : workload->ranks) {
        RankTask* task = rank->rankTask;
        GroupTask* tpGroupTask = rank->tpGroup->groupTask;
        GroupTask* dpGroupTask = rank->dpGroup->groupTask;

        task->tpGroupTask = tpGroupTask;
        task->dpGroupTask = dpGroupTask;
        tpGroupTask->senders.push_back(task);
        tpGroupTask->receivers.push_back(task);
        dpGroupTask->senders.push_back(task);
        dpGroupTask->receivers.push_back(task);

        if(rank->ppFwdGroup != nullptr){            
            RankTask* fwdReceiverTask = rank->ppFwdGroup->ranks[1]->rankTask;
            GroupTask* ppFwdGroupTask = rank->ppFwdGroup->groupTask;               
            task->ppFwdGroupTask = ppFwdGroupTask;
            ppFwdGroupTask->senders.push_back(task);
            ppFwdGroupTask->receivers.push_back(fwdReceiverTask);
        }

        if(rank->ppBwdGroup != nullptr){
            RankTask* bwdReceiverTask = rank->ppBwdGroup->ranks[1]->rankTask;
            GroupTask* ppBwdGroupTask = rank->ppBwdGroup->groupTask; 
            task->ppBwdGroupTask = ppBwdGroupTask;
            ppBwdGroupTask->senders.push_back(task);
            ppBwdGroupTask->receivers.push_back(bwdReceiverTask);
        }
    }

    // init rank microbatch
    for(auto rankTask : tasks) {
        if(dynamic_cast<RankTask*>(rankTask) != nullptr) {
            RankTask* task = dynamic_cast<RankTask*>(rankTask);
            task->microbatch = 1;
            task->state = RankState::PP_WAIT;
        }
    }

    // prepare notifications,
    // all stage 0 (PP)    
    // all stage  -1  (PP)
    // stage 0 (DP)
    for(auto rank : workload->ranks) {
        if(rank->pp == 0) { // add all forward events
            RankTask* task = rank->rankTask;
            for(int i = 1; i <= workload->microbatches; i++){
                tuple<int, int, int> event = make_tuple(EndpointType::RECV, GroupType::PP, i);
                task->events.push_back(event);
            }
        }
        if(rank->pp == workload->PP - 1) { // add all backward events
            RankTask* task = rank->rankTask;
            for(int i = 1; i <= workload->microbatches; i++){
                tuple<int, int, int> event = make_tuple(EndpointType::RECV, GroupType::PP, -i);
                task->events.push_back(event);
            }
        }
        if(rank->pp == workload->PP - 1){
            RankTask* task = rank->rankTask;
            tuple<int, int, int> event = make_tuple(EndpointType::SENT, GroupType::PP, -workload->microbatches);
            task->events.push_back(event);
        }
    }
}

void Simulator::updateStates(){
    // collective active flows
    set<Flow*> activeFlows;
    for(auto task : tasks){

        if(dynamic_cast<GroupTask*>(task) != nullptr) {
            GroupTask* groupTask = dynamic_cast<GroupTask*>(task);
            if(groupTask->activeCollective != nullptr) {
                for(auto flow : groupTask->activeCollective->flows) {
                    activeFlows.insert(flow);
                }
            }
        }
    }
    // update flow throughput
    for(auto flow : activeFlows){
        flow->throughput = 0;
    }

    // collective active links
    set<Link*> activeLinks;
    for(auto flow : activeFlows){
        for(auto link : flow->pathLinks){
            activeLinks.insert(link);
        }
    }

    // update link throughput
    for(auto link : activeLinks){
        link->throughput = 0;
        link->flows.clear();
    }

    // update link flows
    for(auto flow : activeFlows){
        for(auto link : flow->pathLinks){
            link->flows.insert(flow);
        }
    }

    // update throughput
    while(!activeFlows.empty() && !activeLinks.empty()) { // water filling
        // iterate links to get minimum throughput
        double minAug = numeric_limits<double>::infinity();
        for(auto link : activeLinks) {
            double aug = (link->capacity - link->throughput)/link->flows.size();
            if(aug < minAug) {
                minAug = aug;
            }
        }
        // update flows 
        for(auto flow : activeFlows) {
            flow->throughput += minAug;
        }
        // update links
        for(auto link : activeLinks) {
            link->throughput += minAug * link->flows.size();
        } 
        // freeze link
        set<Link*> frozenLinks;
        for(auto link : activeLinks) {
            if(link->throughput >= link->capacity - 1e-6) {
                frozenLinks.insert(link);
            }
        }
        // freeze flows
        set<Flow*> frozenFlows;
        for(auto link : frozenLinks) {
            for(auto flow : link->flows) {
                frozenFlows.insert(flow);
            }
        }
        // freeze flows in the same collective 
        for(auto flow : frozenFlows) {
            for(auto other : flow->collective->flows) {
                if(other != flow) {
                    frozenFlows.insert(other);
                }
            }
        }
        // remove frozen flows
        for(auto flow : frozenFlows) {
            activeFlows.erase(flow);
        }
        // remove frozen links
        for(auto link : frozenLinks) {
            activeLinks.erase(link);
        }
    }

    // if active flows is not empty, it is internal, it completes immediately
    for(auto flow : activeFlows) {
        flow->throughput = numeric_limits<double>::infinity();
        // flow->remainingSize = 0;
    }
}

void Simulator::run(){
    globalTime=0;
    cout << "===========================" << endl;
    int round = 0;
    int targetRound = -1;    
    while(true){
        // cout << "===========================" << endl;
        // cout << "Global Time: " << globalTime << ", Round " << round << endl;
        // cout << "----------------------------" << endl;
        // cout << " before handle events" << endl;
        if(round==targetRound) printStates(); // !!!!!!!!!!!!!!

        while(1){
            int countEvents = 0;
            for(auto task : tasks){
                countEvents += task->handleEvents();
            }
            if(countEvents == 0) break;
        }
        // cout << " after handle events, before update states" << endl;
        if(round==targetRound) printStates(); // !!!!!!!!!!!!!!
        // update states
        updateStates();
        // cout << "----------------------------" << endl;
        // cout << " after update states " << endl;
        if(round==targetRound) printStates(); // !!!!!!!!!!!!!!!
        // stable time
        double time = numeric_limits<double>::infinity();
        for(auto task : tasks){
            double t = task->stableTime();
            if(t < time) time = t;
        }
        // cout << "----------------------------" << endl;
        // cout << "Stable time: " << time << endl;
        if(time == numeric_limits<double>::infinity()){
            break;
        }
        // progress
        for(auto task : tasks){
            task->progress(time);
        }
        globalTime += time;

        // cout << "Progressed time: " << time << endl;
        // cout << "---------------------------" << endl;
        if(round==targetRound )printStates(); // !!!!!!!!!!!!!!!
        // cout << "===========================" << endl;
        round++;
    }
    cout << "Simulation finished" << endl;
    cout << "Global Time: " << globalTime << endl;
    cout << "---------------------------" << endl;
}