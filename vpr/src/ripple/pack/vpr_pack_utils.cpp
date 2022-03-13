#include "vpr_pack_utils.h"

enum e_block_pack_status check_chain_root_placement_feasibility(const t_pb_graph_node* pb_graph_node,
                                                                       const t_pack_molecule* molecule,
                                                                       const AtomBlockId blk_id) {
    enum e_block_pack_status block_pack_status = BLK_PASSED;
    auto& atom_ctx = g_vpr_ctx.atom();

    bool is_long_chain = molecule->chain_info->is_long_chain;

    const auto& chain_root_pins = molecule->pack_pattern->chain_root_pins;

    t_model_ports* root_port = chain_root_pins[0][0]->port->model_port;
    AtomNetId chain_net_id;
    auto port_id = atom_ctx.nlist.find_atom_port(blk_id, root_port);

    if (port_id) {
        chain_net_id = atom_ctx.nlist.port_net(port_id, chain_root_pins[0][0]->pin_number);
    }

    // if this block is part of a long chain or it is driven by a cluster
    // input pin we need to check the placement legality of this block
    // Depending on the logic synthesis even small chains that can fit within one
    // cluster might need to start at the top of the cluster as their input can be
    // driven by a global gnd or vdd. Therefore even if this is not a long chain
    // but its input pin is driven by a net, the placement legality is checked.
    if (is_long_chain || chain_net_id) {
        auto chain_id = molecule->chain_info->chain_id;
        // if this chain has a chain id assigned to it (implies is_long_chain too)
        if (chain_id != -1) {
            // the chosen primitive should be a valid starting point for the chain
            // long chains should only be placed at the top of the chain tieOff = 0
            if (pb_graph_node != chain_root_pins[chain_id][0]->parent_node) {
                block_pack_status = BLK_FAILED_FEASIBLE;
            }
            // the chain doesn't have an assigned chain_id yet
        } else {
            block_pack_status = BLK_FAILED_FEASIBLE;
            for (const auto& chain : chain_root_pins) {
                for (size_t tieOff = 0; tieOff < chain.size(); tieOff++) {
                    // check if this chosen primitive is one of the possible
                    // starting points for this chain.
                    if (pb_graph_node == chain[tieOff]->parent_node) {
                        // this location matches with the one of the dedicated chain
                        // input from outside logic block, therefore it is feasible
                        block_pack_status = BLK_PASSED;
                        break;
                    }
                    // long chains should only be placed at the top of the chain tieOff = 0
                    if (is_long_chain) break;
                }
            }
        }
    }

    return block_pack_status;
}

//did not touch floorplanning_ctx.cluster_constraints , write data to this->temp_cluster_pr
enum e_block_pack_status try_atom_cluster_floorplanning_check(const AtomBlockId blk_id,
                                                                 const int verbosity,
                                                                 PartitionRegion& temp_cluster_pr,
                                                                 bool& cluster_pr_needs_update) {
    auto& floorplanning_ctx = g_vpr_ctx.mutable_floorplanning();

    /*check if the atom can go in the cluster by checking if the atom and cluster have intersecting PartitionRegions*/

    //get partition that atom belongs to
    PartitionId partid;
    partid = floorplanning_ctx.constraints.get_atom_partition(blk_id);

    PartitionRegion atom_pr;
    //PartitionRegion cluster_pr;

    //if the atom does not belong to a partition, it can be put in the cluster
    //regardless of what the cluster's PartitionRegion is because it has no constraints
    if (partid == PartitionId::INVALID()) {
        if (verbosity > 3) {
            VTR_LOG("\t\t\t Intersect: Atom block %d has no floorplanning constraints, passed for cluster  \n", blk_id);
        }
        cluster_pr_needs_update = false;
        return BLK_PASSED;
    } else {
        //get pr of that partition
        atom_pr = floorplanning_ctx.constraints.get_partition_pr(partid);

        //intersect it with the pr of the current cluster
        //cluster_pr = floorplanning_ctx.cluster_constraints[clb_index];

        if (temp_cluster_pr.empty() == true) {
            temp_cluster_pr = atom_pr;
            cluster_pr_needs_update = true;
            if (verbosity > 3) {
                VTR_LOG("\t\t\t Intersect: Atom block %d has floorplanning constraints, passed cluster  which has empty PR\n", blk_id);
            }
            return BLK_PASSED;
        } else {
            //update cluster_pr with the intersection of the cluster's PartitionRegion
            //and the atom's PartitionRegion
            update_cluster_part_reg(temp_cluster_pr, atom_pr);
        }

        if (temp_cluster_pr.empty() == true) {
            if (verbosity > 3) {
                VTR_LOG("\t\t\t Intersect: Atom block %d failed floorplanning check for cluster  \n", blk_id);
            }
            cluster_pr_needs_update = false;
            return BLK_FAILED_FLOORPLANNING;
        } else {
            //update the cluster's PartitionRegion with the intersecting PartitionRegion
            //temp_cluster_pr = cluster_pr;
            cluster_pr_needs_update = true;
            if (verbosity > 3) {
                VTR_LOG("\t\t\t Intersect: Atom block %d passed cluster , cluster PR was updated with intersection result \n", blk_id);
            }
            return BLK_PASSED;
        }
    }
}

bool try_primitive_memory_sibling_feasible(const AtomBlockId blk_id, const t_pb_type* cur_pb_type, const AtomBlockId sibling_blk_id) {
    /* Check that the two atom blocks blk_id and sibling_blk_id (which should both be memory slices)
     * are feasible, in the sence that they have precicely the same net connections (with the
     * exception of nets in data port classes).
     *
     * Note that this routine does not check pin feasibility against the cur_pb_type; so
     * primitive_type_feasible() should also be called on blk_id before concluding it is feasible.
     */
    auto& atom_ctx = g_vpr_ctx.atom();
    VTR_ASSERT(cur_pb_type->class_type == MEMORY_CLASS);

    //First, identify the 'data' ports by looking at the cur_pb_type
    std::unordered_set<t_model_ports*> data_ports;
    for (int iport = 0; iport < cur_pb_type->num_ports; ++iport) {
        const char* port_class = cur_pb_type->ports[iport].port_class;
        if (port_class && strstr(port_class, "data") == port_class) {
            //The port_class starts with "data", so it is a data port

            //Record the port
            data_ports.insert(cur_pb_type->ports[iport].model_port);
        }
    }

    //Now verify that all nets (except those connected to data ports) are equivalent
    //between blk_id and sibling_blk_id

    //Since the atom netlist stores only in-use ports, we iterate over the model to ensure
    //all ports are compared
    const t_model* model = cur_pb_type->model;
    for (t_model_ports* port : {model->inputs, model->outputs}) {
        for (; port; port = port->next) {
            if (data_ports.count(port)) {
                //Don't check data ports
                continue;
            }

            //Note: VPR doesn't support multi-driven nets, so all outputs
            //should be data ports, otherwise the siblings will both be
            //driving the output net

            //Get the ports from each primitive
            auto blk_port_id = atom_ctx.nlist.find_atom_port(blk_id, port);
            auto sib_port_id = atom_ctx.nlist.find_atom_port(sibling_blk_id, port);

            //Check that all nets (including unconnected nets) match
            for (int ipin = 0; ipin < port->size; ++ipin) {
                //The nets are initialized as invalid (i.e. disconnected)
                AtomNetId blk_net_id;
                AtomNetId sib_net_id;

                //We can get the actual net provided the port exists
                //
                //Note that if the port did not exist, the net is left
                //as invalid/disconneced
                if (blk_port_id) {
                    blk_net_id = atom_ctx.nlist.port_net(blk_port_id, ipin);
                }
                if (sib_port_id) {
                    sib_net_id = atom_ctx.nlist.port_net(sib_port_id, ipin);
                }

                //The sibling and block must have the same (possibly disconnected)
                //net on this pin
                if (blk_net_id != sib_net_id) {
                    //Nets do not match, not feasible
                    return false;
                }
            }
        }
    }

    return true;
}

bool try_primitive_feasible(const AtomBlockId blk_id, t_pb* cur_pb) {
    const t_pb_type* cur_pb_type = cur_pb->pb_graph_node->pb_type;

    VTR_ASSERT(cur_pb_type->num_modes == 0); /* primitive */

    auto& atom_ctx = g_vpr_ctx.atom();
    AtomBlockId cur_pb_blk_id = atom_ctx.lookup.pb_atom(cur_pb);
    if (cur_pb_blk_id && cur_pb_blk_id != blk_id) {
        /* This pb already has a different logical block */
        return false;
    }

    if (cur_pb_type->class_type == MEMORY_CLASS) {
        /* Memory class has additional feasibility requirements:
         *   - all siblings must share all nets, including open nets, with the exception of data nets */

        /* find sibling if one exists */
        AtomBlockId sibling_memory_blk_id = find_memory_sibling(cur_pb);

        if (sibling_memory_blk_id) {
            //There is a sibling, see if the current block is feasible with it
            bool sibling_feasible = try_primitive_memory_sibling_feasible(blk_id, cur_pb_type, sibling_memory_blk_id);
            if (!sibling_feasible) {
                return false;
            }
        }
    }

    //Generic feasibility check
    return primitive_type_feasible(blk_id, cur_pb_type);
}

enum e_block_pack_status try_try_place_atom_block_rec(const t_pb_graph_node* pb_graph_node,
                                                         const AtomBlockId blk_id,
                                                         t_pb* cb,
                                                         t_pb** parent,
                                                         const int max_models,
                                                         const ClusterBlockId clb_index,
                                                         const t_cluster_placement_stats* cluster_placement_stats_ptr,
                                                         const t_pack_molecule* molecule,
                                                         t_lb_router_data* router_data,
                                                         int verbosity,
                                                         const int feasible_block_array_size) {
    int i, j;
    bool is_primitive;
    enum e_block_pack_status block_pack_status;

    t_pb* my_parent;
    t_pb *pb, *parent_pb;
    const t_pb_type* pb_type;

    auto& atom_ctx = g_vpr_ctx.mutable_atom();

    my_parent = nullptr;

    block_pack_status = BLK_PASSED;

    /* Discover parent */
    if (pb_graph_node->parent_pb_graph_node != cb->pb_graph_node) {
        block_pack_status = try_try_place_atom_block_rec(pb_graph_node->parent_pb_graph_node, blk_id, cb,
                                                     &my_parent, max_models,clb_index,
                                                     cluster_placement_stats_ptr, molecule, router_data,
                                                     verbosity, feasible_block_array_size);
        parent_pb = my_parent;
    } else {
        parent_pb = cb;
    }

    /* Create siblings if siblings are not allocated */
    if (parent_pb->child_pbs == nullptr) {
        atom_ctx.lookup.set_atom_pb(AtomBlockId::INVALID(), parent_pb);

        VTR_ASSERT(parent_pb->name == nullptr);
        parent_pb->name = vtr::strdup(atom_ctx.nlist.block_name(blk_id).c_str());
        parent_pb->mode = pb_graph_node->pb_type->parent_mode->index;
        set_reset_pb_modes(router_data, parent_pb, true);
        const t_mode* mode = &parent_pb->pb_graph_node->pb_type->modes[parent_pb->mode];
        parent_pb->child_pbs = new t_pb*[mode->num_pb_type_children];

        for (i = 0; i < mode->num_pb_type_children; i++) {
            parent_pb->child_pbs[i] = new t_pb[mode->pb_type_children[i].num_pb];

            for (j = 0; j < mode->pb_type_children[i].num_pb; j++) {
                parent_pb->child_pbs[i][j].parent_pb = parent_pb;

                atom_ctx.lookup.set_atom_pb(AtomBlockId::INVALID(), &parent_pb->child_pbs[i][j]);

                parent_pb->child_pbs[i][j].pb_graph_node = &(parent_pb->pb_graph_node->child_pb_graph_nodes[parent_pb->mode][i][j]);
            }
        }
    } else {
        VTR_ASSERT(parent_pb->mode == pb_graph_node->pb_type->parent_mode->index);
    }

    const t_mode* mode = &parent_pb->pb_graph_node->pb_type->modes[parent_pb->mode];
    for (i = 0; i < mode->num_pb_type_children; i++) {
        if (pb_graph_node->pb_type == &mode->pb_type_children[i]) {
            break;
        }
    }
    VTR_ASSERT(i < mode->num_pb_type_children);
    pb = &parent_pb->child_pbs[i][pb_graph_node->placement_index];
    *parent = pb; /* this pb is parent of it's child that called this function */
    VTR_ASSERT(pb->pb_graph_node == pb_graph_node);
    if (pb->pb_stats == nullptr) {
        alloc_and_load_pb_stats(pb, feasible_block_array_size);
    }
    pb_type = pb_graph_node->pb_type;

    /* Any pb_type under an mode, which is disabled for packing, should not be considerd for mapping 
     * Early exit to flag failure
     */
    if (true == pb_type->parent_mode->disable_packing) {
        return BLK_FAILED_FEASIBLE;
    }

    is_primitive = (pb_type->num_modes == 0);

    if (is_primitive) {
        VTR_ASSERT(!atom_ctx.lookup.pb_atom(pb)
                   //&& atom_ctx.lookup.atom_pb(blk_id) == nullptr
                   //&& atom_ctx.lookup.atom_clb(blk_id) == clb_index
                   );
        /* try pack to location */
        VTR_ASSERT(pb->name == nullptr);
        if(blk_id==AtomBlockId(676)){
            cout<<"find0"<<endl;
        }
        pb->name = vtr::strdup(atom_ctx.nlist.block_name(blk_id).c_str());

        //Update the atom netlist mappings
        atom_ctx.lookup.set_atom_clb(blk_id, clb_index);
        //atom_ctx.lookup.set_atom_pb(blk_id, nullptr);
        atom_ctx.lookup.set_atom_pb(blk_id, pb);
        //atom_to_pb[blk_id]=pb;

        add_atom_as_target(router_data, blk_id);
        if (!try_primitive_feasible(blk_id, pb)) {
            /* failed location feasibility check, revert pack */
            block_pack_status = BLK_FAILED_FEASIBLE;
        }

        // if this block passed and is part of a chained molecule
        if (block_pack_status == BLK_PASSED && molecule->is_chain()) {
            auto molecule_root_block = molecule->atom_block_ids[molecule->root];
            // if this is the root block of the chain molecule check its placmeent feasibility
            if (blk_id == molecule_root_block) {
                block_pack_status = check_chain_root_placement_feasibility(pb_graph_node, molecule, blk_id);
            }
        }

        VTR_LOGV(verbosity > 4 && block_pack_status == BLK_PASSED,
                 "\t\t\tPlaced atom '%s' (%s) at %s\n",
                 atom_ctx.nlist.block_name(blk_id).c_str(),
                 atom_ctx.nlist.block_model(blk_id)->name,
                 pb->hierarchical_type_name().c_str());
    }

    if (block_pack_status != BLK_PASSED) {
        free(pb->name);
        pb->name = nullptr;
    }

    return block_pack_status;
}

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
                                                  PartitionRegion& temp_cluster_pr
                                                  //,map<AtomBlockId , t_pb*>& atom_to_pb
                                                  ) {
    int molecule_size, failed_location;
    int i;
    enum e_block_pack_status block_pack_status;
    t_pb* parent;
    t_pb* cur_pb;

    auto& atom_ctx = g_vpr_ctx.atom();
    auto& floorplanning_ctx = g_vpr_ctx.mutable_floorplanning();

    parent = nullptr;

    block_pack_status = BLK_STATUS_UNDEFINED;

    molecule_size = get_array_size_of_molecule(molecule);
    failed_location = 0;

    // if(molecule->atom_block_ids[0]==AtomBlockId(680)){
    //     cout<<"find"<<endl;
    // }

    if (cluster_placement_stats_ptr->has_long_chain && molecule->is_chain() && molecule->chain_info->is_long_chain) {
        return BLK_FAILED_FEASIBLE;
    }

    bool cluster_pr_needs_update = false;
    bool cluster_pr_update_check = false;

    for (int i_mol = 0; i_mol < molecule_size; i_mol++) {
        if (molecule->atom_block_ids[i_mol]) {
            block_pack_status = try_atom_cluster_floorplanning_check(molecule->atom_block_ids[i_mol],
                                                                 verbosity,
                                                                 temp_cluster_pr,
                                                                 cluster_pr_needs_update);
            if (block_pack_status == BLK_FAILED_FLOORPLANNING) {
                return block_pack_status;
            }
            if (cluster_pr_needs_update == true) {
                cluster_pr_update_check = true;
            }
        }
    }

    block_pack_status = BLK_STATUS_UNDEFINED;

    while (block_pack_status != BLK_PASSED) {
        if (get_next_primitive_list(cluster_placement_stats_ptr, molecule,
                                    primitives_list)) {
            block_pack_status = BLK_PASSED;

            for (i = 0; i < molecule_size && block_pack_status == BLK_PASSED; i++) {
                VTR_ASSERT((primitives_list[i] == nullptr) == (!molecule->atom_block_ids[i]));
                failed_location = i + 1;
                if (molecule->atom_block_ids[i]) {
                    block_pack_status = try_try_place_atom_block_rec(primitives_list[i],
                                                                 molecule->atom_block_ids[i], pb, &parent,
                                                                 max_models,clb_index,
                                                                 cluster_placement_stats_ptr, molecule, router_data,
                                                                 verbosity, feasible_block_array_size);
                }
            }

            if (enable_pin_feasibility_filter && block_pack_status == BLK_PASSED) {
                reset_lookahead_pins_used(pb);
                try_update_lookahead_pins_used(pb);
                if (!check_lookahead_pins_used(pb, max_external_pin_util)) {
                    VTR_LOGV(verbosity > 4, "\t\t\tFAILED Pin Feasibility Filter\n");
                    block_pack_status = BLK_FAILED_FEASIBLE;
                }
            }
            if (block_pack_status == BLK_PASSED) {
                t_mode_selection_status mode_status;
                bool is_routed = false;
                
                do {
                    reset_intra_lb_route(router_data);
                    is_routed = try_intra_lb_route(router_data, verbosity, &mode_status);
                } while (mode_status.is_mode_issue());
                

                if (is_routed == false) {
                    /* Cannot pack */
                    VTR_LOGV(verbosity > 4, "\t\t\tFAILED Detailed Routing Legality\n");
                    block_pack_status = BLK_FAILED_ROUTE;
                } else {
                    /* Pack successful, commit
                     * TODO: SW Engineering note - may want to update cluster stats here too instead of doing it outside
                     */
                    VTR_ASSERT(block_pack_status == BLK_PASSED);
                    if (molecule->is_chain()) {
                        /* Chained molecules often take up lots of area and are important,
                         * if a chain is packed in, want to rename logic block to match chain name */
                        AtomBlockId chain_root_blk_id = molecule->atom_block_ids[molecule->pack_pattern->root_block->block_id];
                        cur_pb = atom_ctx.lookup.atom_pb(chain_root_blk_id)->parent_pb;
                        while (cur_pb != nullptr) {
                            free(cur_pb->name);
                            cur_pb->name = vtr::strdup(atom_ctx.nlist.block_name(chain_root_blk_id).c_str());
                            cur_pb = cur_pb->parent_pb;
                        }
                        // if this molecule is part of a chain, mark the cluster as having a long chain
                        // molecule. Also check if it's the first molecule in the chain to be packed.
                        // If so, update the chain id for this chain of molecules to make sure all
                        // molecules will be packed to the same chain id and can reach each other using
                        // the chain direct links between clusters
                        if (molecule->chain_info->is_long_chain) {
                            cluster_placement_stats_ptr->has_long_chain = true;
                            if (molecule->chain_info->chain_id == -1) {
                                update_molecule_chain_info(molecule, primitives_list[molecule->root]);
                            }
                        }
                    }

                    //update cluster PartitionRegion if atom with floorplanning constraints was added
                    // if (cluster_pr_update_check) {
                    //     floorplanning_ctx.cluster_constraints[clb_index] = temp_cluster_pr;
                    //     if (verbosity > 2) {
                    //         VTR_LOG("\nUpdated PartitionRegion of cluster %d\n", clb_index);
                    //     }
                    // }

                    for (i = 0; i < molecule_size; i++) {
                        if (molecule->atom_block_ids[i]) {
                            /* invalidate all molecules that share atom block with current molecule */

                            auto rng = atom_molecules.equal_range(molecule->atom_block_ids[i]);
                            for (const auto& kv : vtr::make_range(rng.first, rng.second)) {
                                t_pack_molecule* cur_molecule = kv.second;
                                cur_molecule->valid = false;
                            }

                            commit_primitive(cluster_placement_stats_ptr, primitives_list[i]);
                        }
                    }
                }
            }

            if (block_pack_status != BLK_PASSED) {
                for (i = 0; i < failed_location; i++) {
                    if (molecule->atom_block_ids[i]) {
                        remove_atom_from_target(router_data, molecule->atom_block_ids[i]);
                    }
                }
                for (i = 0; i < failed_location; i++) {
                    if (molecule->atom_block_ids[i]) {
                        revert_place_atom_block(molecule->atom_block_ids[i], router_data, atom_molecules);
                    }
                }

                /* Packing failed, but a part of the pb tree is still allocated and pbs have their modes set.
                 * Before trying to pack next molecule the unused pbs need to be freed and, the most important,
                 * their modes reset. This task is performed by the cleanup_pb() function below. */
                cleanup_pb(pb);

            } else {
                VTR_LOGV(verbosity > 3, "\t\tPASSED pack molecule\n");
            }
        } else {
            VTR_LOGV(verbosity > 3, "\t\tFAILED No candidate primitives available\n");
            block_pack_status = BLK_FAILED_FEASIBLE;
            break; /* no more candidate primitives available, this molecule will not pack, return fail */
        }
    }
    return block_pack_status;
}
