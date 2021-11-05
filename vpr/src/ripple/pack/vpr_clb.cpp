#include "vpr_clb.h"


bool VPR_CLB::AddInsts(const Group& group,t_vpr_setup& vpr_setup){

}

VPR_CLB::VPR_CLB(){
    valid=false;
}

VPR_CLB::~VPR_CLB(){
    //delete pb;
    delete router_data;
    cout<<"this clb "<<"deconstructed"<<endl;
}