#pragma once
#include "db.h"
#include "cluster.h"
#include "cluster_router.h"
#include "echo_files.h"
#include "output_clustering.h"

using namespace db;

extern t_molecule_link* unclustered_list_head;
extern t_molecule_link* memory_pool; /*Declared here so I can free easily.*/

// auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
// t_packer_opts &packer_opts = database.vpr_setup->PackerOpts;
// auto& atom_ctx = g_vpr_ctx.atom();
// auto& device_ctx = g_vpr_ctx.mutable_device();
// auto& floorplanning_ctx = g_vpr_ctx.mutable_floorplanning();


class VPR_Pack_Data{
public:
    enum {FREE_ALL_FOR_REPACK,FREE_PARTICAL_AT_LAST};
    int num_clb = 0;
    t_pb_graph_node** primitives_list;
    vtr::vector<ClusterBlockId, std::vector<t_intra_lb_net>*> intra_lb_routing;
    std::shared_ptr<SetupTimingInfo> timing_info;
    t_cluster_placement_stats* cluster_placement_stats;
    std::map<t_logical_block_type_ptr, size_t> num_used_type_instances;//return value of do_clustering
    std::map<const t_model*, std::vector<t_logical_block_type_ptr>> primitive_candidate_block_types;
    t_pb_type* le_pb_type;
    std::vector<int> le_count;
    t_logical_block_type_ptr logic_block_type;
    vtr::vector<ClusterBlockId, std::vector<AtomNetId>> clb_inter_blk_nets;

    void Init();
    void Free(int free_mode = FREE_ALL_FOR_REPACK , bool outputclustering = true);
};