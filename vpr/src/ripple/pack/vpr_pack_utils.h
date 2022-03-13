#ifndef VPR_PACK_UTILS_H_
#define VPR_PACK_UTILS_H_
#include "vtr_math.h"
#include "cluster.h"
#include "cluster_router.h"
#include "cluster_placement.h"
#include "vpr_pack_data.h"
enum e_block_pack_status try_atom_cluster_floorplanning_check(const AtomBlockId blk_id,
                                                                 const int verbosity,
                                                                 PartitionRegion& temp_cluster_pr,
                                                                 bool& cluster_pr_needs_update);

bool try_primitive_memory_sibling_feasible(const AtomBlockId blk_id, const t_pb_type* cur_pb_type, const AtomBlockId sibling_blk_id);

bool try_primitive_feasible(const AtomBlockId blk_id, t_pb* cur_pb);

// enum e_block_pack_status try_try_place_atom_block_rec(const t_pb_graph_node* pb_graph_node,
//                                                          const AtomBlockId blk_id,
//                                                          t_pb* cb,
//                                                          t_pb** parent,
//                                                          const int max_models,
//                                                          const int max_cluster_size,
//                                                          const ClusterBlockId clb_index,
//                                                          const t_cluster_placement_stats* cluster_placement_stats_ptr,
//                                                          const t_pack_molecule* molecule,
//                                                          t_lb_router_data* router_data,
//                                                          int verbosity,
//                                                          const int feasible_block_array_size);

enum e_block_pack_status try_try_pack_molecule(t_cluster_placement_stats* cluster_placement_stats_ptr,
                                                  const std::multimap<AtomBlockId, t_pack_molecule*>& atom_molecules,
                                                  t_pack_molecule* molecule,
                                                  t_pb_graph_node** primitives_list,
                                                  t_pb* pb,
                                                  const ClusterBlockId clb_index,
                                                  const int max_models,
                                                  t_lb_router_data* router_data,
                                                  int verbosity,
                                                  bool enable_pin_feasibility_filter,
                                                  const int feasible_block_array_size,
                                                  t_ext_pin_util max_external_pin_util,
                                                  PartitionRegion& temp_cluster_pr);                                                         

#endif //!VPR_PACK_UTILS_H_