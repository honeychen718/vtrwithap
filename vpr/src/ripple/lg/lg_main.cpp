#include "pack/clb.h"
#include "alg/bipartite.h"
#include "lg_main.h"
#include "lg_dp_utils.h"
#include "gp_setting.h"
#include "SetupGrid.h"
#include "vtr_math.h"
#include "vpr_clb.h"

const t_ext_pin_util FULL_EXTERNAL_PIN_UTIL(1., 1.);

void read_cluster_placement_stats_from_clb( int type_index,
                                            VPR_CLB *clb,
                                            //map<int,map<t_pb_type*,map<t_pb_graph_node*,ripple_primitive_stats>>> &ripple_cluster_placement_stats,
                                            t_cluster_placement_stats* cluster_placement_stats_ptr){
    int t , m;
    t_cluster_placement_primitive *cur , *prev;
    ripple_primitive_stats ripple_primitive_stats;
    //t_cluster_placement_stats* cluster_placement_stats_ptr;
    for (size_t i = 0; i < clb->block_types.size(); i++) {
        //auto &type=clb->block_types[i];
        //cluster_placement_stats_ptr=&cluster_placement_stats[type_index];
        reset_cluster_placement_stats(cluster_placement_stats_ptr);
        if(clb->type_index_to_cluster_placement_stats.find(type_index)==clb->type_index_to_cluster_placement_stats.end()){
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
                    clb->type_index_to_cluster_placement_stats[type_index][cur->pb_graph_node->pb_type][cur->pb_graph_node]=ripple_primitive_stats;
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
                    ripple_primitive_stats=clb->type_index_to_cluster_placement_stats[type_index][cur->pb_graph_node->pb_type][cur->pb_graph_node];
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

void write_cluster_placement_stats_to_clb(map<int,map<t_pb_type*,map<t_pb_graph_node*,ripple_primitive_stats>>> &ripple_cluster_placement_stats,
                                            t_cluster_placement_stats* cluster_placement_stats){
    //
    int i;
    t_cluster_placement_stats* cluster_placement_stats_ptr;
    t_cluster_placement_primitive *cur;

    for(auto &pair:ripple_cluster_placement_stats){
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

bool vpr_start_new_cluster( VPR_CLB* &clb,t_pack_molecule* molecule,t_packer_opts& packer_opts,
                            std::vector<t_lb_type_rr_node>* &lb_type_rr_graphs,
                            const std::multimap<AtomBlockId, t_pack_molecule*>& atom_molecules,
                            t_pb_graph_node** primitives_list, int max_cluster_size,ClusterBlockId clb_index,
                            ClusteredNetlist* &clb_nlist,t_lb_router_data* &router_data,
                            std::map<t_logical_block_type_ptr, size_t>& num_used_type_instances,
                            const std::map<const t_model*, std::vector<t_logical_block_type_ptr>>& primitive_candidate_block_types,
                            PartitionRegion& temp_cluster_pr,
                            bool balance_block_type_utilization,
                            t_cluster_placement_stats* cluster_placement_stats){
    

    auto& atom_ctx = g_vpr_ctx.atom();
    auto& device_ctx = g_vpr_ctx.mutable_device();
    auto& floorplanning_ctx = g_vpr_ctx.mutable_floorplanning();

    /*Cluster's PartitionRegion is empty initially, meaning it has no floorplanning constraints*/
    PartitionRegion empty_pr;
    floorplanning_ctx.cluster_constraints.push_back(empty_pr);

    /* Allocate a dummy initial cluster and load a atom block as a seed and check if it is legal */
    AtomBlockId root_atom = molecule->atom_block_ids[molecule->root];
    const std::string& root_atom_name = atom_ctx.nlist.block_name(root_atom);
    const t_model* root_model = atom_ctx.nlist.block_model(root_atom);
    
    auto itr = primitive_candidate_block_types.find(root_model);
    VTR_ASSERT(itr != primitive_candidate_block_types.end());
    //std::vector<t_logical_block_type_ptr> candidate_types = itr->second;
    clb->block_types = itr->second;

    if (balance_block_type_utilization) {
        //We sort the candidate types in ascending order by their current utilization.
        //This means that the packer will prefer to use types with lower utilization.
        //This is a naive approach to try balancing utilization when multiple types can
        //support the same primitive(s).
        std::stable_sort(clb->block_types.begin(), clb->block_types.end(),
                         [&](t_logical_block_type_ptr lhs, t_logical_block_type_ptr rhs) {
                             int lhs_num_instances = 0;
                             int rhs_num_instances = 0;
                             // Count number of instances for each type
                             for (auto type : lhs->equivalent_tiles)
                                 lhs_num_instances += device_ctx.grid.num_instances(type);
                             for (auto type : rhs->equivalent_tiles)
                                 rhs_num_instances += device_ctx.grid.num_instances(type);

                             float lhs_util = vtr::safe_ratio<float>(num_used_type_instances[lhs], lhs_num_instances);
                             float rhs_util = vtr::safe_ratio<float>(num_used_type_instances[rhs], rhs_num_instances);
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
    for (size_t i = 0; i < clb->block_types.size(); i++) {
        auto type =clb->block_types[i];
        t_pb* pb = new t_pb;
        pb->pb_graph_node = type->pb_graph_head;
        alloc_and_load_pb_stats(pb, packer_opts.feasible_block_array_size);
        pb->parent_pb = nullptr;
        router_data=alloc_and_load_router_data(&lb_type_rr_graphs[type->index], type);
        cluster_placement_stats_ptr=&cluster_placement_stats[type->index];
        e_block_pack_status pack_result = BLK_STATUS_UNDEFINED;
        read_cluster_placement_stats_from_clb(type->index,clb,cluster_placement_stats_ptr);
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
                                atom_molecules,
                                molecule, primitives_list, pb,
                                gpSetting.num_models, max_cluster_size, clb_index,
                                1,//detailed_routing_stage, set to 1 for now
                                router_data,
                                packer_opts.pack_verbosity,
                                packer_opts.enable_pin_feasibility_filter,
                                packer_opts.feasible_block_array_size,
                                FULL_EXTERNAL_PIN_UTIL,
                                temp_cluster_pr);
            success = (pack_result == BLK_PASSED);
        }
        if (success) {
        //Once clustering succeeds, add it to the clb netlist
            if (pb->name != nullptr) {
                free(pb->name);
            }
            pb->name = vtr::strdup(root_atom_name.c_str());
            clb_index =clb_nlist->create_block(root_atom_name.c_str(), pb, type);
            for(auto &pair:clb->type_index_to_cluster_placement_stats){
                if(pair.first != type->index){
                    clb->type_index_to_cluster_placement_stats.erase(pair.first);
                }
            }
            break;
        } else {
        //Free failed clustering and try again
            free_router_data(router_data);
            free_pb(pb);
            delete pb;
            router_data = nullptr;
        }
    }

    if(!success) {
        printlog(LOG_ERROR, "vpr_start_new_cluster failed!!");
        std::abort();
    }
    //Successfully create cluster
    auto block_type = clb_nlist->block_type(clb_index);
    num_used_type_instances[block_type]++;

    /* Expand FPGA size if needed */
    // Check used type instances against the possible equivalent physical locations
    unsigned int num_instances = 0;
    for (auto equivalent_tile : block_type->equivalent_tiles) {
        num_instances += device_ctx.grid.num_instances(equivalent_tile);
    }

    if (num_used_type_instances[block_type] > num_instances) {
        //to be un//ed
        //device_ctx.grid = create_device_grid(device_layout_name, arch->grid_layouts, num_used_type_instances, target_device_utilization);
        printlog(LOG_INFO, "device to be expanded");
    }
    return success;
}

bool ExceedClkrgn(Group& group, vector<Site*>& candSites) {
    if (database.crmap_nx == 0) return false;

    for (auto site : candSites) {
        if (group.InClkBox(site)) return false;
    }

    cout << group.lgBox.lx() << " " << group.lgBox.ly() << " " << group.lgBox.ux() << " " << group.lgBox.uy() << endl;

    return true;
}

bool Legalizer::MergeGroupToSite(Site* site, Group& group, bool fixDspRam) {
    if (site->pack == NULL) {
        database.place(database.addPack(site->type), site->x, site->y);
    }
    auto& type = site->type->name;
    if (type == SiteType::SLICE) {
        lgData.invokeCount++;
        if (lgData.clbMap[site->x][site->y]->AddInsts(group)) {
            lgData.successCount++;
            return true;
        }
    } else if (type == SiteType::DSP || type == SiteType::BRAM) {
        if (database.place(group.instances[0], site, 0)) {
            if (fixDspRam) group.instances[0]->fixed = true;
            return true;
        }
    } else if (type == SiteType::IO) {
        for (int i = 0; i < (int)site->pack->instances.size(); i++) {
            if (database.place(group.instances[0], site, i)) return true;
        }
    }
    return false;
}

bool Legalizer::MergeMoleculeToSite(Site* site, t_pack_molecule* molecule, 
                                bool fixDspRam,t_packer_opts& packer_opts,
                                 std::vector<t_lb_type_rr_node>* lb_type_rr_graphs,
                                 //t_pack_molecule* molecule_head,
                                 std::multimap<AtomBlockId, t_pack_molecule*>& atom_molecules,
                                 t_pb_graph_node** primitives_list,
                                 int max_cluster_size,ClusteredNetlist* clb_nlist,
                                 std::map<t_logical_block_type_ptr, size_t>& num_used_type_instances,
                                 const std::unordered_set<AtomNetId>& is_clock,
                                 //const t_pack_high_fanout_thresholds& high_fanout_thresholds,
                                 std::shared_ptr<SetupTimingInfo>& timing_info,const t_ext_pin_util_targets& ext_pin_util_targets,
                                 //vtr::vector<ClusterBlockId, std::vector<t_intra_lb_net>*>& intra_lb_routing,
                                 vtr::vector<ClusterBlockId, std::vector<AtomNetId>>& clb_inter_blk_nets,
                                 t_logical_block_type_ptr& logic_block_type,t_pb_type* le_pb_type,
                                 std::vector<int>& le_count,int& num_clb,
                                 const std::map<const t_model*, std::vector<t_logical_block_type_ptr>>& primitive_candidate_block_types,
                                 bool balance_block_type_utilization,
                                 t_cluster_placement_stats* cluster_placement_stats) {
    if (site->pack == NULL) {
        database.place(database.addPack(site->type), site->x, site->y);
    }
    if(site==database.getSite(14,0)){
        cout<<"find site"<<endl;
    }
    AtomBlockId target_blk_id = AtomBlockId(162);
    if(molecule->atom_block_ids[0]==target_blk_id){
        cout<<"find blk"<<endl;
    }
//****************************
    // t_cluster_placement_stats *cluster_placement_stats;
    // t_pb_graph_node** primitives_list;
    // t_lb_router_data* router_data = nullptr;

    // const t_molecule_stats max_molecule_stats = calc_max_molecules_stats(molecule_head);
    // mark_all_molecules_valid(molecule_head);
    // int num_molecules = count_molecules(molecule_head);

    // int cur_cluster_size,cur_pb_depth,max_cluster_size,max_pb_depth;
    // for (const auto& type : g_vpr_ctx.mutable_device().logical_block_types) {
    //     if (is_empty_type(&type))
    //         continue;

    //     cur_cluster_size = get_max_primitives_in_pb_type(type.pb_type);
    //     cur_pb_depth = get_max_depth_of_pb_type(type.pb_type);
    //     if (cur_cluster_size > max_cluster_size) {
    //         max_cluster_size = cur_cluster_size;
    //     }
    //     if (cur_pb_depth > max_pb_depth) {
    //         max_pb_depth = cur_pb_depth;
    //     }
    // }

    // alloc_and_init_clustering(max_molecule_stats,
    //                           &cluster_placement_stats, &primitives_list, molecule_head,
    //                           num_molecules);
    bool success;
    bool is_cluster_legal= false;
    enum e_block_pack_status block_pack_status;
    t_ext_pin_util target_ext_pin_util;
    t_cluster_placement_stats* cur_cluster_placement_stats_ptr;
    //PartitionRegion temp_cluster_pr;
    auto& atom_ctx = g_vpr_ctx.atom();
    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
    VPR_CLB* clb=nullptr;
    if(site->type->name == SiteType::IO){
        for(int i = 0;i<database.subtile_capacity[site->type];i++){
            if(!lgData.clbMap[site->x][site->y][i].is_full){
                clb = &lgData.clbMap[site->x][site->y][i];
                site->cur_cluster=i;
                break;
            }
        }
        if(clb==nullptr){
            return false;
        }
    }else{
        clb = lgData.clbMap[site->x][site->y];
        site->cur_cluster=0;
    }
    
    ClusterBlockId clb_index;
    if(!clb->valid){
        clb_index=(ClusterBlockId)num_clb;
        success=vpr_start_new_cluster(  clb,molecule,packer_opts,lb_type_rr_graphs,
                                        atom_molecules,
                                        primitives_list,max_cluster_size,clb_index,clb_nlist,
                                        clb->router_data,num_used_type_instances,primitive_candidate_block_types,
                                        clb->temp_cluster_pr,balance_block_type_utilization,
                                        cluster_placement_stats);
        if(success){
            //router_data=nullptr;
            clb->valid=true;
            clb->index=clb_index;
            num_clb++;
        }
    }else{
        int type_index;
        clb_index=clb->index;
        type_index=cluster_ctx.clb_nlist.block_type(clb_index)->index;
        cur_cluster_placement_stats_ptr = &cluster_placement_stats[type_index];
        target_ext_pin_util = ext_pin_util_targets.get_pin_util(cluster_ctx.clb_nlist.block_type(clb_index)->name);
        read_cluster_placement_stats_from_clb(type_index,clb,cur_cluster_placement_stats_ptr);
        block_pack_status = try_pack_molecule(cur_cluster_placement_stats_ptr,
                                        atom_molecules,
                                        molecule,
                                        primitives_list,
                                        cluster_ctx.clb_nlist.block_pb(clb_index),
                                        gpSetting.num_models,
                                        max_cluster_size,
                                        clb_index,
                                        1,//detailed_routing_stage set to 1
                                        clb->router_data,
                                        packer_opts.pack_verbosity,
                                        packer_opts.enable_pin_feasibility_filter,
                                        packer_opts.feasible_block_array_size,
                                        target_ext_pin_util,
                                        clb->temp_cluster_pr);
        success = block_pack_status == BLK_PASSED;
    }
    if(success){
        write_cluster_placement_stats_to_clb(clb->type_index_to_cluster_placement_stats,cluster_placement_stats);
    }


    
    //if(high_fanout_thresholds != NULL) cout<<1<<endl;
    //cout<<1<<endl;
    //auto block_type = clb_nlist->block_type(clb_index);
    
    // for(int v=0;v<group.instances.size();v++){
    //     cout<<group.instances[v]->vpratomblkid.id<<"\t";
    // }
    // cout<<endl;
//****************************
    auto& type = site->type->name;
    if (type == SiteType::SLICE) {
        lgData.invokeCount++;
        if (success) {
            lgData.successCount++;
        }
    }
    if(type == SiteType::IO){
        if(success){
            clb->is_full=true;
        }
    }
    return success;
}

bool Legalizer::AssignPackToSite(Site* site, Group& group) {
    if (site->pack == NULL && group.IsTypeMatch(site)) {
        database.place(database.addPack(site->type), site->x, site->y);
        for (unsigned i = 0; i < group.instances.size(); i++) {
            if (group.instances[i] != NULL) {
                database.place(group.instances[i], site, i);
            } else {
                site->pack->instances[i] = NULL;
            }
        }
        return true;
    } else
        return false;
}

void Legalizer::SortCandSitesByHpwl(vector<Site*>& candSites, const Group& group) {
    function<double(Site*)> cal_hpwl = [&](Site* site) {
        double wl = 0;
        double gx = lgData.groupsX[group.id], gy = lgData.groupsY[group.id];

        for (auto net : lgData.group2Nets[group.id]) {
            if (net->isClk) continue;
            const auto& b = lgData.netBox[net->id];
            if (b.size() == 1) continue;

            Box<double> tmpBox(b.x.ou(gx), b.y.ou(gy), b.x.ol(gx), b.y.ol(gy));
            tmpBox.fupdate(site->cx(), site->cy());
            wl += tmpBox.uhp();
        }

        return ((int)(wl * 1000)) / 1000.0;
    };
    ComputeAndSort(candSites, cal_hpwl, less<double>());
    // ComputeAndSort(candSites, cal_hpwl, less<double>(), true);
    // originally, sites are in the order of disp
    // changing to non-stable sort will 3% tot runtime improvement
}

void Legalizer::SortCandSitesByPins(vector<Site*>& candSites, const Group& group) {
    function<int(Site*)> cal_npin = [&](Site* site) { return lgData.nPinPerSite[site->x][site->y]; };
    ComputeAndSort(candSites, cal_npin, less<int>());
}

void Legalizer::SortCandSitesByAlign(vector<Site*>& candSites, const Group& group) {
    function<int(Site*)> cal_align = [&](Site* site) {
        int align = 0;
        for (auto net : lgData.group2Nets[group.id]) {
            if (net->isClk) continue;
            for (auto groupId : lgData.net2Gids[net->id]) {
                if (groupId == group.id) continue;
                int gx = lgData.groupsX[groupId];
                int gy = lgData.groupsY[groupId];
                if (site->x == gx) {
                    align += 1;
                }
                if (site->y == gy) {
                    align += 2;
                    if (SWCol(site->x) == SWCol(gx)) align += 1;
                }
            }
        }
        return align;
    };
    ComputeAndSort(candSites, cal_align, greater<int>());
}

void Legalizer::SortGroupsByGroupsize() {
    function<int(int)> cal_gs = [&](int gid) {
        int size = 0;
        for (auto inst : groups[gid].instances) {
            if (inst == NULL) continue;
            if (inst->IsFF())
                size += 3;
            else if (inst->IsLUT())
                size++;
        }
        return size;
    };
    ComputeAndSort(lgData.groupIds, cal_gs, greater<int>());
}

void Legalizer::SortGroupsByPins() {
    function<int(int)> cal_npin = [&](int gid) { return lgData.group2Nets[gid].size(); };
    ComputeAndSort(lgData.groupIds, cal_npin, greater<int>());
}

void Legalizer::SortGroupsByLGBox() {
    function<int(int)> cal_boxsz = [&](int gid) { return groups[gid].lgBox.x() * groups[gid].lgBox.y(); };
    ComputeAndSort(lgData.groupIds, cal_boxsz, less<int>(), true);
}

void Legalizer::SortGroupsByOptRgnDist() {
    function<double(int)> cal_ordist = [&](int gid) {
        double lx, hx, ly, hy;
        vector<double> boxX, boxY;
        for (auto net : lgData.group2Nets[gid]) {
            if (net->isClk) continue;
            const auto& b = lgData.netBox[net->id];
            if (b.size() == 1) continue;
            boxX.push_back(b.x.ol(groups[gid].x));
            boxX.push_back(b.x.ou(groups[gid].x));
            boxY.push_back(b.y.ol(groups[gid].y));
            boxY.push_back(b.y.ou(groups[gid].y));
        }
        GetMedianTwo(boxX, lx, hx);
        GetMedianTwo(boxY, ly, hy);

        Box<double> optrgn(hx, hy, lx, ly);

        return optrgn.udist(groups[gid].x, groups[gid].y);
    };
    ComputeAndSort(lgData.groupIds, cal_ordist, greater<int>());
}

Legalizer::Legalizer(vector<Group>& _groups) : groups(_groups), lgData(_groups) {}

void Legalizer::Init(lgPackMethod packMethod) { lgData.Init(packMethod); }

void Legalizer::GetResult(lgRetrunGroup retGroup) {
    lgData.GetDispStatics();
    lgData.GetResult(retGroup);
    lgData.GetPackStatics();
}

bool Legalizer::RunAll(lgSiteOrder siteOrder, lgGroupOrder groupOrder) {
    const int MAX_WIN = 1000;

    // SortGroupsByPins();
    // SortGroupsByGroupsize(); //better for overlap reduction
    // SortGroupsByOptRgnDist(); //better for hpwl reduction
    if (groupOrder == GROUP_LGBOX) {
        printlog(LOG_INFO, "group order: GROUP_LGBOX");
        SortGroupsByLGBox();
    } else {
        printlog(LOG_INFO, "group order: DEFAULT");
    }

    int nChain = 0, chainLen = 0, nSucc = 0;

    for (const auto gid : lgData.groupIds) {
        Group& group = groups[gid];

        bool isFixed = false;
        for (auto inst : group.instances) {
            if (inst != NULL && inst->fixed) {
                isFixed = true;
                break;
            }
        }
        if (isFixed) {
            lgData.placedGroupMap[lgData.groupsX[gid]][lgData.groupsY[gid]].push_back(group.id);
            continue;
        }

        int winWidth = 0;
        bool isPlaced = false;
        Site* curSite = NULL;

        while (!isPlaced) {
            vector<Site*> candSites;
            if (siteOrder != SITE_HPWL_SMALL_WIN) {
                while (candSites.size() < 200) {
                    GetWinElem(candSites, database.sites, {lgData.groupsX[gid], lgData.groupsY[gid]}, winWidth);
                    if (candSites.size() == 0) break;
                    winWidth++;
                }
            } else {
                GetWinElem(candSites, database.sites, {lgData.groupsX[gid], lgData.groupsY[gid]}, winWidth);
                winWidth++;
            }

            if (ExceedClkrgn(group, candSites) || winWidth >= MAX_WIN || candSites.size() == 0) {
                printlog(LOG_WARN, "no candSites available");
                break;
            }

            SqueezeCandSites(candSites, group, !group.IsBLE());

            if (siteOrder == SITE_HPWL || siteOrder == SITE_HPWL_SMALL_WIN)
                SortCandSitesByHpwl(candSites, group);
            else if (siteOrder == SITE_ALIGN)
                SortCandSitesByAlign(candSites, group);

            for (unsigned s = 0; !isPlaced && s < candSites.size(); s++) {
                curSite = candSites[s];
                if (MergeGroupToSite(curSite, group, false)) {
                    isPlaced = true;
                    lgData.PartialUpdate(group, curSite);

                    double disp = abs((int)group.x - (int)lgData.groupsX[group.id]) * 0.5 +
                                  abs((int)group.y - (int)lgData.groupsY[group.id]);
                    if (disp != 0 && database.crmap_nx != 0) {
                        int len = ChainMove(group, DISP_OPT);
                        if (len != -1) {
                            nSucc++;
                            chainLen += len;
                        }
                        nChain++;
                    }

                    disp = abs((int)group.x - (int)lgData.groupsX[group.id]) * 0.5 +
                           abs((int)group.y - (int)lgData.groupsY[group.id]);
                    if (disp >= 2 && database.crmap_nx != 0) {
                        int len = ChainMove(group, MAX_DISP_OPT);
                        if (len != -1) {
                            nSucc++;
                            chainLen += len;
                        }
                        nChain++;
                    }
                }
            }
        }

        if (!isPlaced) return false;
    }

    printlog(LOG_INFO,
             "chain move: avgLen=%.2f, #fail=%d, #success=%d(%.2f%%)",
             chainLen * 1.0 / nSucc,
             nChain - nSucc,
             nSucc,
             nSucc * 100.0 / nChain);

    return true;
}

bool Legalizer::RunAll( lgSiteOrder siteOrder, 
                        lgGroupOrder groupOrder,
                        t_packer_opts& packer_opts,////these two are member of vpr_setup
                        std::vector<t_lb_type_rr_node>* lb_type_rr_graphs,//these two are member of vpr_setup
                        std::multimap<AtomBlockId, t_pack_molecule*>& atom_molecules,
                        t_pb_graph_node** primitives_list,
                        int max_cluster_size,ClusteredNetlist* clb_nlist,
                        std::map<t_logical_block_type_ptr, size_t>& num_used_type_instances,
                        const std::unordered_set<AtomNetId>& is_clock,const t_pack_high_fanout_thresholds& high_fanout_thresholds,
                        std::shared_ptr<SetupTimingInfo>& timing_info,
                        const t_ext_pin_util_targets& ext_pin_util_targets,
                        vtr::vector<ClusterBlockId, std::vector<t_intra_lb_net>*>& intra_lb_routing,
                        vtr::vector<ClusterBlockId, std::vector<AtomNetId>>& clb_inter_blk_nets,
                        t_logical_block_type_ptr& logic_block_type,t_pb_type* le_pb_type,
                        std::vector<int>& le_count,int& num_clb,
                        const std::map<const t_model*, std::vector<t_logical_block_type_ptr>>& primitive_candidate_block_types,
                        bool balance_block_type_utilization,
                        t_cluster_placement_stats* cluster_placement_stats) {
    const int MAX_WIN = 1000;

    // SortGroupsByPins();
    // SortGroupsByGroupsize(); //better for overlap reduction
    // SortGroupsByOptRgnDist(); //better for hpwl reduction
    if (groupOrder == GROUP_LGBOX) {
        printlog(LOG_INFO, "group order: GROUP_LGBOX");
        SortGroupsByLGBox();
    } else {
        printlog(LOG_INFO, "group order: DEFAULT");
    }

    int nChain = 0, chainLen = 0, nSucc = 0;
    // for(auto grouptemp:groups){
    //     for (auto instancetemp:grouptemp.instances){
    //         if(instancetemp->id==260){
    //             cout<<"findinstance"<<endl;
    //         }
    //     }
    // }
    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
    for (const auto gid : lgData.groupIds) {
        // cout<<gid<<endl;
        // if(gid==475){
        //     cout<<"checkbreak"<<endl;
        // }

        Group& group = groups[gid];

        // for(auto insttemp:group.instances ){
        //     if (insttemp->id==0){
        //         cout<<"here"<<endl;
        //     }
        // }

        bool isFixed = false;
        for (auto inst : group.instances) {
            if (inst != NULL && inst->fixed) {
                isFixed = true;
                break;
            }
        }
        if (isFixed) {
            lgData.placedGroupMap[lgData.groupsX[gid]][lgData.groupsY[gid]].push_back(group.id);
            continue;
        }

        int winWidth = 0;
        bool isPlaced = false;
        Site* curSite = NULL;

        while (!isPlaced) {
            vector<Site*> candSites;
            if (siteOrder != SITE_HPWL_SMALL_WIN) {
                while (candSites.size() < 200) {
                    GetWinElem(candSites, database.sites, {lgData.groupsX[gid], lgData.groupsY[gid]}, winWidth);
                    if (candSites.size() == 0) break;
                    winWidth++;
                }
            } else {
                GetWinElem(candSites, database.sites, {lgData.groupsX[gid], lgData.groupsY[gid]}, winWidth);
                winWidth++;
            }

            if (ExceedClkrgn(group, candSites) || winWidth >= MAX_WIN || candSites.size() == 0) {
                printlog(LOG_WARN, "no candSites available");
                break;
            }

            SqueezeCandSites(candSites, group, !group.IsBLE());

            if (siteOrder == SITE_HPWL || siteOrder == SITE_HPWL_SMALL_WIN)
                SortCandSitesByHpwl(candSites, group);
            else if (siteOrder == SITE_ALIGN)
                SortCandSitesByAlign(candSites, group);

            for (unsigned s = 0; !isPlaced && s < candSites.size(); s++) {
                curSite = candSites[s];
                isPlaced=true;
                
                if(!MergeMoleculeToSite(curSite, group.vpr_molecule, false,packer_opts,
                                lb_type_rr_graphs,atom_molecules,
                                primitives_list,max_cluster_size,clb_nlist,num_used_type_instances,
                                is_clock,timing_info,ext_pin_util_targets,
                                //intra_lb_routing,
                                clb_inter_blk_nets,logic_block_type,
                                le_pb_type,le_count,num_clb,primitive_candidate_block_types,
                                balance_block_type_utilization,cluster_placement_stats)){
                    isPlaced=false;
                }
                
                if(isPlaced){
                    //place succeed!!!
                    auto clb_index=lgData.clbMap[curSite->x][curSite->y][curSite->cur_cluster].index;
                    int high_fanout_threshold =  high_fanout_thresholds.get_threshold(cluster_ctx.clb_nlist.block_type(clb_index)->name);
                    
                    update_cluster_stats(group.vpr_molecule, clb_index,
                        is_clock, //Set of clock nets
                        is_clock, //Set of global nets (currently all clocks)
                        packer_opts.global_clocks,
                        packer_opts.alpha, packer_opts.beta,
                        packer_opts.timing_driven, packer_opts.connection_driven,
                        high_fanout_threshold,
                        *timing_info);
                
                    lgData.PartialUpdate(group, curSite);

                    double disp = abs((int)group.x - (int)lgData.groupsX[group.id]) +
                                  abs((int)group.y - (int)lgData.groupsY[group.id]);

                    //jia: no clock region!!!!
                    // if (disp != 0 && database.crmap_nx != 0) {
                    //     int len = ChainMove(group, DISP_OPT);
                    //     if (len != -1) {
                    //         nSucc++;
                    //         chainLen += len;
                    //     }
                    //     nChain++;
                    // }

                    // disp = abs((int)group.x - (int)lgData.groupsX[group.id]) +
                    //        abs((int)group.y - (int)lgData.groupsY[group.id]);
                    if (disp >= 2 && database.crmap_nx != 0) {
                        int len = ChainMove(group, MAX_DISP_OPT);
                        if (len != -1) {
                            nSucc++;
                            chainLen += len;
                        }
                        nChain++;
                    }
                }
            }
        }
        if (!isPlaced) return false;
    }
    /****************************************************************
     * after finishing a clb in vpr
     *****************************************************************/
    bool is_cluster_legal;
    auto& atom_ctx = g_vpr_ctx.atom();
    for(int x=0;x<database.sitemap_nx;x++){
        for(int y=0;y<database.sitemap_ny;y++){
            for(int i=0;i< database.subtile_capacity[database.getSite(x,y)->type] ; i++){
                VPR_CLB* clb=&lgData.clbMap[x][y][i];
                if(clb!=nullptr && clb->valid){
                    t_mode_selection_status mode_status;
                    is_cluster_legal = try_intra_lb_route(clb->router_data, packer_opts.pack_verbosity, &mode_status);
                    if (is_cluster_legal) { 
                        intra_lb_routing.push_back(clb->router_data->saved_lb_nets);
                        //VTR_ASSERT((int)intra_lb_routing.size() == num_clb);
                        // if((int)intra_lb_routing.size() != num_clb){
                        //     printlog(LOG_ERROR, "intra_lb_routing.size() != num_clb");
                        // }
                        clb->router_data->saved_lb_nets = nullptr;

                        //Pick a new seed
                        //istart = get_highest_gain_seed_molecule(&seedindex, atom_molecules, seed_atoms);

                        // if (packer_opts.timing_driven) {
                        //     if (num_blocks_hill_added > 0) {
                        //         blocks_since_last_analysis += num_blocks_hill_added;
                        //     }
                        // }

                        /* store info that will be used later in packing from pb_stats and free the rest */
                        t_pb_stats* pb_stats = cluster_ctx.clb_nlist.block_pb(clb->index)->pb_stats;
                        for (const AtomNetId mnet_id : pb_stats->marked_nets) {
                            int external_terminals = atom_ctx.nlist.net_pins(mnet_id).size() - pb_stats->num_pins_of_net_in_pb[mnet_id];
                            /* Check if external terminals of net is within the fanout limit and that there exists external terminals */
                            if (external_terminals < packer_opts.transitive_fanout_threshold && external_terminals > 0) {
                                clb_inter_blk_nets[clb->index].push_back(mnet_id);
                            }
                        }
                        auto cur_pb = cluster_ctx.clb_nlist.block_pb(clb->index);

                        // update the data structure holding the LE counts
                        update_le_count(cur_pb, logic_block_type, le_pb_type, le_count);

                        //print clustering progress incrementally
                        //print_pack_status(num_clb, num_molecules, num_molecules_processed, mols_since_last_print, device_ctx.grid.width(), device_ctx.grid.height());

                        free_pb_stats_recursive(cur_pb);
                    } else {
                        /* Free up data structures and requeue used molecules */
                        num_used_type_instances[cluster_ctx.clb_nlist.block_type(clb->index)]--;
                        revalid_molecules(cluster_ctx.clb_nlist.block_pb(clb->index), atom_molecules);
                        cluster_ctx.clb_nlist.remove_block(clb->index);
                        cluster_ctx.clb_nlist.compress();
                        num_clb--;
                        //seedindex = savedseedindex;
                        abort();
                    }
                }
                //free_router_data(router_data);
                //router_data = nullptr;//? necessary?
            }
        }
    }
    //free_cluster_placement_stats(cluster_placement_stats);




        
    

    printlog(LOG_INFO,
             "chain move: avgLen=%.2f, #fail=%d, #success=%d(%.2f%%)",
             chainLen * 1.0 / nSucc,
             nChain - nSucc,
             nSucc,
             nSucc * 100.0 / nChain);

    return true;
}

void Legalizer::RunPartial() {
    for (auto type : {SiteType::DSP, SiteType::BRAM}) BipartiteLeg(type, 10);
}

void Legalizer::BipartiteLeg(SiteType::Name type, int minNumSites) {
    assert(minNumSites <= 100);
    vector<Group*> cells;
    for (auto& g : groups) {
        if (g.GetSiteType() == type) {
            cells.push_back(&g);
            for (auto inst : g.instances) {
                database.unplace(inst);
            }
        }
    }
    database.ClearEmptyPack();

    std::unordered_map<Site*, int> siteIds;
    vector<Site*> sites;
    vector<vector<pair<int, long>>> allCandSites(cells.size());  // (site, score) TODO: secondary obj disp
    // add safe sites
    for (size_t cid = 0; cid < cells.size(); ++cid) {
        auto& cell = *cells[cid];
        int winWidth = 0;
        bool found = false;
        for (; !found; ++winWidth) {
            vector<Site*> candSites;
            GetWinElem(candSites, database.sites, {lgData.groupsX[cell.id], lgData.groupsY[cell.id]}, winWidth);
            if (ExceedClkrgn(cell, candSites)) {
                printlog(LOG_ERROR, "BipartiteLeg: cannot find safe sites");
                exit(1);
            }
            SqueezeCandSites(candSites, cell, true);
            SortCandSitesByHpwl(candSites, cell);
            for (auto site : candSites) {
                auto it = siteIds.find(site);
                if (it == siteIds.end()) {
                    int sid = siteIds.size();
                    siteIds[site] = sid;
                    sites.push_back(site);
                    allCandSites[cid].push_back({sid, -1});
                    found = true;
                    break;
                }
            }
        }
    }
    // add more sites
    for (size_t cid = 0; cid < cells.size(); ++cid) {
        auto& cell = *cells[cid];
        vector<Site*> candSites;
        for (int winWidth = 0; (int)candSites.size() < minNumSites; ++winWidth) {
            GetWinElem(candSites, database.sites, {lgData.groupsX[cell.id], lgData.groupsY[cell.id]}, winWidth);
            SqueezeCandSites(candSites, cell, true);
        }
        for (auto site : candSites) {
            int sid;
            auto it = siteIds.find(site);
            if (it == siteIds.end()) {
                sid = siteIds.size();
                siteIds[site] = sid;
                sites.push_back(site);
            } else
                sid = it->second;
            allCandSites[cid].push_back({sid, -1});
        }
    }
    // cal wl
    for (size_t cid = 0; cid < cells.size(); ++cid) {
        auto& cell = *cells[cid];
        // cell.print();
        for (auto& site_wl : allCandSites[cid]) {
            auto site = sites[site_wl.first];
            double wl = 0;
            for (auto net : lgData.group2Nets[cell.id]) {
                const auto& b = lgData.netBox[net->id];
                Box<double> tmpBox(b.x.ou(lgData.groupsX[cell.id]),
                                   b.y.ou(lgData.groupsY[cell.id]),
                                   b.x.ol(lgData.groupsX[cell.id]),
                                   b.y.ol(lgData.groupsY[cell.id]));
                tmpBox.fupdate(site->cx(), site->cy());
                wl += tmpBox.uhp();
            }
            site_wl.second = wl * 100;
            // cout << site->x << " " << site->y << " " << wl << endl;
        }
    }

    vector<pair<int, long>> res;
    long cost = 0;
    MinCostBipartiteMatching(allCandSites, allCandSites.size(), sites.size(), res, cost);

    for (size_t cid = 0; cid < cells.size(); ++cid) {
        if (res[cid].first < 0)
            printlog(LOG_ERROR, "BipartiteLeg: cannot find ...");
        else {
            auto site = sites[res[cid].first];
            MergeGroupToSite(site, *cells[cid]);
            lgData.PartialUpdate(*cells[cid], site);
        }
    }
}

double Legalizer::GetHpwl() { return lgData.GetHpwl(); }