#include "vpr_clb.h"
//for debug
// #include "DebugNew.h"
void VPR_CLB::read_cluster_placement_stats_from_clb( int type_index,t_cluster_placement_stats* cluster_placement_stats_ptr){
    int t , m;
    t_cluster_placement_primitive *cur , *prev;
    ripple_primitive_stats ripple_primitive_stats;
    //t_cluster_placement_stats* cluster_placement_stats_ptr;
    for (size_t i = 0; i < this->block_types.size(); i++) {
        //auto &type=clb->block_types[i];
        //cluster_placement_stats_ptr=&cluster_placement_stats[type_index];
        reset_cluster_placement_stats(cluster_placement_stats_ptr);
        if(this->type_index_to_cluster_placement_stats.find(type_index)==this->type_index_to_cluster_placement_stats.end()){
            //initiate type_index_to_cluster_placement_stats[type->index] in clb
            for (t = 0; t < cluster_placement_stats_ptr->num_pb_types; t++) {
                if (cluster_placement_stats_ptr->valid_primitives[t]->next_primitive == nullptr) {
                    continue;
                }
                cur = cluster_placement_stats_ptr->valid_primitives[t]->next_primitive;
                while(cur){
                    ripple_primitive_stats.base_cost=cur->base_cost;
                    ripple_primitive_stats.incremental_cost=cur->incremental_cost;
                    ripple_primitive_stats.valid=cur->valid;
                    this->type_index_to_cluster_placement_stats[type_index][cur->pb_graph_node->pb_type][cur->pb_graph_node]=ripple_primitive_stats;
                    cur = cur->next_primitive;
                }
            }
        }else{
            //write type_index_to_cluster_placement_stats[type->index] to cluster_placement_stats
            for (t = 0; t < cluster_placement_stats_ptr->num_pb_types; t++) {
                if (cluster_placement_stats_ptr->valid_primitives[t]->next_primitive == nullptr) {
                    continue;
                }
                prev = cluster_placement_stats_ptr->valid_primitives[t];
                cur = cluster_placement_stats_ptr->valid_primitives[t]->next_primitive;
                while(cur){
                    ripple_primitive_stats=this->type_index_to_cluster_placement_stats[type_index][cur->pb_graph_node->pb_type][cur->pb_graph_node];
                    cur->base_cost=ripple_primitive_stats.base_cost;
                    cur->incremental_cost=ripple_primitive_stats.incremental_cost;
                    cur->valid=ripple_primitive_stats.valid;
                    if(cur->valid == false){
                        cur->valid = 0;
                        prev->next_primitive = cur->next_primitive;
                        cur->next_primitive = cluster_placement_stats_ptr->invalid;
                        cluster_placement_stats_ptr->invalid = cur;
                        cur = prev->next_primitive;
                    }else{
                        prev = cur;
                        cur = cur->next_primitive;
                    }
                }
            }
        }
    }
}

void VPR_CLB::write_cluster_placement_stats_to_clb(t_cluster_placement_stats* cluster_placement_stats){
    //
    int i;
    t_cluster_placement_stats* cluster_placement_stats_ptr;
    t_cluster_placement_primitive *cur;

    for(auto &pair:type_index_to_cluster_placement_stats){
        cluster_placement_stats_ptr=&cluster_placement_stats[pair.first];
        flush_intermediate_queues(cluster_placement_stats_ptr);
        //VTR_ASSERT(!cluster_placement_stats_ptr->tried && !cluster_placement_stats_ptr->in_flight);
        cur=cluster_placement_stats_ptr->invalid;
        while(cur){
            auto &ripple_primitive=pair.second[cur->pb_graph_node->pb_type][cur->pb_graph_node];
            ripple_primitive.base_cost=cur->base_cost;
            ripple_primitive.incremental_cost=cur->incremental_cost;
            //VTR_ASSERT(!cur->valid);
            ripple_primitive.valid=cur->valid;
            cur=cur->next_primitive;
        }
        for (i = 0; i < cluster_placement_stats_ptr->num_pb_types; i++) {
            cur=cluster_placement_stats_ptr->valid_primitives[i]->next_primitive;
            while(cur){
                auto &ripple_primitive=pair.second[cur->pb_graph_node->pb_type][cur->pb_graph_node];
                ripple_primitive.base_cost=cur->base_cost;
                ripple_primitive.incremental_cost=cur->incremental_cost;
                //VTR_ASSERT(cur->valid);
                ripple_primitive.valid=cur->valid;
                cur=cur->next_primitive;
            }
        }
    }
}

bool VPR_CLB::try_start_new_cluster(const Group& group){
    bool createblock = this->index == ClusterBlockId();
    ClusterBlockId clb_index=createblock?(ClusterBlockId)packdata->num_clb : this->index;
    t_pack_molecule* molecule=group.vpr_molecule;
    std::vector<t_lb_type_rr_node>* lb_type_rr_graphs=database.vpr_setup->PackerRRGraph;

    // PartitionRegion empty_pr;
    // floorplanning_ctx.cluster_constraints.push_back(empty_pr);

    auto& atom_ctx = g_vpr_ctx.atom();
    auto& device_ctx = g_vpr_ctx.mutable_device();
    t_packer_opts &packer_opts = database.vpr_setup->PackerOpts;
    auto& clb_nlist = g_vpr_ctx.mutable_clustering().clb_nlist;

    AtomBlockId root_atom = molecule->atom_block_ids[molecule->root];
    const std::string& root_atom_name = atom_ctx.nlist.block_name(root_atom);
    const t_model* root_model = atom_ctx.nlist.block_model(root_atom);
    
    auto itr = packdata->primitive_candidate_block_types.find(root_model);
    VTR_ASSERT(itr != packdata->primitive_candidate_block_types.end());
    this->block_types = itr->second;

    if (database.balance_block_type_util) {
        std::stable_sort(this->block_types.begin(), this->block_types.end(),
                         [&](t_logical_block_type_ptr lhs, t_logical_block_type_ptr rhs) {
                             int lhs_num_instances = 0;
                             int rhs_num_instances = 0;
                             for (auto type : lhs->equivalent_tiles)
                                 lhs_num_instances += device_ctx.grid.num_instances(type);
                             for (auto type : rhs->equivalent_tiles)
                                 rhs_num_instances += device_ctx.grid.num_instances(type);

                             float lhs_util = vtr::safe_ratio<float>(packdata->num_used_type_instances[lhs], lhs_num_instances);
                             float rhs_util = vtr::safe_ratio<float>(packdata->num_used_type_instances[rhs], rhs_num_instances);
                             return lhs_util < rhs_util;
                         });
    }
    
    size_t i;

    bool success=false;
    
    t_cluster_placement_stats* cluster_placement_stats_ptr;

    for (i = 0; i < this->block_types.size(); i++) {
        auto type =this->block_types[i];
        this->pb = new t_pb;
        pb->pb_graph_node = type->pb_graph_head;
        alloc_and_load_pb_stats(pb, packer_opts.feasible_block_array_size);
        pb->parent_pb = nullptr;
        this->router_data=alloc_and_load_router_data(&lb_type_rr_graphs[type->index], type);
        cluster_placement_stats_ptr=&packdata->cluster_placement_stats[type->index];
        e_block_pack_status pack_result = BLK_STATUS_UNDEFINED;
        read_cluster_placement_stats_from_clb(type->index,cluster_placement_stats_ptr);
        for (int j = 0; j < type->pb_graph_head->pb_type->num_modes && !success; j++) {
            pb->mode = j;
            reset_cluster_placement_stats(cluster_placement_stats_ptr);
            set_mode_cluster_placement_stats(pb->pb_graph_node, j);

            pack_result = try_try_pack_molecule(cluster_placement_stats_ptr,
                                database.atom_molecules,
                                molecule, packdata->primitives_list, pb,
                                clb_index,
                                database.num_models,
                                this->router_data,
                                packer_opts.pack_verbosity,
                                packer_opts.enable_pin_feasibility_filter,
                                packer_opts.feasible_block_array_size,
                                t_ext_pin_util(1., 1.),
                                this->temp_cluster_pr);
            success = (pack_result == BLK_PASSED);
        }
        if (success) {
        //Once clustering succeeds, add it to the clb netlist
            if (pb->name != nullptr) {
                free(pb->name);
            }
            pb->name = vtr::strdup(root_atom_name.c_str());

            if(createblock){
                clb_index =clb_nlist.create_block(root_atom_name.c_str(), pb, type);
                database.clusterblockid_site[clb_index]=this->site;
                assert(!this->index);
                this->index=clb_index;
                packdata->num_clb++;
            }

            for(auto &pair:this->type_index_to_cluster_placement_stats){
                if(pair.first != type->index){
                    this->type_index_to_cluster_placement_stats.erase(pair.first);
                }
            }
            break;
        } else {
        //Free failed clustering and try again
            free_router_data(this->router_data);
            free_pb(pb);
            delete pb;
            this->router_data = nullptr;
        }
    }

    if(!success) {
        printlog(LOG_ERROR, "vpr_start_new_cluster failed!!");
        std::abort();
    }
    //Successfully create cluster
    typeindex=i;
    packdata->num_used_type_instances[block_types[i]]++;

    /* Expand FPGA size if needed */
    // Check used type instances against the possible equivalent physical locations
    unsigned int num_instances = 0;
    for (auto equivalent_tile : block_types[i]->equivalent_tiles) {
        num_instances += device_ctx.grid.num_instances(equivalent_tile);
    }

    if (packdata->num_used_type_instances[block_types[i]] > num_instances) {
        //to be un//ed
        //device_ctx.grid = create_device_grid(device_layout_name, arch->grid_layouts, num_used_type_instances, target_device_utilization);
        printlog(LOG_INFO, "device to be expanded");
    }
    return success;
}

bool VPR_CLB::vpr_start_new_cluster(const Group& group){
    ClusterBlockId clb_index=(ClusterBlockId)packdata->num_clb;
    t_pack_molecule* molecule=group.vpr_molecule;
    t_packer_opts &packer_opts = database.vpr_setup->PackerOpts;
    std::vector<t_lb_type_rr_node>* lb_type_rr_graphs=database.vpr_setup->PackerRRGraph;
    auto& atom_ctx = g_vpr_ctx.atom();
    auto& device_ctx = g_vpr_ctx.mutable_device();
    auto& floorplanning_ctx = g_vpr_ctx.mutable_floorplanning();
    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
    ClusteredNetlist* clb_nlist = &cluster_ctx.clb_nlist;

    /*Cluster's PartitionRegion is empty initially, meaning it has no floorplanning constraints*/
    PartitionRegion empty_pr;
    floorplanning_ctx.cluster_constraints.push_back(empty_pr);

    /* Allocate a dummy initial cluster and load a atom block as a seed and check if it is legal */
    AtomBlockId root_atom = molecule->atom_block_ids[molecule->root];
    const std::string& root_atom_name = atom_ctx.nlist.block_name(root_atom);
    const t_model* root_model = atom_ctx.nlist.block_model(root_atom);
    
    auto itr = packdata->primitive_candidate_block_types.find(root_model);
    VTR_ASSERT(itr != packdata->primitive_candidate_block_types.end());
    //std::vector<t_logical_block_type_ptr> candidate_types = itr->second;
    this->block_types = itr->second;

    if (database.balance_block_type_util) {
        //We sort the candidate types in ascending order by their current utilization.
        //This means that the packer will prefer to use types with lower utilization.
        //This is a naive approach to try balancing utilization when multiple types can
        //support the same primitive(s).
        std::stable_sort(this->block_types.begin(), this->block_types.end(),
                         [&](t_logical_block_type_ptr lhs, t_logical_block_type_ptr rhs) {
                             int lhs_num_instances = 0;
                             int rhs_num_instances = 0;
                             // Count number of instances for each type
                             for (auto type : lhs->equivalent_tiles)
                                 lhs_num_instances += device_ctx.grid.num_instances(type);
                             for (auto type : rhs->equivalent_tiles)
                                 rhs_num_instances += device_ctx.grid.num_instances(type);

                             float lhs_util = vtr::safe_ratio<float>(packdata->num_used_type_instances[lhs], lhs_num_instances);
                             float rhs_util = vtr::safe_ratio<float>(packdata->num_used_type_instances[rhs], rhs_num_instances);
                             //Lower util first
                             return lhs_util < rhs_util;
                         });
    }
    
    
    //t_pb* &pb = pb;

    //PartitionRegion temp_cluster_pr;
    
    bool success=false;
    
    t_cluster_placement_stats* cluster_placement_stats_ptr;
    
    //ripple_primitives_stats ripple_cur;
    // cluster_placement_stats_for_ripple ripple_placement_stats;
    // placement_primitives_for_ripple ripple_placement_primitives;
    for (size_t i = 0; i < this->block_types.size(); i++) {
        auto type =this->block_types[i];
        this->pb = new t_pb;
        pb->pb_graph_node = type->pb_graph_head;
        alloc_and_load_pb_stats(pb, packer_opts.feasible_block_array_size);
        pb->parent_pb = nullptr;
        this->router_data=alloc_and_load_router_data(&lb_type_rr_graphs[type->index], type);
        cluster_placement_stats_ptr=&packdata->cluster_placement_stats[type->index];
        e_block_pack_status pack_result = BLK_STATUS_UNDEFINED;
        read_cluster_placement_stats_from_clb(type->index,cluster_placement_stats_ptr);
        for (int j = 0; j < type->pb_graph_head->pb_type->num_modes && !success; j++) {
            pb->mode = j;
            //**********************************
            // if(clb->type_index_to_cluster_placement_stats.find(type->index)==clb->type_index_to_cluster_placement_stats.end()){
            //     clb->type_index_to_cluster_placement_stats[type->index]= new t_cluster_placement_stats;
            //     *(clb->type_index_to_cluster_placement_stats[type->index])=cluster_placement_stats[type->index];
            // }
            
            //reset_cluster_placement_stats(clb->type_index_to_cluster_placement_stats[type->index]);
            //clb->cluster_placement_stats=alloc_and_load_cluster_placement_stats();
            reset_cluster_placement_stats(cluster_placement_stats_ptr);
            set_mode_cluster_placement_stats(pb->pb_graph_node, j);

            //*********************************************************
            pack_result = try_pack_molecule(cluster_placement_stats_ptr,
                                database.atom_molecules,
                                molecule, packdata->primitives_list, pb,
                                database.num_models,0, clb_index,
                                1,//detailed_routing_stage, set to 1 for now
                                this->router_data,
                                packer_opts.pack_verbosity,
                                packer_opts.enable_pin_feasibility_filter,
                                packer_opts.feasible_block_array_size,
                                t_ext_pin_util(1., 1.),
                                this->temp_cluster_pr);
            success = (pack_result == BLK_PASSED);
        }
        if (success) {
        //Once clustering succeeds, add it to the clb netlist
            if (pb->name != nullptr) {
                free(pb->name);
            }
            pb->name = vtr::strdup(root_atom_name.c_str());
            clb_index =clb_nlist->create_block(root_atom_name.c_str(), pb, type);
            database.clusterblockid_site[clb_index]=this->site;
            for(auto &pair:this->type_index_to_cluster_placement_stats){
                if(pair.first != type->index){
                    this->type_index_to_cluster_placement_stats.erase(pair.first);
                }
            }
            break;
        } else {
        //Free failed clustering and try again
            free_router_data(this->router_data);
            free_pb(pb);
            delete pb;
            this->router_data = nullptr;
        }
    }

    if(!success) {
        printlog(LOG_ERROR, "vpr_start_new_cluster failed!!");
        std::abort();
    }
    //Successfully create cluster
    auto block_type = clb_nlist->block_type(clb_index);
    packdata->num_used_type_instances[block_type]++;

    /* Expand FPGA size if needed */
    // Check used type instances against the possible equivalent physical locations
    unsigned int num_instances = 0;
    for (auto equivalent_tile : block_type->equivalent_tiles) {
        num_instances += device_ctx.grid.num_instances(equivalent_tile);
    }

    if (packdata->num_used_type_instances[block_type] > num_instances) {
        //to be un//ed
        //device_ctx.grid = create_device_grid(device_layout_name, arch->grid_layouts, num_used_type_instances, target_device_utilization);
        printlog(LOG_INFO, "device to be expanded");
    }
    return success;
}

bool VPR_CLB::TryAddInsts(const Group& group ){//for dp init and dp duoyu cluster
    bool success = false;
    t_ext_pin_util target_ext_pin_util;
    enum e_block_pack_status block_pack_status;
    t_cluster_placement_stats* cur_cluster_placement_stats_ptr;

    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
    t_packer_opts &packer_opts = database.vpr_setup->PackerOpts;

    if(!this->CLBIsEmpty){
        if(try_start_new_cluster(group)){
            this->CLBIsEmpty=true;
            success=true;
        }
    }else{
        int type_index=block_types[typeindex]->index;
        cur_cluster_placement_stats_ptr = &packdata->cluster_placement_stats[type_index];
        target_ext_pin_util = database.target_external_pin_util.get_pin_util(block_types[typeindex]->name);
        read_cluster_placement_stats_from_clb(type_index,cur_cluster_placement_stats_ptr);
        block_pack_status = try_try_pack_molecule(cur_cluster_placement_stats_ptr,
                                        database.atom_molecules,
                                        group.vpr_molecule,
                                        packdata->primitives_list,
                                        pb,
                                        this->index,
                                        database.num_models,
                                        this->router_data,
                                        packer_opts.pack_verbosity,
                                        packer_opts.enable_pin_feasibility_filter,
                                        packer_opts.feasible_block_array_size,
                                        target_ext_pin_util,
                                        this->temp_cluster_pr);
        success = block_pack_status == BLK_PASSED;
    }
    
    if(success){
        write_cluster_placement_stats_to_clb(packdata->cluster_placement_stats);
        
        int high_fanout_threshold = database.high_fanout_thresholds.get_threshold(block_types[typeindex]->name);
                    
        update_cluster_stats(group.vpr_molecule, this->index,
            database.is_clock, //Set of clock nets
            database.is_clock, //Set of global nets (currently all clocks)
            packer_opts.global_clocks,
            packer_opts.alpha, packer_opts.beta,
            packer_opts.timing_driven, packer_opts.connection_driven,
            high_fanout_threshold,
            *packdata->timing_info);

        //for ripple
        //Group* group_ptr = &group;
        groups.push_back(&group);
        return true;
    }

    return false;
}

bool VPR_CLB::AddInsts(const Group& group){
    bool success = false;
    t_ext_pin_util target_ext_pin_util;
    enum e_block_pack_status block_pack_status;
    t_cluster_placement_stats* cur_cluster_placement_stats_ptr;
    
    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
    t_packer_opts &packer_opts = database.vpr_setup->PackerOpts;
    
    if(!this->CLBIsEmpty){
        if(vpr_start_new_cluster(group)){
            this->CLBIsEmpty=true;
            this->index=(ClusterBlockId)packdata->num_clb;
            packdata->num_clb++;
            success=true;
        }
    }
    else{
        int type_index;
        type_index=cluster_ctx.clb_nlist.block_type(this->index)->index;
        cur_cluster_placement_stats_ptr = &packdata->cluster_placement_stats[type_index];
        target_ext_pin_util = database.target_external_pin_util.get_pin_util(cluster_ctx.clb_nlist.block_type(this->index)->name);
        read_cluster_placement_stats_from_clb(type_index,cur_cluster_placement_stats_ptr);
        block_pack_status = try_pack_molecule(cur_cluster_placement_stats_ptr,
                                        database.atom_molecules,
                                        group.vpr_molecule,
                                        packdata->primitives_list,
                                        pb,
                                        database.num_models,
                                        0,
                                        this->index,
                                        1,//detailed_routing_stage set to 1
                                        this->router_data,
                                        packer_opts.pack_verbosity,
                                        packer_opts.enable_pin_feasibility_filter,
                                        packer_opts.feasible_block_array_size,
                                        target_ext_pin_util,
                                        this->temp_cluster_pr);
        success = block_pack_status == BLK_PASSED;
    }

    if(success){
        write_cluster_placement_stats_to_clb(packdata->cluster_placement_stats);
        
        int high_fanout_threshold = database.high_fanout_thresholds.get_threshold(cluster_ctx.clb_nlist.block_type(this->index)->name);
                    
        update_cluster_stats(group.vpr_molecule, this->index,
            database.is_clock, //Set of clock nets
            database.is_clock, //Set of global nets (currently all clocks)
            packer_opts.global_clocks,
            packer_opts.alpha, packer_opts.beta,
            packer_opts.timing_driven, packer_opts.connection_driven,
            high_fanout_threshold,
            *packdata->timing_info);

        //for ripple
        //Group* group_ptr = &group;
        groups.push_back(&group);
        return true;
    }

    return false;
}

void VPR_CLB::RemoveGroup(const Group& group){
    // int i;
    // for (i = 0; i < group.vpr_molecule->num_blocks; i++) {
    //     if (group.vpr_molecule->atom_block_ids[i]) {
    //         remove_atom_from_target(this->router_data, group.vpr_molecule->atom_block_ids[i]);
    //     }
    // }
    // for (i = 0; i < group.vpr_molecule->num_blocks; i++) {
    //     if (group.vpr_molecule->atom_block_ids[i]) {
    //         revert_place_atom_block(group.vpr_molecule->atom_block_ids[i], this->router_data, database.atom_molecules);
    //     }
    // }

    // /* Packing failed, but a part of the pb tree is still allocated and pbs have their modes set.
    //     * Before trying to pack next molecule the unused pbs need to be freed and, the most important,
    //     * their modes reset. This task is performed by the cleanup_pb() function below. */
    // auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
    // cleanup_pb(cluster_ctx.clb_nlist.block_pb(this->index));
    Deleteblock();

    for(const Group* _group : groups){
        if(_group->id != group.id){
            AddInsts(*_group);
        }
    }
}

void VPR_CLB::Deleteblock(){
    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
    free_pb_stats_recursive(pb);
    free_router_data(router_data);
    router_data = nullptr;
    cluster_ctx.clb_nlist.remove_block(index);
    packdata->num_clb--;
    for(const Group* group:groups){
        group->vpr_molecule->valid=true;
    }
    type_index_to_cluster_placement_stats.clear();
}

VPR_CLB::VPR_CLB(){
}

VPR_CLB::VPR_CLB(VPR_Pack_Data* vprpackdata , Site* _site){
    packdata=vprpackdata;
    site = _site;
}

VPR_CLB::VPR_CLB(VPR_Pack_Data* vprpackdata , Site* _site , bool _keeprouterdata){
    packdata=vprpackdata;
    site = _site;
    keeprouterdata = _keeprouterdata;
}

VPR_CLB::~VPR_CLB(){
    //delete pb;
    if(CLBIsEmpty){

        // update the data structure holding the LE counts
        if(pb){
            update_le_count(pb, packdata->logic_block_type, packdata->le_pb_type, packdata->le_count);
            free_pb_stats_recursive(pb);
        }

        if(keeprouterdata==true){ //for output , so keep the saved_lb_nets !!!!
            packdata->intra_lb_routing[index]=router_data->saved_lb_nets;
            router_data->saved_lb_nets = nullptr;
        }

        free_router_data(router_data);
        router_data=nullptr;
    }
}

void VPR_CLB::GetResult(Group& group){
    for(const Group* _group:groups){
        for(Instance* inst:_group->instances){
            group.instances.push_back(inst);
        }
    }
}

bool VPR_CLB::IsEmpty(){
    return groups.empty();
}