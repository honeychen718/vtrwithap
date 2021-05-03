/*
 * place_constraints.cpp
 *
 *  Created on: Mar. 1, 2021
 *      Author: khalid88
 *
 *  This file contains routines that help with making sure floorplanning constraints are respected throughout
 *  the placement stage of VPR.
 */

#include "globals.h"
#include "place_constraints.h"

/*checks that each block's location is compatible with its floorplanning constraints if it has any*/
int check_placement_floorplanning() {
    int error = 0;
    auto& cluster_ctx = g_vpr_ctx.clustering();
    auto& place_ctx = g_vpr_ctx.placement();

    for (auto blk_id : cluster_ctx.clb_nlist.blocks()) {
        auto& loc = place_ctx.block_locs[blk_id].loc;
        if (!cluster_floorplanning_check(blk_id, loc)) {
            error++;
            VTR_LOG_ERROR("Block %zu is not in correct floorplanning region.\n", size_t(blk_id));
        }
    }

    return error;
}

/*returns true if cluster has floorplanning constraints, false if it doesn't*/
bool is_cluster_constrained(ClusterBlockId blk_id) {
    auto& floorplanning_ctx = g_vpr_ctx.floorplanning();
    PartitionRegion pr;
    pr = floorplanning_ctx.cluster_constraints[blk_id];
    return (!pr.empty());
}

bool is_macro_constrained(t_pl_macro pl_macro) {
    bool is_macro_constrained = false;
    bool is_member_constrained = false;

    for (size_t imember = 0; imember < pl_macro.members.size(); imember++) {
        ClusterBlockId iblk = pl_macro.members[imember].blk_index;
        is_member_constrained = is_cluster_constrained(iblk);

        if (is_member_constrained) {
            is_macro_constrained = true;
            break;
        }
    }

    return is_macro_constrained;
}

/*Returns PartitionRegion of where the head of the macro could go*/
PartitionRegion constrained_macro_locs(t_pl_macro pl_macro) {
    PartitionRegion macro_pr;
    bool is_member_constrained = false;
    int num_constrained_members = 0;
    auto& floorplanning_ctx = g_vpr_ctx.floorplanning();

    for (size_t imember = 0; imember < pl_macro.members.size(); imember++) {
        ClusterBlockId iblk = pl_macro.members[imember].blk_index;
        is_member_constrained = is_cluster_constrained(iblk);

        if (is_member_constrained) {
            num_constrained_members++;
            //PartitionRegion of the constrained block
            PartitionRegion block_pr;
            //PartitionRegion of the constrained block modified for the head according to the offset
            PartitionRegion modified_pr;

            block_pr = floorplanning_ctx.cluster_constraints[iblk];
            std::vector<Region> block_regions = block_pr.get_partition_region();

            for (unsigned int i = 0; i < block_regions.size(); i++) {
                Region modified_reg;
                auto offset = pl_macro.members[imember].offset;

                vtr::Rect<int> reg_rect = block_regions[i].get_region_rect();

                t_pl_loc min_pl_loc(reg_rect.xmin(), reg_rect.ymin(), block_regions[i].get_sub_tile());

                t_pl_loc modified_min_pl_loc = min_pl_loc + offset;

                t_pl_loc max_pl_loc(reg_rect.xmax(), reg_rect.ymax(), block_regions[i].get_sub_tile());

                t_pl_loc modified_max_pl_loc = max_pl_loc + offset;

                modified_reg.set_region_rect(modified_min_pl_loc.x, modified_min_pl_loc.y, modified_max_pl_loc.x, modified_max_pl_loc.y);
                //check that subtile is not an invalid value before changing, otherwise it just stays -1
                if (block_regions[i].get_sub_tile() != -1) {
                    modified_reg.set_sub_tile(modified_min_pl_loc.sub_tile);
                }

                modified_pr.add_to_part_region(modified_reg);
            }

            if (num_constrained_members == 1) {
                macro_pr = modified_pr;
            } else {
                macro_pr = intersection(macro_pr, modified_pr);
            }
        }
    }

    //if the intersection is empty, no way to place macro members together, give an error
    if (macro_pr.empty()) {
        VPR_ERROR(VPR_ERROR_PLACE, " \n Feasible floorplanning constraints could not be calculated for the placement macro.\n");
    }

    return macro_pr;
}

/*returns true if location is compatible with floorplanning constraints, false if not*/
bool cluster_floorplanning_check(ClusterBlockId blk_id, t_pl_loc loc) {
    auto& floorplanning_ctx = g_vpr_ctx.floorplanning();

    bool floorplanning_good = false;

    bool cluster_constrained = is_cluster_constrained(blk_id);

    if (!cluster_constrained) {
        //not constrained so will not have floorplanning issues
        floorplanning_good = true;
    } else {
        PartitionRegion pr;
        pr = floorplanning_ctx.cluster_constraints[blk_id];
        bool in_pr = pr.is_loc_in_part_reg(loc);

        //if location is in partitionregion, floorplanning is respected
        //if not it is not
        if (in_pr) {
            floorplanning_good = true;
        } else {
            VTR_LOG("Block %zu did not pass cluster_floorplanning_check \n", size_t(blk_id));
            VTR_LOG("Loc is x: %d, y: %d, subtile: %d \n", loc.x, loc.y, loc.sub_tile);
        }
    }

    return floorplanning_good;
}

void check_cluster_constraints_init(std::string constraints_file) {
    auto& floorplanning_ctx = g_vpr_ctx.mutable_floorplanning();
    auto& cluster_ctx = g_vpr_ctx.clustering();

    if (floorplanning_ctx.cluster_constraints.size() == 0 && constraints_file.empty()) {
        VTR_LOG("No constraints file case \n");
        floorplanning_ctx.cluster_constraints.resize(cluster_ctx.clb_nlist.blocks().size());

        //Assign an empty PartitionRegion to each cluster block
        for (auto blk_id : cluster_ctx.clb_nlist.blocks()) {
            PartitionRegion empty_pr;
            floorplanning_ctx.cluster_constraints[blk_id] = empty_pr;
        }
    } else if (floorplanning_ctx.cluster_constraints.size() == 0 && !constraints_file.empty()) {
        VTR_LOG("Constraints file case\n");
    	load_cluster_constraints();
    }
}

void load_cluster_constraints() {
    auto& floorplanning_ctx = g_vpr_ctx.mutable_floorplanning();
    auto& cluster_ctx = g_vpr_ctx.clustering();
    auto& atom_ctx = g_vpr_ctx.atom();

    VTR_LOG("In load_cluster_constraints \n");

    vtr::vector<ClusterBlockId, std::vector<AtomBlockId>> cluster_constrained_atoms;

    for (auto blk_id : cluster_ctx.clb_nlist.blocks()) {
    	cluster_constrained_atoms.push_back(std::vector<AtomBlockId>());
    }

    VTR_LOG("Initialized cluster_constrained_atoms \n");

    for (auto atom_blk_id : atom_ctx.nlist.blocks()) {
        PartitionId partid = floorplanning_ctx.constraints.get_atom_partition(atom_blk_id);

        if (partid != PartitionId::INVALID()) {
			ClusterBlockId clb_index = atom_ctx.lookup.atom_clb(atom_blk_id);
			cluster_constrained_atoms[clb_index].push_back(atom_blk_id);
        }
    }

    VTR_LOG("Filled in cluster_constrained_atoms \n");

    floorplanning_ctx.cluster_constraints.resize(cluster_ctx.clb_nlist.blocks().size());

    for (auto cluster_id : cluster_ctx.clb_nlist.blocks()) {
    	std::vector<AtomBlockId> atoms = cluster_constrained_atoms[cluster_id];
    	if (atoms.size() != 0) {
    		for (unsigned int i = 0; i < atoms.size(); i++) {
    			PartitionId partid = floorplanning_ctx.constraints.get_atom_partition(atoms[i]);
    			PartitionRegion pr = floorplanning_ctx.constraints.get_partition_pr(partid);

    			if (i == 0) {
    				floorplanning_ctx.cluster_constraints[cluster_id] = pr;
    			} else {
    				PartitionRegion intersect_pr = intersection(pr, floorplanning_ctx.cluster_constraints[cluster_id]);
    				if (intersect_pr.empty()) {
    					VTR_LOG_ERROR("Cluster block %zu has atoms with incompatible floorplan constraints.\n", size_t(cluster_id));
    				} else {
    					floorplanning_ctx.cluster_constraints[cluster_id] = intersect_pr;
    				}
    			}
    		}
    	} else {
            PartitionRegion empty_pr;
            floorplanning_ctx.cluster_constraints[cluster_id] = empty_pr;
    	}
    }

    VTR_LOG("Filled in cluster_constraints \n");

    for (ClusterBlockId clb_id : cluster_ctx.clb_nlist.blocks()) {
        std::vector<Region> reg = floorplanning_ctx.cluster_constraints[clb_id].get_partition_region();
        if (reg.size() != 0) {
            VTR_LOG("\nRegions in Cluster %zu:\n", size_t(clb_id));
            for (unsigned int i = 0; i < reg.size(); i++) {
            	vtr::Rect<int> rect = reg[i].get_region_rect();
            	VTR_LOG("\n xlow: %d, ylow: %d, xhigh: %d, yhigh: %d, subtile: %d\n", rect.xmin(), rect.ymin(), rect.xmax(), rect.ymax(), reg[i].get_sub_tile());
            }
        }
    }

}
