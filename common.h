#ifndef COMMON_H
#define COMMON_H

enum GroupType {
    TP,
    PP,
    DP
};

enum NodeType {
    HOST,
    TOR,
    AGG,
    CORE
};

enum RankState {
    PP_WAIT,
    COMPUTE,
    TP_COMM,
    DP_WAIT,
    DP_COMM,
    DONE,
};

enum EndpointType {
    SENT,
    RECV,
};

#endif