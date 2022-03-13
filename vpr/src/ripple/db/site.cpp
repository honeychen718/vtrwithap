#include "site.h"
#include "db.h"

using namespace db;

/***** Resource *****/
Resource::Name Resource::NameString2Enum(const string &name) {
    using db::Resource;

    if (name == "LUT")
        return LUT;
    else if (name == "FF")
        return FF;
    else if (name == "CARRY8")
        return CARRY8;
    else if (name == "DSP48E2"||name == "multiply")
        return DSP48E2;
    else if (name == "RAMB36E2"||name == "single_port_ram")
        return RAMB36E2;
    else if (name == "IO")
        return IO;
    else {
        cerr << "unknown master name: " << name << endl;
        exit(1);
    }
}

string resourceNameStrs[] = {"LUT", "FF", "CARRY8", "DSP48E2", "RAMB36E2", "IO"};
string Resource::NameEnum2String(const Resource::Name &name) { return resourceNameStrs[name]; }

Resource::Resource(Name n) { this->name = n; }

Resource::Resource(const Resource &resource) {
    name = resource.name;
    masters = resource.masters;
}

void Resource::addMaster(Master *master) {
    masters.push_back(master);
    master->resource = this;
}

/***** SiteType *****/
SiteType::Name SiteType::NameString2Enum(const string &name) {
    using db::SiteType;
    if (name == "SLICE"||name == "clb")
        return SLICE;
    else if (name == "DSP"||name == "mult_36")
        return DSP;
    else if (name == "BRAM"||name == "memory")
        return BRAM;
    else if (name == "IO"||name == "io")
        return IO;
    else if (name == "EMPTY")
         return EMPTY;
    else {
        cerr << "unknown site type name: " << name << endl;
        exit(1);
    }
}

string siteTypeNameStrs[] = {"SLICE", "DSP", "BRAM", "IO"};
string SiteType::NameEnum2String(const SiteType::Name &name) { return siteTypeNameStrs[name]; }

SiteType::SiteType(Name n) { this->name = n; }

SiteType::SiteType(const SiteType &sitetype) {
    name = sitetype.name;
    //resources = sitetype.resources;
}
SiteType::SiteType(const t_physical_tile_type* tile_type){
    auto iter = tile_name_to_sitetype_name.find(string(tile_type->name));
    if(iter != tile_name_to_sitetype_name.end()){
        this->name = iter->second;
    }else{
        this->name = UNKNOWN;
    }

    this->physical_tile = tile_type;
}

void SiteType::addResource(Resource *resource) {
    // resources.push_back(resource);
    // resource->siteType = this;
    assert(0);
}
void SiteType::addResource(Resource *resource, int count) {
    // for (int i = 0; i < count; i++) {
    //     addResource(resource);
    // }
    assert(0);
}

bool SiteType::matchInstance(Instance *instance) {
    auto iter = database.primitive_candidate_block_types.find(instance->master->vpr_model);
    if(iter != database.primitive_candidate_block_types.end()){
        auto& logical_block_types = iter->second;
        for(auto type:logical_block_types){
            for(auto tile : type->equivalent_tiles){
                if(tile == this->physical_tile){
                    return true;
                }
            }
        }
    }else{
        assert(0);
    }
    return false;
}

/***** Site *****/
Site::Site() {
    type = NULL;
    pack = NULL;
    x = -1;
    y = -1;
    w = 0;
    h = 0;
    //hasclb=false;//add by jia 
}

Site::Site(int x, int y, SiteType *sitetype) {
    this->x = x;
    this->y = y;
    this->w = 1;
    this->h = 1;
    this->type = sitetype;
    this->pack = NULL;
    //hasclb=false;//add by jia
}

// Site::~Site() {
//     delete router_data;
// }

/***** Pack *****/

Pack::Pack() {
    type = NULL;
    site = NULL;
}

Pack::Pack(SiteType *sitetype) {
    //this->instances.resize(sitetype->resources.size(), NULL);
    this->type = sitetype;
    this->site = NULL;
}

void Pack::print() {
    string instList = "";
    int nInst = 0;
    for (auto inst : instances)
        if (inst != NULL) {
            nInst++;
            instList = instList + " " + to_string(inst->id);
        }
    printlog(LOG_INFO, "(x,y)=(%lf,%lf), %d instances=[%s]", site->cx(), site->cy(), nInst, instList.c_str());
}

// bool Pack::IsEmpty() {
//     bool isEmptyPack = true;
//     for (auto inst : instances) {
//         if (inst != NULL) {
//             isEmptyPack = false;
//             break;
//         }
//     }
//     return isEmptyPack;
// }

bool Pack::IsEmpty() {
    return instances.size() == 0;
}

void Pack::RemoveInst(Instance *inst){
    for(vector<Instance *>::iterator iter=instances.begin();iter!=instances.end();iter++){ 
        if(*iter == inst){
            instances.erase(iter);
            break;
        }
    }
}