#ifndef _DP_DP_DATA_H_
#define _DP_DP_DATA_H_

#include "db/db.h"
#include "pack/clb.h"
#include "lgclk/lgclk_data.h"
#include "cong/cong.h"

using namespace db;

class DPData {
private:
    CongEstBB congEst;
    bool useCLB1;
    vector<Group>& groups;
    map<VPR_CLB* , map<AtomBlockId,const t_pb*>> resumebox;  //key is VPR_CLB ptr in clbMap
    vector<VPR_CLB*> resumechain;
public:
    ColrgnData colrgnData;
    vector<vector<VPR_CLB*>> clbMap;
    vector<vector<vector<int>>> groupMap; //???? group.id or group index in groups????

    VPR_Pack_Data* packdata;

    inline VPR_CLB* NewCLB() {
        return new VPR_CLB;
    }
    
    //back up clb that this clb cover , maybe in clbmap , also could be in clbchain 
    inline void BackupCLB(VPR_CLB* &backupclb,VPR_CLB* &newclb){
        auto& atom_ctx = g_vpr_ctx.mutable_atom();
        assert(resumebox.find(newclb)==resumebox.end());
        for(const Group* group : backupclb->groups){
            for(Instance* inst : group->instances){
                resumebox[newclb][inst->vpratomblkid]=atom_ctx.lookup.atom_pb(inst->vpratomblkid);
                atom_ctx.lookup.set_atom_pb(inst->vpratomblkid,nullptr);
                atom_ctx.lookup.set_atom_clb(inst->vpratomblkid,ClusterBlockId::INVALID());
            }
        }
    }

    inline VPR_CLB* NewCLB(Site* site) {
        //check the clb to cover is in clbmap or clbchain
        VPR_CLB* backupclb=NULL; 
        for(auto iter=resumechain.rbegin();iter!=resumechain.rend();iter++){
            if((*iter)->site == site){
                backupclb=*iter;//leastest clb that in this site in clb chain
                break;
            }
        }
        if(backupclb==NULL) backupclb=clbMap[site->x][site->y];//not in chain,backup clb in clbmap
        //revalid the molecules
        for(const Group* group : backupclb->groups){
            if(!group->vpr_molecule->valid){
                group->vpr_molecule->valid=true;
            }
        }

        VPR_CLB* clb = new VPR_CLB(clbMap[site->x][site->y]->packdata, site, 0);
        clb->index = clbMap[site->x][site->y]->index;
        //backup clb in clbmap
        BackupCLB(backupclb,clb);
        resumechain.push_back(clb);
        return clb;
    }

    inline void ResumeCLB(VPR_CLB* &clb){//for chain move only !!!input clb is clb in clbMap
        auto& atom_ctx = g_vpr_ctx.mutable_atom();
        for(auto &pair : resumebox[clb]){
            atom_ctx.lookup.set_atom_pb(pair.first,pair.second);
            atom_ctx.lookup.set_atom_clb(pair.first,clb->index);
        }
        resumebox.erase(clb);
        for(auto iter=resumechain.begin();iter!=resumechain.end();iter++){
            if(*iter == clb){
                resumechain.erase(iter);
                break;
            }
        }
    }

    inline void DeleteCLB(VPR_CLB* &clb) {//for failed dp
        update_le_count(clb->pb, packdata->logic_block_type, packdata->le_pb_type, packdata->le_count);
        free_pb_stats_recursive(clb->pb);
        free_pb(clb->pb);
        clb->pb = NULL;
        ResumeCLB(clb);
        delete clb;
        clb=NULL;
    }

    inline void UpdateCLB(VPR_CLB* &clb1, VPR_CLB* &clb2){//for successed dp
        assert(clb1!=clb2);
        auto& clb_nlist = g_vpr_ctx.mutable_clustering().clb_nlist;
        clb1->type_index_to_cluster_placement_stats=clb2->type_index_to_cluster_placement_stats;
        clb1->temp_cluster_pr = clb2->temp_cluster_pr;
        clb1->groups.clear();
        clb1->groups = clb2->groups;
        clb1->pb = clb2->pb;
        clb_nlist.update_block(clb1->index,clb2->pb);
        clb2->pb = NULL;
        resumebox.erase(clb2);
        for(auto iter=resumechain.begin();iter!=resumechain.end();iter++){
            if(*iter == clb2){
                resumechain.erase(iter);
                break;
            }
        }
        delete clb2;
        clb2 = NULL;
    }

    inline void UpdateCLB(VPR_CLB* &clb){//for successed dp
        assert(clb!=clbMap[clb->site->x][clb->site->y]);
        auto& clb_nlist = g_vpr_ctx.mutable_clustering().clb_nlist;
        clb_nlist.update_block(clb->index,clb->pb);
        clbMap[clb->site->x][clb->site->y]->pb=NULL;
        resumebox.erase(clb);
        for(auto iter=resumechain.begin();iter!=resumechain.end();iter++){
            if(*iter == clb){
                resumechain.erase(iter);
                break;
            }
        }
        delete clbMap[clb->site->x][clb->site->y];
        clbMap[clb->site->x][clb->site->y]=clb;
        clb=NULL;
    }

    bool IsClkMoveLeg(Group& group, Site* site);
    bool IsClkMoveLeg(Pack* pack, Site* site);
    bool IsClkMoveLeg(const vector<pair<Pack*, Site*>>& pairs);

    bool WorsenCong(Site* src, Site* dst);

    DPData(vector<Group>& groups);
    ~DPData();
};

#endif