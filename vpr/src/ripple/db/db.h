#pragma once

#include "global.h"
#include "utils/ripple_draw.h"
#include "instance.h"
#include "site.h"
#include "net.h"
#include "swbox.h"
#include "group.h"
#include "clkrgn.h"
#include "timing_info.h"
#include "prepack.h"
#include "pack.h"
#include "PreClusterDelayCalculator.h"
#include "PreClusterTimingGraphResolver.h"
#include "cluster.h"
#include "tatum/TimingReporter.hpp"
#include "cluster_placement.h"

using namespace std;

namespace db {

class Database {
private:
    // net
    unordered_map<string, Net*> name_nets;

    // instance
    //unordered_map<Master::Name, Master*, hash<int>> name_masters;
    //unordered_map<const t_model*, Master*> model_masters;//for vpr model
    unordered_map<string, Instance*> name_instances;
    unordered_map<AtomBlockId,Instance*> atom_instances; 

    // site
    unordered_map<Resource::Name, Resource*, hash<int>> name_resources;
    unordered_map<SiteType::Name, SiteType*, hash<int>> name_sitetypes;

public:
    string bmName = "";  // benchmark name (for dev)
    Database();
    ~Database();
    void setup();
    void print();

    //vpr setup added by jia
    t_vpr_setup* vpr_setup;
    t_arch* arch;


    //atom
    std::multimap<AtomBlockId, t_pack_molecule*> atom_molecules;

    //about pack
    bool balance_block_type_util; //before do clustering
    std::unordered_set<AtomNetId> is_clock;//before do clustering
    t_pack_molecule* list_of_pack_molecules;//molecule_head
    t_ext_pin_util_targets target_external_pin_util;//dont know
    t_pack_high_fanout_thresholds high_fanout_thresholds;//before do clustering 
    std::unordered_map<AtomBlockId, t_pb_graph_node*> expected_lowest_cost_pb_gnode;//before do clustering
    void setuppackvar(vector<Group>& groups);
    void freepackvar();

    //for congestion adjest
    vector<ClbNet*> clbnets;
    unordered_map<string, ClbNet*> name_clbnets;
    unordered_map<ClusterBlockId,Site*> clusterblockid_site;
    void writeclbnets();
    ClbNet* get_net(const string &name);
    ClbNet* create_net(const string &name);

    unsigned ffPerSlice, lutPerSlice;//added by jia

    int num_models;//added by jia

    // net
    vector<Net*> nets;

    // instance
    vector<Master*> masters;
    vector<Instance*> instances;

    // site
    map<const t_model*, std::vector<t_logical_block_type_ptr>> primitive_candidate_block_types;
    map<SiteType* , int> subtile_capacity; //added by jia
    vector<Resource*> resources;
    vector<SiteType*> sitetypes;
    vector<Pack*> packs;
    vector<vector<Site*>> sites;
    int sitemap_nx;
    int sitemap_ny;
    void setSiteMap(int nx, int ny);
    bool place(Instance* instance, int x, int y, int slot);
    bool place(Instance* instance, Site* site, int slot);
    bool place(Instance* instance, int x, int y);
    bool place(Instance* instance, Site* site);
    bool unplace(Instance* instance);
    bool unplaceAll();
    bool place(Pack* pack, int x, int y);
    bool place(Pack* pack, Site* site);
    bool unplace(Pack* pack);
    void ClearEmptyPack();

    // swbox
    vector<vector<SwitchBox*>> switchboxes;
    int switchbox_nx;
    int switchbox_ny;
    void setSwitchBoxes(int nx, int ny);
    bool setDemand(int x1, int y1, int x2, int y2, int demand);
    bool setDemand(SwitchBox* a, SwitchBox* b, int demand);
    bool setSupply(int x1, int y1, int x2, int y2, int supply);
    bool setSupply(SwitchBox* a, SwitchBox* b, int supply);
    void setSupplyAll();
    enum PinRule {
        PinRuleNone = 0,
        PinRuleSlice = 1,
        PinRuleDSP0 = 2,
        PinRuleDSP1 = 3,
        PinRuleBRAM = 4,
        PinRuleInput = 5,
        PinRuleOutput = 6,
        PinRuleInputS = 7,
        PinRuleOutputS = 8,
    };
    vector<vector<int>> pinSwitchBoxMap;
    void setPinMap();
    PinRule getPinRule(Instance* inst);
    int getIOY(Instance* inst);
    void setSwitchBoxPins(Instance* instance);
    void setSwitchBoxPins();
    int clkPinIdx, srPinIdx, cePinIdx, ffIPinIdx;  // FF pin index
    int lutOPinIdx;                                // LUT pin index
    void setCellPinIndex();

    // clkrgn
    vector<vector<ClkRgn*>> clkrgns;
    vector<vector<HfCol*>> hfcols;
    int crmap_nx;
    int crmap_ny;
    // const unsigned COLRGN_NCLK=6;
    // const unsigned CLKRGN_NCLK=12;
    const unsigned COLRGN_NCLK = 12;
    const unsigned CLKRGN_NCLK = 24;

    vector<vector<double>> targetDensity;
    int tdmap_nx;
    int tdmap_ny;

    bool isPackValid(Pack* pack);
    bool isPlacementValid();
    double getHPWL(bool printXY = false);
    double getSiteHPWL();
    double getSwitchBoxHPWL(bool printXY = false);
    int getRouteNet();
    //int getTotNumDupInputs();

    Master* addMaster(const Master& master);
    Instance* addInstance(const Instance& instance);
    Net* addNet(const Net& net);
    Resource* addResource(const Resource& resource);
    SiteType* addSiteType(const SiteType& sitetype);
    Site* addSite(int x, int y, SiteType* sitetype);
    Pack* addPack(SiteType* sitetype);
    SwitchBox* addSwitchBox(int x, int y);

    Master* getMaster(Master::Name name);
    Master* getMaster(const t_model* model , int lut_num_input_pins = -1);//for vpr model
    Instance* getInstance(const string& name);
    Instance* getInstance(const AtomBlockId & atom);
    Net* getNet(const string& name);
    Resource* getResource(Resource::Name name);
    SiteType* getSiteType(SiteType::Name name);
    SiteType* getSiteType(const t_physical_tile_type_ptr tile_type);
    Site* getSite(int x, int y);
    SwitchBox* getSwitchBox(int x, int y);

    /***** File IO (defined in db_bookshelf.cpp) *****/
    bool readAux(string file);
    bool readNodes(string file);
    bool readNets(string file);
    bool readWts(string file);
    bool readPl(string file);
    bool readScl(string file);
    bool readLib(string file);
    bool writePl(string file);
    bool readArch();
    
    bool read_from_vpr_database();
    bool alloc_and_load_Masters();
    bool alloc_and_load_Instances();
    bool alloc_and_load_Resources();
    bool alloc_and_load_Sites();
    bool alloc_and_load_Nets();

    /***** Drawing (defined in db_draw.cpp *****/
    enum DrawType {
        DrawInstances,
        DrawFixedPins,
        DrawSites,
    };
    DrawCanvas* canvas;
    void drawSites();
    void drawInstances();
    void drawFixedPins();
    void draw(DrawType type);
    void draw(string file, vector<DrawType>& types);
    void draw(string file, DrawType type);
    void saveDraw(string file);
    void resetDraw();
    void resetDraw(int color);
    void resetDraw(unsigned char r, unsigned char g, unsigned char b);
};

extern Database database;

// utils
int SWCol(int x);
int NumDupInputs(const Instance& lhs, const Instance& rhs);
bool IsLUTCompatible(const Instance& lhs, const Instance& rhs);

inline bool sameClk(const Instance* inst1, const Instance* inst2) {
    return inst1->pins[database.clkPinIdx]->net == inst2->pins[database.clkPinIdx]->net;
}

inline bool sameSr(const Instance* inst1, const Instance* inst2) {
    return inst1->pins[database.srPinIdx]->net == inst2->pins[database.srPinIdx]->net;
}

inline bool sameCe(const Instance* inst1, const Instance* inst2) {
    return inst1->pins[database.cePinIdx]->net == inst2->pins[database.cePinIdx]->net;
}

inline bool sameCSC(const Instance* inst1, const Instance* inst2) {
    return sameClk(inst1, inst2) && sameSr(inst1, inst2) && sameCe(inst1, inst2);
}
}  // namespace db