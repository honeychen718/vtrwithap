#pragma once

#include "../global.h"
#include "pack_types.h"

namespace db {

class Master;
class Instance;
class SiteType;
class Pack;

class Resource {
public:
    enum Name { LUT, FF, CARRY8, DSP48E2, RAMB36E2, IO };
    static Name NameString2Enum(const string &name);
    static string NameEnum2String(const Resource::Name &name);
    unordered_map<string , Resource::Name> model_name_to_resource_name{//all possible model name in vpr arch ,should update constantly
        {"adder" , CARRY8},
        {"multiply" , DSP48E2},
        {"single_port_ram" , RAMB36E2},
        {"dual_port_ram" , RAMB36E2},
        {".input" , IO},
        {".output" , IO},
        {".latch" , FF},
        {".names" , LUT}
    };
    Name name;
    SiteType *siteType;
    vector<Master *> masters;

    Resource(Name n);
    Resource(const Resource &resource);

    void addMaster(Master *master);
};

class SiteType {
public:
    enum Name { SLICE, DSP, BRAM, IO ,EMPTY,UNKNOWN};
    static Name NameString2Enum(const string &name);
    static string NameEnum2String(const SiteType::Name &name);
    unordered_map<string , SiteType::Name> tile_name_to_sitetype_name{
        {"clb" , SLICE},
        {"mult_36" , DSP},
        {"memory" , BRAM},
        {"io" , IO},
        {"EMPTY" , EMPTY}
    };

    int id;
    Name name;
    t_physical_tile_type_ptr physical_tile;
    //vector<Resource *> resources;

    SiteType(Name n);
    SiteType(const SiteType &sitetype);
    SiteType(const t_physical_tile_type* tile_type);

    void addResource(Resource *resource);
    void addResource(Resource *resource, int count);

    bool matchInstance(Instance *instance);
};

class Site {
public:
    SiteType *type;
    Pack *pack;
    int x, y;
    int w, h;
    int cur_cluster;//for io delete once bad mergegroup_to_site is deleted
    Site();
    Site(int x, int y, SiteType *sitetype);
    //~Site();
    // center
    inline double cx() { return x + 0.5 * w; }
    inline double cy() { return y + 0.5 * h; }



};

class Pack {
public:
    SiteType *type;
    Site *site;
    vector<Instance *> instances;
    int id;
    void print();
    bool IsEmpty();
    void RemoveInst(Instance *inst);
    Pack();
    Pack(SiteType *type);
};

}  // namespace db