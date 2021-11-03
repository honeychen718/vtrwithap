#pragma once

#include "clb_base.h"

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
    t_pb* pb;
    std::vector<t_logical_block_type_ptr> block_types;//VPR type
    t_cluster_placement_stats *cluster_placement_stats;
    //bool IsEmpty(){return pb == nullptr;}
    bool AddInsts();
    //add by jia 
    bool valid;
    ClusterBlockId index;
    t_lb_router_data* router_data;
    PartitionRegion temp_cluster_pr;
};