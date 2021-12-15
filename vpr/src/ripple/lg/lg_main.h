#ifndef _LG_MAIN_H_
#define _LG_MAIN_H_

#include "db/db.h"
#include "./lg_data.h"
#include "cluster.h"
#include "cluster_router.h"
#include "cluster_placement.h"
#include "timing_info.h"
using namespace db;

enum ChainmoveTarget { DISP_OPT, MAX_DISP_OPT };

class Legalizer {
private:
    lgSiteOrder siteOrder;
    lgGroupOrder groupOrder;

    vector<Group> &groups;

    bool MergeGroupToSite(Site *site, Group &group, bool fixDspRam = true);
    bool MergeMoleculeToSite(  Site *site, t_pack_molecule* molecule, bool fixDspRam ,
                            t_packer_opts& packer_opts,
                            std::vector<t_lb_type_rr_node>* lb_type_rr_graphs,
                            std::multimap<AtomBlockId, t_pack_molecule*>& atom_molecules,
                            t_pb_graph_node** primitives_list,
                            int max_cluster_size,ClusteredNetlist* clb_nlist,
                            std::map<t_logical_block_type_ptr, size_t>& num_used_type_instances,
                            //const std::unordered_set<AtomNetId>& is_clock,
                            //const t_pack_high_fanout_thresholds& high_fanout_thresholds,
                            //std::shared_ptr<SetupTimingInfo>& timing_info,
                            const t_ext_pin_util_targets& ext_pin_util_targets,
                            //vtr::vector<ClusterBlockId, std::vector<t_intra_lb_net>*>& intra_lb_routing,
                            //vtr::vector<ClusterBlockId, std::vector<AtomNetId>>& clb_inter_blk_nets,
                            //t_pb_type* le_pb_type,
                            int& num_clb,
                            const std::map<const t_model*, std::vector<t_logical_block_type_ptr>>& primitive_candidate_block_types,
                            bool balance_block_type_utilization,
                            t_cluster_placement_stats* cluster_placement_stats);
    bool AssignPackToSite(Site *site, Group &group);

    void SortCandSitesByHpwl(vector<Site *> &candSites, const Group &group);
    void SortCandSitesByPins(vector<Site *> &candSites, const Group &group);
    void SortCandSitesByAlign(vector<Site *> &candSites, const Group &group);
    void SortGroupsByGroupsize();
    void SortGroupsByPins();
    void SortGroupsByLGBox();
    void SortGroupsByOptRgnDist();

    void BipartiteLeg(SiteType::Name type, int minNumSites);
    int ChainMove(Group &group, ChainmoveTarget optTarget);

public:
    LGData lgData;

    Legalizer(vector<Group> &_groups);
    void Init(lgPackMethod packMethod);
    void GetResult(lgRetrunGroup retGroup);

    bool RunAll(lgSiteOrder siteOrder, lgGroupOrder groupOrder);
    bool RunAll(lgSiteOrder siteOrder, lgGroupOrder groupOrder,
                t_packer_opts& packer_opts,
                std::vector<t_lb_type_rr_node>* lb_type_rr_graphs,
                std::multimap<AtomBlockId, t_pack_molecule*>& atom_molecules,
                t_pb_graph_node** primitives_list,
                int max_cluster_size,ClusteredNetlist* clb_nlist,
                std::map<t_logical_block_type_ptr, size_t>& num_used_type_instances,
                const std::unordered_set<AtomNetId>& is_clock,const t_pack_high_fanout_thresholds& high_fanout_thresholds,
                std::shared_ptr<SetupTimingInfo>& timing_info,const t_ext_pin_util_targets& ext_pin_util_targets,
                vtr::vector<ClusterBlockId, std::vector<t_intra_lb_net>*>& intra_lb_routing,
                vtr::vector<ClusterBlockId, std::vector<AtomNetId>>& clb_inter_blk_nets,
                t_logical_block_type_ptr& logic_block_type,t_pb_type* le_pb_type,
                std::vector<int>& le_count,int& num_clb,
                const std::map<const t_model*, std::vector<t_logical_block_type_ptr>>& primitive_candidate_block_types,
                bool balance_block_type_utilization,
                t_cluster_placement_stats* cluster_placement_stats);
    void RunPartial();

    double GetHpwl();
};

//extern t_molecule_stats calc_max_molecules_stats(const t_pack_molecule* molecule_head);
// extern void mark_all_molecules_valid(t_pack_molecule* molecule_head);
// extern int count_molecules(t_pack_molecule* molecule_head);
// extern void alloc_and_init_clustering(const t_molecule_stats& max_molecule_stats,
//                                       t_cluster_placement_stats** cluster_placement_stats,
//                                       t_pb_graph_node*** primitives_list,
//                                       t_pack_molecule* molecules_head,
//                                       int num_molecules);
//extern void alloc_and_load_pb_stats(t_pb* pb, const int feasible_block_array_size);
#endif
