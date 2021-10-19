#ifndef CLUSTER_H
#define CLUSTER_H
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <vector>

#include "physical_types.h"
#include "vpr_types.h"
#include "atom_netlist_fwd.h"
#include "pack_types.h"
#include "vpr_types.h"

std::map<t_logical_block_type_ptr, size_t> do_clustering(const t_packer_opts& packer_opts,
                                                         const t_analysis_opts& analysis_opts,
                                                         const t_arch* arch,
                                                         t_pack_molecule* molecule_head,
                                                         int num_models,
                                                         const std::unordered_set<AtomNetId>& is_clock,
                                                         std::multimap<AtomBlockId, t_pack_molecule*>& atom_molecules,
                                                         const std::unordered_map<AtomBlockId, t_pb_graph_node*>& expected_lowest_cost_pb_gnode,
                                                         bool allow_unrelated_clustering,
                                                         bool balance_block_type_utilization,
                                                         std::vector<t_lb_type_rr_node>* lb_type_rr_graphs,
                                                         const t_ext_pin_util_targets& ext_pin_util_targets,
                                                         const t_pack_high_fanout_thresholds& high_fanout_thresholds);

int get_cluster_of_block(int blkidx);

void print_pb_type_count(const ClusteredNetlist& clb_nlist);

struct t_molecule_stats {
    int num_blocks = 0; //Number of blocks across all primitives in molecule

    int num_pins = 0;        //Number of pins across all primitives in molecule
    int num_input_pins = 0;  //Number of input pins across all primitives in molecule
    int num_output_pins = 0; //Number of output pins across all primitives in molecule

    int num_used_ext_pins = 0;    //Number of *used external* pins across all primitives in molecule
    int num_used_ext_inputs = 0;  //Number of *used external* input pins across all primitives in molecule
    int num_used_ext_outputs = 0; //Number of *used external* output pins across all primitives in molecule
};

t_molecule_stats calc_max_molecules_stats(const t_pack_molecule* molecule_head);
void mark_all_molecules_valid(t_pack_molecule* molecule_head);
int count_molecules(t_pack_molecule* molecule_head);
void alloc_and_init_clustering(const t_molecule_stats& max_molecule_stats,
                                      t_cluster_placement_stats** cluster_placement_stats,
                                      t_pb_graph_node*** primitives_list,
                                      t_pack_molecule* molecules_head,
                                      int num_molecules);
void alloc_and_load_pb_stats(t_pb* pb, const int feasible_block_array_size);
enum e_block_pack_status try_pack_molecule(t_cluster_placement_stats* cluster_placement_stats_ptr,
                                                  const std::multimap<AtomBlockId, t_pack_molecule*>& atom_molecules,
                                                  t_pack_molecule* molecule,
                                                  t_pb_graph_node** primitives_list,
                                                  t_pb* pb,
                                                  const int max_models,
                                                  const int max_cluster_size,
                                                  const ClusterBlockId clb_index,
                                                  const int detailed_routing_stage,
                                                  t_lb_router_data* router_data,
                                                  int verbosity,
                                                  bool enable_pin_feasibility_filter,
                                                  const int feasible_block_array_size,
                                                  t_ext_pin_util max_external_pin_util,
                                                  PartitionRegion& temp_cluster_pr);
void update_cluster_stats(const t_pack_molecule* molecule,
                                 const ClusterBlockId clb_index,
                                 const std::unordered_set<AtomNetId>& is_clock,
                                 const std::unordered_set<AtomNetId>& is_global,
                                 const bool global_clocks,
                                 const float alpha,
                                 const float beta,
                                 const bool timing_driven,
                                 const bool connection_driven,
                                 const int high_fanout_net_threshold,
                                 const SetupTimingInfo& timing_info);
#endif
