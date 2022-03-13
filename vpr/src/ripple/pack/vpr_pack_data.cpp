#include "vpr_pack_data.h"
//for debug
// #include "DebugNew.h"

void VPR_Pack_Data::Init(){
    auto& atom_ctx = g_vpr_ctx.atom();
    t_packer_opts &packer_opts = database.vpr_setup->PackerOpts;

    //in do_clustering , before while()
    std::shared_ptr<PreClusterDelayCalculator> clustering_delay_calc;
    le_count.assign(3,0);
    //vtr::vector<ClusterBlockId, std::vector<AtomNetId>> clb_inter_blk_nets(atom_ctx.nlist.blocks().size());

    const t_molecule_stats max_molecule_stats = calc_max_molecules_stats(database.list_of_pack_molecules);
    mark_all_molecules_valid(database.list_of_pack_molecules);
    int num_molecules = count_molecules(database.list_of_pack_molecules);

    alloc_and_init_clustering(max_molecule_stats,
                              &cluster_placement_stats, &primitives_list, database.list_of_pack_molecules,
                              num_molecules);
    primitive_candidate_block_types = identify_primitive_candidate_block_types();
    // find the cluster type that has lut primitives
    logic_block_type = identify_logic_block_type(primitive_candidate_block_types);
    // find a LE pb_type within the found logic_block_type
    le_pb_type = identify_le_block_type(logic_block_type);

    if (packer_opts.timing_driven) {
        /*
         * Initialize the timing analyzer
         */
        clustering_delay_calc = std::make_shared<PreClusterDelayCalculator>(atom_ctx.nlist, atom_ctx.lookup, packer_opts.inter_cluster_net_delay, database.expected_lowest_cost_pb_gnode);
        timing_info = make_setup_timing_info(clustering_delay_calc, packer_opts.timing_update_type);

        //Calculate the initial timing
        timing_info->update();

        // if (isEchoFileEnabled(E_ECHO_PRE_PACKING_TIMING_GRAPH)) {
        //     auto& timing_ctx = g_vpr_ctx.timing();
        //     tatum::write_echo(getEchoFileName(E_ECHO_PRE_PACKING_TIMING_GRAPH),
        //                       *timing_ctx.graph, *timing_ctx.constraints, *clustering_delay_calc, timing_info->analyzer());

        //     tatum::NodeId debug_tnode = id_or_pin_name_to_tnode(analysis_opts.echo_dot_timing_graph_node);
        //     write_setup_timing_graph_dot(getEchoFileName(E_ECHO_PRE_PACKING_TIMING_GRAPH) + std::string(".dot"),
        //                                  *timing_info, debug_tnode);
        // }

        {
            const t_analysis_opts& analysis_opts = database.vpr_setup->AnalysisOpts;
            auto& timing_ctx = g_vpr_ctx.timing();
            PreClusterTimingGraphResolver resolver(atom_ctx.nlist,
                                                   atom_ctx.lookup, *timing_ctx.graph, *clustering_delay_calc);
            resolver.set_detail_level(analysis_opts.timing_report_detail);

            tatum::TimingReporter timing_reporter(resolver, *timing_ctx.graph,
                                                  *timing_ctx.constraints);

            timing_reporter.report_timing_setup(
                "pre_pack.report_timing.setup.rpt",
                *timing_info->setup_analyzer(),
                analysis_opts.timing_report_npaths);
        }
    }
}

void VPR_Pack_Data::Free(int free_mode, bool outputclustering){
    t_packer_opts& packer_opts = database.vpr_setup->PackerOpts;
    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
    VTR_ASSERT(num_clb == (int)cluster_ctx.clb_nlist.blocks().size());
    if(outputclustering){
        check_clustering();

        if (getEchoEnabled() && isEchoFileEnabled(E_ECHO_CLUSTERS)) {
            echo_clusters(getEchoFileName(E_ECHO_CLUSTERS));
        }
        
        //insert a bug!!!! 
        output_clustering(intra_lb_routing, packer_opts.global_clocks, database.is_clock, database.arch->architecture_id, packer_opts.output_file.c_str(), false);
    }

    VTR_ASSERT(cluster_ctx.clb_nlist.blocks().size() == intra_lb_routing.size());
    for (auto blk_id : cluster_ctx.clb_nlist.blocks())
        free_intra_lb_nets(intra_lb_routing[blk_id]);

    intra_lb_routing.clear();

    free_cluster_placement_stats(cluster_placement_stats);

    for (auto blk_id : cluster_ctx.clb_nlist.blocks())
        cluster_ctx.clb_nlist.remove_block(blk_id);

    cluster_ctx.clb_nlist = ClusteredNetlist();

    num_clb=0;

    free(unclustered_list_head);
    free(memory_pool);

    free(primitives_list);

    if(free_mode==FREE_ALL_FOR_REPACK){
        for (auto blk : g_vpr_ctx.atom().nlist.blocks()) {
            g_vpr_ctx.mutable_atom().lookup.set_atom_clb(blk, ClusterBlockId::INVALID());
            g_vpr_ctx.mutable_atom().lookup.set_atom_pb(blk, nullptr);
        }
        for (auto net : g_vpr_ctx.atom().nlist.nets()) {
            g_vpr_ctx.mutable_atom().lookup.set_atom_clb_net(net, ClusterNetId::INVALID());
        }
    }else{
        VTR_ASSERT(free_mode==FREE_PARTICAL_AT_LAST);
    }
}