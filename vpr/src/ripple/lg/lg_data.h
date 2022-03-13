#ifndef _LG_DATA_H_
#define _LG_DATA_H_

#include "../db/db.h"
#include "../global.h"
#include "../pack/clb.h"
#include "lg.h"
#include "vtr_assert.h"
// #include "echo_files.h"
// #include "output_clustering.h"
#include "vpr_pack_data.h"

using namespace db;

class LGData {
private:
    vector<Group> &groups;

    void InitNetInfo();
    void InitGroupInfo();
    void InitLGStat();
    void InitClkInfo();
    void InitPackVar();//added by jia 

    void Group2Pack();
    void UpdateGroupXY();
    void UpdateGroupXYnOrder();

public:
    vector<double> groupsX;
    vector<double> groupsY;
    vector<int> groupIds;

    vector<vector<vector<int>>> groupMap;
    vector<vector<vector<int>>> placedGroupMap;
    vector<vector<VPR_CLB *>> clbMap;


    VPR_Pack_Data packdata;
    void freeclbdata();

    vector<DynamicBox<double>> netBox;

    vector<int> inst2Gid;
    vector<vector<Net *>> group2Nets;
    vector<vector<int>> net2Gids;

    void PartialUpdate(Group &group, Site *targetSite);
    void ChainUpdate(vector<int> &path, vector<VPR_CLB *> &clbChain, vector<vector<int>> &lgorderChain);

    LGData(vector<Group> &_groups);
    void Init(lgPackMethod packMethod);
    void GetResult(lgRetrunGroup retGroup,lgGetResultMethod all_or_partial);

    // statistic variables
    int invokeCount;
    int successCount;
    vector<vector<double>> dispPerSite;
    vector<double> dispPerGroup;
    vector<vector<int>> nPinPerSite;
    vector<int> nPinPerGroup;

    void GetDispStatics();
    void GetPackStatics();
    double GetHpwl();

    //for congestion adjust
    bool writeclbnets;
};

#endif
