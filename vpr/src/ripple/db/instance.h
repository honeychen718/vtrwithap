#pragma once

#include "../global.h"
#include "site.h"

namespace db {

class RipplePinType;
class Pin;
class Resource;
class SiteType;
class Pack;

// TODO: rename to CellType
class Master {
public:
    enum Name {
        LUT1,
        LUT2,
        LUT3,
        LUT4,
        LUT5,
        LUT6,      // LUT
        FDRE,      // FF
        CARRY8,    // CARRY
        DSP48E2,   // DSP
        RAMB36E2,  // RAM
        IBUF,
        OBUF,
        BUFGCE,  // IO: input buf, output buf, clock buf
        UNKNOWN
    };
    static Name NameString2Enum(const string &name);
    static string NameEnum2String(const Master::Name &name);
    unordered_map<string , Master::Name> model_name_to_master_name{//all possible model name in vpr arch ,should update constantly
        {"adder" , CARRY8},
        {"multiply" , DSP48E2},
        {"single_port_ram" , RAMB36E2},
        {"dual_port_ram" , RAMB36E2},
        {".input" , IBUF},
        {".output" , OBUF},
        {".latch" , FDRE}
    };
    vector<Master::Name> lut_master_names{
        LUT1,
        LUT2,
        LUT3,
        LUT4,
        LUT5,
        LUT6};

    Name name;
    const t_model *vpr_model;
    Resource *resource;
    vector<RipplePinType *> pins;

    Master(Name n);
    Master(const Master &master);
    Master(const t_model* model);
    Master(const t_model* model , int num_input_pins);
    ~Master();

    void addPin(RipplePinType &pin);
    RipplePinType *getPin(const string &name);
};

// TODO: rename to Cell
class Instance {
public:
    //t_pack_molecule* vpr_molecule;
    AtomBlockId vpratomblkid;//add by jia
    int id;
    string name;
    Master *master;
    Pack *pack;
    //int slot;
    bool fixed;
    bool inputFixed;
    vector<Pin *> pins;

    Instance();
    Instance(const string &name, Master *master);
    Instance(const Instance &instance);
    Instance(const string &name, Master *master ,AtomBlockId &vpratomblkid);
    ~Instance();

    inline bool IsLUT() { return master->resource->name == Resource::LUT; }
    inline bool IsFF() { return master->name == Master::FDRE; }
    inline bool IsCARRY() { return master->name == Master::CARRY8; }
    inline bool IsLUTFF() { return IsLUT() || IsFF(); }  // TODO: check CARRY8
    inline bool IsDSP() { return master->name == Master::DSP48E2; }
    inline bool IsRAM() { return master->name == Master::RAMB36E2; }
    inline bool IsDSPRAM() { return IsDSP() || IsRAM(); }
    inline bool IsIO() { return master->resource->name == Resource::IO; }

    Pin *getPin(const string &name);
    bool matchSiteType(SiteType *type);
};

}  // namespace db