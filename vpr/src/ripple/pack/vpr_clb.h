#pragma once

#include "clb_base.h"
struct ripple_primitive_stats{
    bool valid;
    float base_cost;
    float incremental_cost;
};

// class placement_primitives_for_ripple{//add by jia
// public:
//     t_pb_graph_node* pb_graph_node;
//     float base_cost;
//     float incremental_cost;
// };

// class cluster_placement_stats_for_ripple{//add by jia
// public:
//     vector<vector<placement_primitives_for_ripple>> valid_primitives;
//     vector<placement_primitives_for_ripple> invalid_primitives;
// };


class VPR_CLB{
private:
    ClusterBlockId clb_id;
public:
    VPR_CLB();
    ~VPR_CLB();
    bool AddInsts(const Group& group){}
    bool AddInsts(const Group& group,t_vpr_setup& vpr_setup);
    inline bool IsEmpty(){}
    void GetResult(Group& group){}
    void GetFinalResult(Group& group){}
    void Print() const{}
    
public:
    //t_pb* pb;
    std::vector<t_logical_block_type_ptr> block_types;//VPR type
    //std::map<int,t_cluster_placement_stats*> type_index_to_cluster_placement_stats;
    //t_cluster_placement_stats *cluster_placement_stats;
    //bool IsEmpty(){return pb == nullptr;}
    //t_cluster_placement_stats* cluster_placement_stats;
    bool AddInsts();
    //add by jia 
    bool valid;
    bool is_full; //for io
    ClusterBlockId index;
    t_lb_router_data* router_data;
    PartitionRegion temp_cluster_pr;
    //map<int,cluster_placement_stats_for_ripple> type_index_to_cluster_placement_stats;
    map<int,map<t_pb_type*,map<t_pb_graph_node*,ripple_primitive_stats>>> type_index_to_cluster_placement_stats;
};