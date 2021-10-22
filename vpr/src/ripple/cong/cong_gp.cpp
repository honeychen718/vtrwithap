#include "cong_gp.h"
#include "lg/lg_main.h"
#include "gp/gp.h"
#include "cong_est_bb.h"
#include "prepack.h"
#include "pack.h"
#include "timing_info.h"

extern t_molecule_link* unclustered_list_head;
extern t_molecule_link* memory_pool;

Legalizer* legalizer;
//******************************************up
t_pack_molecule* TurnGroupsIntoMolecules(vector<Group>& groups,
                                        std::multimap<AtomBlockId, t_pack_molecule*>& atom_molecules,
                                        std::unordered_map<AtomBlockId, t_pb_graph_node*>& expected_lowest_cost_pb_gnode){
    t_pack_molecule* list_of_molecules_head;
    t_pack_molecule* cur_molecule;
    cur_molecule = list_of_molecules_head = nullptr;
    for(Group& group:groups){
        cur_molecule = new t_pack_molecule;
        cur_molecule->valid = true;
        cur_molecule->type = MOLECULE_FORCED_PACK;
        cur_molecule->atom_block_ids = std::vector<AtomBlockId>(group.instances.size()); //Initializes invalid
        cur_molecule->num_blocks = group.instances.size();
        //cur_molecule->root = 0;
        for(int i=0; i<group.instances.size(); ++i){
            Instance* &inst = group.instances[i];
            cur_molecule->atom_block_ids[i]=inst->vpratomblkid;
            atom_molecules.insert({inst->vpratomblkid, cur_molecule});
        }
        cur_molecule->next = list_of_molecules_head;
        list_of_molecules_head = cur_molecule;
        group.vpr_molecule=cur_molecule;

    }

    return list_of_molecules_head;
}
//******************************************down
void gp_cong(vector<Group>& groups, int iteration ,t_vpr_setup& vpr_setup) {
    printlog(LOG_INFO, "");
    printlog(LOG_INFO, " = = = = begin congestion-driven GP = = = = ");
    printlog(LOG_INFO, "");
    double prevHpwl, curHpwl, origHpwl;

    // initial legalization
    legalizer = new Legalizer(groups);
    legalizer->Init(USE_VPR_CLB);

//before do_clustering
//******************************************up
    t_packer_opts* packer_opts = &vpr_setup.PackerOpts;
    std::vector<t_lb_type_rr_node>* lb_type_rr_graphs=vpr_setup.PackerRRGraph;
    std::unordered_set<AtomNetId> is_clock;
    std::multimap<AtomBlockId, t_pack_molecule*> atom_molecules;
    std::unordered_map<AtomBlockId, t_pb_graph_node*> expected_lowest_cost_pb_gnode;
    std::unique_ptr<t_pack_molecule, decltype(&free_pack_molecules)> list_of_pack_molecules(nullptr, free_pack_molecules);
    is_clock = alloc_and_load_is_clock(packer_opts->global_clocks);
    
    list_of_pack_molecules.reset(TurnGroupsIntoMolecules(groups,
                                                        atom_molecules,
                                                        expected_lowest_cost_pb_gnode));
    t_ext_pin_util_targets target_external_pin_util = parse_target_external_pin_util(packer_opts->target_external_pin_util);
    t_pack_high_fanout_thresholds high_fanout_thresholds = parse_high_fanout_thresholds(packer_opts->high_fanout_threshold);
    t_pack_molecule* molecule_head=list_of_pack_molecules.get();
//*******************************************down

//in do_clustering , before while()
//********************************************up
    std::map<t_logical_block_type_ptr, size_t> num_used_type_instances;

    t_cluster_placement_stats *cluster_placement_stats;
    t_pb_graph_node** primitives_list;
    t_lb_router_data* router_data = nullptr;

    auto& atom_ctx = g_vpr_ctx.atom();
    auto& device_ctx = g_vpr_ctx.mutable_device();
    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();

    vtr::vector<ClusterBlockId, std::vector<t_intra_lb_net>*> intra_lb_routing;

    std::shared_ptr<SetupTimingInfo> timing_info;

    std::vector<int> le_count(3, 0);
    int num_clb = 0;

    vtr::vector<ClusterBlockId, std::vector<AtomNetId>> clb_inter_blk_nets(atom_ctx.nlist.blocks().size());

    const t_molecule_stats max_molecule_stats = calc_max_molecules_stats(molecule_head);
    mark_all_molecules_valid(molecule_head);
    int num_molecules = count_molecules(molecule_head);

    int cur_cluster_size,cur_pb_depth,max_cluster_size,max_pb_depth;
    for (const auto& type : g_vpr_ctx.mutable_device().logical_block_types) {
        if (is_empty_type(&type))
            continue;

        cur_cluster_size = get_max_primitives_in_pb_type(type.pb_type);
        cur_pb_depth = get_max_depth_of_pb_type(type.pb_type);
        if (cur_cluster_size > max_cluster_size) {
            max_cluster_size = cur_cluster_size;
        }
        if (cur_pb_depth > max_pb_depth) {
            max_pb_depth = cur_pb_depth;
        }
    }

    alloc_and_init_clustering(max_molecule_stats,
                              &cluster_placement_stats, &primitives_list, molecule_head,
                              num_molecules);
    auto primitive_candidate_block_types = identify_primitive_candidate_block_types();
    // find the cluster type that has lut primitives
    auto logic_block_type = identify_logic_block_type(primitive_candidate_block_types);
    // find a LE pb_type within the found logic_block_type
    auto le_pb_type = identify_le_block_type(logic_block_type);


//******************************************down
    legalizer->RunAll(  SITE_HPWL_SMALL_WIN, DEFAULT,packer_opts,lb_type_rr_graphs,atom_molecules,
                        cluster_placement_stats,primitives_list,max_cluster_size,&cluster_ctx.clb_nlist,
                        router_data,num_used_type_instances,is_clock,high_fanout_thresholds,timing_info,
                        target_external_pin_util,intra_lb_routing,clb_inter_blk_nets,logic_block_type,
                        le_pb_type,le_count,num_clb);
    /****************************************************************
     * Free Data Structures (after while in do_clustering)
     *****************************************************************/
    if((int)cluster_ctx.clb_nlist.blocks().size() != num_clb){
        printlog(LOG_ERROR, "intra_lb_routing.size() != num_clb");
    }
    check_clustering();
    ////output_clustering(intra_lb_routing, packer_opts.global_clocks, is_clock, arch->architecture_id, packer_opts.output_file.c_str(), false);
    VTR_ASSERT(cluster_ctx.clb_nlist.blocks().size() == intra_lb_routing.size());
    for (auto blk_id : cluster_ctx.clb_nlist.blocks())
        free_intra_lb_nets(intra_lb_routing[blk_id]);

    intra_lb_routing.clear();


    free_cluster_placement_stats(cluster_placement_stats);

    for (auto blk_id : cluster_ctx.clb_nlist.blocks())
        cluster_ctx.clb_nlist.remove_block(blk_id);

    cluster_ctx.clb_nlist = ClusteredNetlist();

    free(unclustered_list_head);//check whethere unclustered_list_head is given value in alloc_and_init_clustering()
    free(memory_pool);//same as up

    free(primitives_list);



    //******************************************************************
    legalizer->GetResult(NO_UPDATE);
    origHpwl = curHpwl = database.getHPWL();
    printlog(LOG_INFO, "**************");

    for (int iter = 0; iter < iteration; iter++) {
        // inflate
        AdjAreaByCong(groups);

        // gp again
        gplace(groups);

        // legalize
        legalizer->Init(USE_VPR_CLB);
        legalizer->RunAll(SITE_HPWL_SMALL_WIN, DEFAULT);
        legalizer->GetResult(NO_UPDATE);
        prevHpwl = curHpwl;
        curHpwl = database.getHPWL();

        printlog(LOG_INFO, "= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =");
        printlog(LOG_INFO,
                 "iter %d: prev_WL=%0f, cur_WL=%0f, delta=%.2lf%%",
                 iter,
                 prevHpwl,
                 curHpwl,
                 (curHpwl / prevHpwl - 1) * 100.0);
        printlog(LOG_INFO, "= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =");
    }
    printlog(LOG_INFO, "");
    printlog(LOG_INFO, " = = = = finish congestion-driven GP = = = = ");
    printlog(LOG_INFO,
             "Totally, orig_WL=%0f, cur_WL=%0f, delta=%.2lf%%",
             origHpwl,
             curHpwl,
             (curHpwl / origHpwl - 1) * 100.0);
    CongEstBB cong;
    cong.Run();
    WriteMap("cong_final", cong.siteDemand);

    delete legalizer;
}

void GetBleDispMap(vector<Group>& groups, vector<vector<double>>& bleDispMap) {
    bleDispMap.resize(database.sitemap_nx, vector<double>(database.sitemap_ny, 0.0));
    for (int x = 0; x < database.sitemap_nx; x++)
        for (int y = 0; y < database.sitemap_ny; y++)
            if (database.sites[x][y]->pack != NULL && database.sites[x][y]->type->name == SiteType::SLICE) {
                unsigned num = 0;
                for (auto gid : legalizer->lgData.groupMap[x][y]) {
                    if (groups[gid].instances[0]->IsLUTFF()) {
                        ++num;
                        bleDispMap[x][y] += legalizer->lgData.dispPerGroup[gid];
                    }
                }
                if (num != 0) bleDispMap[x][y] /= num;
            }
}

void AdjAreaByCong(vector<Group>& groups) {
    CongEstBB cong;
    cong.Run();
    static int cnt = 0;
    WriteMap("cong_iter" + to_string(cnt++), cong.siteDemand);
    int nScaleUp = 0, nScaleDown = 0, nLargeDisp = 0;
    double maxAreaScale = 1.0, minAreaScale = 1.0;

    // only consider sites with bles
    vector<Point2<int>> sites;
    for (int x = 0; x < database.sitemap_nx; x++)
        for (int y = 0; y < database.sitemap_ny; y++)
            if (database.sites[x][y]->pack != NULL && cong.siteDemand[x][y] > cong.minDem) {
                bool ble = false;
                for (auto gid : legalizer->lgData.groupMap[x][y]) {
                    if (groups[gid].instances[0]->IsLUTFF()) {
                        ble = true;
                        break;
                    }
                }
                if (ble) sites.emplace_back(x, y);
            }
    function<double(Point2<int>)> get_demand = [&](const Point2<int> s) { return cong.siteDemand[s.x()][s.y()]; };
    ComputeAndSort(sites, get_demand, less<double>());

    // inflate (also handle large disp?)
    for (unsigned i = sites.size() - 1; i > 0.9 * sites.size(); --i) {
        int x = sites[i].x();
        int y = sites[i].y();
        if (cong.siteDemand[x][y] < 360) break;
        nScaleUp++;
        for (auto gid : legalizer->lgData.groupMap[x][y]) {
            if (!groups[gid].instances[0]->IsLUTFF()) continue;
            groups[gid].areaScale *= 1.2;
            if (groups[gid].areaScale > maxAreaScale) maxAreaScale = groups[gid].areaScale;
        }
    }

    // shrink
    // vector<vector<double>> bleDispMap;
    // GetBleDispMap(groups, bleDispMap);
    for (unsigned i = 0; i < 0.3 * sites.size(); ++i) {
        int x = sites[i].x();
        int y = sites[i].y();
        if (cong.siteDemand[x][y] > 180) break;
        // if (bleDispMap[x][y] > 0.5){
        if (legalizer->lgData.dispPerSite[x][y] > 2) {
            ++nLargeDisp;
            continue;
        }
        nScaleDown++;
        for (auto gid : legalizer->lgData.groupMap[x][y]) {
            if (!groups[gid].instances[0]->IsLUTFF()) continue;
            groups[gid].areaScale *= 0.8;
            if (groups[gid].areaScale < minAreaScale) minAreaScale = groups[gid].areaScale;
        }
    }

    printlog(LOG_INFO,
             "#site: %d, #up: %d, #down: %d, #large disp: %d, max as: %.2lf, min as: %.2lf",
             sites.size(),
             nScaleUp,
             nScaleDown,
             nLargeDisp,
             maxAreaScale,
             minAreaScale);
}