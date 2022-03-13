#include "instance.h"
#include "db.h"

using namespace db;

/***** Master *****/
Master::Name Master::NameString2Enum(const string &name) {
    if (name == "LUT1")
        return LUT1;
    else if (name == "LUT2")
        return LUT2;
    else if (name == "LUT3")
        return LUT3;
    else if (name == "LUT4")
        return LUT4;
    else if (name == "LUT5")
        return LUT5;
    else if (name == "LUT6")
        return LUT6;
    else if (name == "FDRE"||name == ".latch")
        return FDRE;
    else if (name == "CARRY8"||name == "adder")
        return CARRY8;
    else if (name == "DSP48E2"||name=="multiply")
        return DSP48E2;
    else if (name == "RAMB36E2"||name=="single_port_ram")
        return RAMB36E2;
    else if (name == "IBUF"||name == ".input")
        return IBUF;
    else if (name == "OBUF"||name == ".output")
        return OBUF;
    else if (name == "BUFGCE")
        return BUFGCE;
    else {
        cerr << "unknown master name: " << name << endl;
        exit(1);
        //return NULL;
    }
}

string masterNameStrs[] = {
    "LUT1", "LUT2", "LUT3", "LUT4", "LUT5", "LUT6", "FDRE", "CARRY8", "DSP48E2", "RAMB36E2", "IBUF", "OBUF", "BUFGCE"};
string Master::NameEnum2String(const Master::Name &name) { return masterNameStrs[name]; }

Master::Master(Name n) {
    this->name = n;
    this->resource = NULL;
}

Master::Master(const Master &master) {
    name = master.name;
    resource = master.resource;
    pins.resize(master.pins.size());
    for (int i = 0; i < (int)master.pins.size(); i++) {
        pins[i] = new RipplePinType(*master.pins[i]);
    }
}

Master::Master(const t_model* model){
    string model_name = model->name;
    auto iter = model_name_to_master_name.find(model_name);
    if(iter != model_name_to_master_name.end()){
        this->name = iter->second;
    }else{
        this->name = Master::UNKNOWN;
    }
    this->resource = NULL;
    this->vpr_model = model;

    const t_model_ports* cur_port;
    cur_port = model->outputs;
    VTR_ASSERT(model->name && model->name != ".names");
    while (cur_port) {
        string cur_port_name = cur_port->name;
        for (int pini = 0; pini < cur_port->size; pini++) {
            RipplePinType pintype(cur_port_name + "[" + to_string(pini) + "]", 'o');
            this->addPin(pintype);
        }
        cur_port = cur_port->next;
    }
    cur_port = model->inputs;
    while (cur_port) {
        string cur_port_name = cur_port->name;
        char type = (cur_port->is_clock) ? 'c' : 'i';
        for (int pini = 0; pini < cur_port->size; pini++) {
            RipplePinType pintype(cur_port_name + "[" + to_string(pini) + "]", type);
            this->addPin(pintype);
        }
        cur_port = cur_port->next;
    }
}

Master::Master(const t_model* model , int num_input_pins){
    VTR_ASSERT((string)model->name == ".names");
    this->name = lut_master_names[num_input_pins-1];
    this->resource = NULL;
    this->vpr_model = model;
    RipplePinType pintype("out[0]", 'o');
    this->addPin(pintype);
    for (int j = 0; j < num_input_pins; j++) {
        RipplePinType pintype("in[" + to_string(j) + "]", 'i');
        this->addPin(pintype);
    }
}

Master::~Master() {
    for (int i = 0; i < (int)pins.size(); i++) {
        delete pins[i];
    }
}

void Master::addPin(RipplePinType &pin) { pins.push_back(new RipplePinType(pin)); }

RipplePinType *Master::getPin(const string &name) {
    for (int i = 0; i < (int)pins.size(); i++) {
        if (pins[i]->name == name) {
            return pins[i];
        }
    }
    return NULL;
}

/***** Instance *****/
Instance::Instance() {
    id = -1;
    master = NULL;
    pack = NULL;
    //slot = -1;
    fixed = false;
    inputFixed = false;
}

Instance::Instance(const string &name, Master *master) {
    this->id = -1;
    this->name = name;
    this->master = master;
    this->pack = NULL;
    //this->slot = -1;
    this->fixed = false;
    this->inputFixed = false;
    this->pins.resize(master->pins.size());
    for (int i = 0; i < (int)master->pins.size(); i++) {
        this->pins[i] = new Pin(this, i);
    }
}

Instance::Instance(const Instance &instance) {
    vpratomblkid=instance.vpratomblkid;
    id = -1;
    name = instance.name;
    master = instance.master;
    pack = NULL;
    //slot = -1;
    fixed = instance.fixed;
    inputFixed = instance.inputFixed;
    pins.resize(instance.pins.size());
    for (int i = 0; i < (int)instance.pins.size(); i++) {
        // pins[i] = new Pin(*instance.pins[i]);
        pins[i] = new Pin(this, i);
    }
}

Instance::Instance(const string &name, Master *master,AtomBlockId &vpratomblkid) {
    this->vpratomblkid=vpratomblkid;
    this->id = -1;
    this->name = name;
    this->master = master;
    this->pack = NULL;
    //this->slot = -1;
    this->fixed = false;
    this->inputFixed = false;
    this->pins.resize(master->pins.size());
    for (int i = 0; i < (int)master->pins.size(); i++) {
        this->pins[i] = new Pin(this, i);
    }
}

Instance::~Instance() {
    for (int i = 0; i < (int)pins.size(); i++) {
        delete pins[i];
    }
}

Pin *Instance::getPin(const string &name) {
    for (int i = 0; i < (int)pins.size(); i++) {
        if (pins[i]->type->name == name) {
            return pins[i];
        }
    }
    return NULL;
}

bool Instance::matchSiteType(SiteType *type) { return type->matchInstance(this); }
