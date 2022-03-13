#pragma once
#include "db.h"
#include "vtr_math.h"
#include "cluster.h"
#include "cluster_router.h"
#include "cluster_placement.h"
#include "vpr_pack_data.h"
#include "vpr_pack_utils.h"

using namespace db;

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
    void read_cluster_placement_stats_from_clb(int type_index,t_cluster_placement_stats* cluster_placement_stats_ptr);
    void write_cluster_placement_stats_to_clb(t_cluster_placement_stats* cluster_placement_stats);
    bool try_start_new_cluster(const Group& group);
    bool vpr_start_new_cluster(const Group& group);
public:
    VPR_CLB();
    VPR_CLB(VPR_Pack_Data* packdata, Site* _site);
    VPR_CLB(VPR_Pack_Data* vprpackdata , Site* _site , bool _keeprouterdata);
    ~VPR_CLB();
    bool TryAddInsts(const Group& group);
    bool AddInsts(const Group& group);
    void RemoveGroup(const Group& group);
    void Deleteblock();
    //bool AddInsts(const Group& group,t_vpr_setup& vpr_setup);
    bool IsEmpty();
    void GetResult(Group& group);
    void GetFinalResult(Group& group){}
    void Print() const{}

public:
    std::vector<t_logical_block_type_ptr> block_types;//VPR posibal type
    int typeindex;
    bool keeprouterdata=true; // 0 or try , 1 for output  //set to 1 now , waiting for debug 
    bool CLBIsEmpty = false;
    bool is_full = false; //for io , delete it once bad merge group_to_site is deleted
    ClusterBlockId index;
    t_lb_router_data* router_data=nullptr;
    t_pb* pb;
    PartitionRegion temp_cluster_pr;
    map<int,map<t_pb_type*,map<t_pb_graph_node*,ripple_primitive_stats>>> type_index_to_cluster_placement_stats;    vector<const Group*> groups;
    Site* site;
    VPR_Pack_Data* packdata;
};