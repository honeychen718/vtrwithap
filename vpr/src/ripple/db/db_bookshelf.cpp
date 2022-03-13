#include "db.h"
#include "SetupGrid.h"
#include "gp_setting.h"
using namespace db;

void bilf_models_in_pb_type(const t_pb_type* pb_type, map<string, int>& blif_models_and_num);
bool pb_type_contains_blif_model(const t_pb_type* pb_type, const std::string& blif_model_name, int& capacity);

bool read_line_as_tokens(istream& is, vector<string>& tokens) {
    tokens.clear();
    string line;
    getline(is, line);
    while (is && tokens.empty()) {
        string token = "";
        for (unsigned i = 0; i < line.size(); ++i) {
            char currChar = line[i];
            if (isspace(currChar)) {
                if (!token.empty()) {
                    // Add the current token to the list of tokens
                    tokens.push_back(token);
                    token.clear();
                }
            } else {
                // Add the char to the current token
                token.push_back(currChar);
            }
        }
        if (!token.empty()) {
            tokens.push_back(token);
        }
        if (tokens.empty()) {
            // Previous line read was empty. Read the next one.
            getline(is, line);
        }
    }
    return !tokens.empty();
}

bool Database::readAux(string file) {
    string directory;
    size_t found = file.find_last_of("/\\");
    if (found == file.npos) {
        directory = "./";
    } else {
        directory = file.substr(0, found);
        directory += "/";
    }

    ifstream fs(file);
    if (!fs.good()) {
        printlog(LOG_ERROR, "Cannot open %s to read", file.c_str());
        return false;
    }

    string buffer;
    while (fs >> buffer) {
        if (buffer == "#") {
            //ignore comments
            fs.ignore(std::numeric_limits<streamsize>::max(), '\n');
            continue;
        }
        size_t dot_pos = buffer.find_last_of(".");
        if (dot_pos == string::npos) {
            //ignore words without "dot"
            continue;
        }
        string ext = buffer.substr(dot_pos);
        if (ext == ".nodes") {
            setting.io_nodes = directory + buffer;
        } else if (ext == ".nets") {
            setting.io_nets = directory + buffer;
        } else if (ext == ".pl") {
            setting.io_pl = directory + buffer;
        } else if (ext == ".wts") {
            setting.io_wts = directory + buffer;
        } else if (ext == ".scl") {
            setting.io_scl = directory + buffer;
        } else if (ext == ".lib") {
            setting.io_lib = directory + buffer;
        } else {
            continue;
        }
    }

    printlog(LOG_VERBOSE, "nodes = %s", setting.io_nodes.c_str());
    printlog(LOG_VERBOSE, "nets  = %s", setting.io_nets.c_str());
    printlog(LOG_VERBOSE, "pl    = %s", setting.io_pl.c_str());
    printlog(LOG_VERBOSE, "wts   = %s", setting.io_wts.c_str());
    printlog(LOG_VERBOSE, "scl   = %s", setting.io_scl.c_str());
    printlog(LOG_VERBOSE, "lib   = %s", setting.io_lib.c_str());
    fs.close();

    return true;
}

bool Database::alloc_and_load_Instances(){
    auto& atom_ctx = g_vpr_ctx.atom();
    auto blocks = atom_ctx.nlist.blocks();
    for (auto blk_iter = blocks.begin(); blk_iter != blocks.end(); ++blk_iter) {
        auto blk_id = *blk_iter;
        auto model = atom_ctx.nlist.block_model(blk_id);
        string block_name = atom_ctx.nlist.block_name(blk_id);
        Instance* instance = database.getInstance(block_name);
        if (instance != NULL) {
            printlog(LOG_ERROR, "Instance duplicated: %s", block_name.c_str());
        } else {
            Master* master = NULL;
            if((string)model->name == ".names"){
                int num_input_pins = atom_ctx.nlist.block_input_pins(blk_id).size();
                master = database.getMaster(model , num_input_pins);
            }else{
                master = database.getMaster(model);
            }
            
            if (master == NULL) {
                printlog(LOG_ERROR, "Master not found: %s", model->name);
            } else {
                Instance newinstance(block_name, master ,blk_id);
                instance = database.addInstance(newinstance);
            }
        }
    }
}

bool Database::readNodes(string file) {
    ifstream fs(file);
    if (!fs.good()) {
        cerr << "Read nodes file: " << file << " fail" << endl;
        return false;
    }

    vector<string> tokens;
    while (read_line_as_tokens(fs, tokens)) {
        Instance* instance = database.getInstance(tokens[0]);
        if (instance != NULL) {
            printlog(LOG_ERROR, "Instance duplicated: %s", tokens[0].c_str());
        } else {
            Master* master = database.getMaster(Master::NameString2Enum(tokens[1]));
            if (master == NULL) {
                printlog(LOG_ERROR, "Master not found: %s", tokens[1].c_str());
            } else {
                Instance newinstance(tokens[0], master);
                instance = database.addInstance(newinstance);
            }
        }
    }

    fs.close();
    return true;
}

bool Database::alloc_and_load_Nets(){
    auto& atom_ctx = g_vpr_ctx.atom();
    auto nets = atom_ctx.nlist.nets();
    Net* net = NULL;
    for (auto net_iter = nets.begin(); net_iter != nets.end(); ++net_iter) {
        string net_name = atom_ctx.nlist.net_name(*net_iter);
        //cout<<"net:\t"<<net_name<<endl;
        net = database.getNet(net_name);
        if (net != NULL) {
            printlog(LOG_ERROR, "Net duplicated: %s", net_name.c_str());
        } else {
            Net newnet(net_name);
            net = database.addNet(newnet);
        }
        auto net_pins = atom_ctx.nlist.net_pins(*net_iter);
        for (auto pin_iter = net_pins.begin(); pin_iter != net_pins.end(); ++pin_iter) {
            auto pin_block = atom_ctx.nlist.pin_block(*pin_iter);
            auto model = atom_ctx.nlist.block_model(pin_block);
            string block_name = atom_ctx.nlist.block_name(pin_block);
            string pin_full_name = atom_ctx.nlist.pin_name(*pin_iter);
            size_t dot_pos = pin_full_name.find_last_of(".");
            string pin_name = pin_full_name.substr(dot_pos + 1);
            Instance* instance = database.getInstance(block_name.c_str());
            Pin* pin = NULL;
            if (instance != NULL) {
                pin = instance->getPin(pin_name);
            } else {
                printlog(LOG_ERROR, "Instance not found: %s", pin_name.c_str());
            }
            if (pin == NULL) {
                printlog(LOG_ERROR, "Pin not found: %s", pin_name.c_str());
            } else {
                net->addPin(pin);
                if (pin->type->type == 'o' && pin->instance->master->name == Master::BUFGCE) {
                    net->isClk = true;
                }
            }
            //cout<<block_name<<"\t"<<atom_ctx.nlist.pin_name(*pin_iter)<<endl;
        }
        //cout<<"\n"<<endl;
    }
}

bool Database::readNets(string file) {
    ifstream fs(file);
    if (!fs.good()) {
        cerr << "Read net file: " << file << " fail" << endl;
        return false;
    }

    vector<string> tokens;
    Net* net = NULL;
    while (read_line_as_tokens(fs, tokens)) {
        if (tokens[0][0] == '#') {
            continue;
        }
        if (tokens[0] == "net") {
            net = database.getNet(tokens[1]);
            if (net != NULL) {
                printlog(LOG_ERROR, "Net duplicated: %s", tokens[1].c_str());
            } else {
                Net newnet(tokens[1]);
                net = database.addNet(newnet);
            }
        } else if (tokens[0] == "endnet") {
            net = NULL;
        } else {
            Instance* instance = database.getInstance(tokens[0].c_str());
            Pin* pin = NULL;

            if (instance != NULL) {
                pin = instance->getPin(tokens[1]);
            } else {
                printlog(LOG_ERROR, "Instance not found: %s", tokens[0].c_str());
            }

            if (pin == NULL) {
                printlog(LOG_ERROR, "Pin not found: %s", tokens[1].c_str());
            } else {
                net->addPin(pin);
                if (pin->type->type == 'o' && pin->instance->master->name == Master::BUFGCE) {
                    net->isClk = true;
                }
            }
        }
    }

    fs.close();
    return false;
}

bool Database::alloc_and_load_Sites(){
    auto& device_ctx = g_vpr_ctx.mutable_device();
    device_ctx.grid = create_device_grid("ripple", database.arch->grid_layouts);
    //auto &grid=device_ctx.grid;
    //cout<<device_ctx.grid.width()<<"\t"<<device_ctx.grid.height()<<"\t"<<endl;
    int nx = device_ctx.grid.width();
    int ny = device_ctx.grid.height();
    database.setSiteMap(nx, ny);
    database.setSwitchBoxes(nx - 1, ny - 1);
    for (int x = 0; x < nx; x++) {
        for (int y = 0; y < ny; y++) {
            //cout<<i<<"\t"<<j<<"\t"<<device_ctx.grid[i][j].type->name<<endl;
            string site_name = device_ctx.grid[x][y].type->name;
            SiteType* sitetype = database.getSiteType(SiteType::NameString2Enum(site_name));
            if (sitetype == NULL) {
                printlog(LOG_ERROR, "Site type not found: %s", site_name.c_str());
            } else {
                database.addSite(x, y, sitetype);
            }
        }
    }
}

bool Database::readPl(string file) {
    ifstream fs(file);
    if (!fs.good()) {
        cerr << "Read pl file: " << file << " fail" << endl;
        return false;
    }
    vector<string> tokens;
    while (read_line_as_tokens(fs, tokens)) {
        Instance* instance = database.getInstance(tokens[0]);
        if (instance == NULL) {
            printlog(LOG_ERROR, "Instance not found: %s", tokens[0].c_str());
            continue;
        }
        int x = atoi(tokens[1].c_str());
        int y = atoi(tokens[2].c_str());
        int slot = atoi(tokens[3].c_str());
        instance->inputFixed = (tokens.size() >= 5 && tokens[4] == "FIXED");
        instance->fixed = instance->inputFixed;
        if (instance->IsFF())
            slot += 16;
        else if (instance->IsCARRY())
            slot += 32;
        place(instance, x, y);
    }
    fs.close();
    return true;
}

bool Database::alloc_and_load_Resources(){
    primitive_candidate_block_types = identify_primitive_candidate_block_types();
    auto& device_ctx = g_vpr_ctx.mutable_device();
    for (auto const& tile : device_ctx.physical_tile_types) {
        const t_physical_tile_type* physical_tile = &tile;
        SiteType* sitetype = database.getSiteType(physical_tile);
        if (sitetype == NULL) {
            SiteType* newsitetype = new SiteType(physical_tile);
            sitetypes.push_back(newsitetype);
        } else {
            printlog(LOG_WARN, "Duplicated site type: %s", sitetype->name);
        }

        //write dsp&ram area in GPsetting
        if (sitetype->name == SiteType::DSP) {
            gpSetting.dspArea = tile.height * tile.width;
        }

        if (sitetype->name == SiteType::BRAM) {
            gpSetting.ramArea = tile.height * tile.width;
        }
        //
        assert(tile.sub_tiles.size()<=1);
        for (auto const& sub_tile : tile.sub_tiles) {
            int const sub_tile_capacity = sub_tile.capacity.total();
            database.subtile_capacity[sitetype]=sub_tile_capacity;
            for (auto const& pb_type : sub_tile.equivalent_sites) {
                //cout<<equivalent_site->name<<endl;
                t_model* cur_model = vpr_setup->user_models;
                while (cur_model) {
                    int capacity = 1;
                    //string model_name=cur_model->name;
                    if (pb_type_contains_blif_model(pb_type->pb_type, cur_model->name, capacity)) {
                        capacity *= sub_tile_capacity;
                        string resource_name = cur_model->name;
                        Resource* resource = database.getResource(Resource::NameString2Enum(resource_name));
                        if (resource == NULL) {
                            Resource newresource(Resource::NameString2Enum(resource_name));
                            resource = database.addResource(newresource);
                        }
                        sitetype->addResource(resource, capacity);
                        Master* master = database.getMaster(cur_model);
                        if (master == NULL) {
                            printlog(LOG_ERROR, "Master not found: %s", cur_model->name);
                        } else {
                            resource->addMaster(master);
                        }
                    }
                    cur_model = cur_model->next;
                }
                cur_model = vpr_setup->library_models;
                bool has_io_resource_added = false;
                while (cur_model) {
                    int capacity = 1;
                    //string model_name=cur_model->name;
                    string model_name = cur_model->name;
                    if (pb_type_contains_blif_model(pb_type->pb_type, cur_model->name, capacity)) {
                        capacity *= sub_tile_capacity;
                        if (model_name == ".input" || model_name == ".output") {
                            Resource* resource = database.getResource(Resource::NameString2Enum("IO"));
                            if (resource == NULL) {
                                Resource newresource(Resource::NameString2Enum("IO"));
                                resource = database.addResource(newresource);
                            }
                            if (!has_io_resource_added) {
                                sitetype->addResource(resource, capacity);
                                has_io_resource_added = true;
                            }
                            Master* master = database.getMaster(cur_model);
                            if (master == NULL) {
                                printlog(LOG_ERROR, "Master not found: %s", cur_model->name);
                            } else {
                                resource->addMaster(master);
                            }
                        } else if (model_name == ".names") {
                            Resource* resource = database.getResource(Resource::NameString2Enum("LUT"));
                            if (resource == NULL) {
                                Resource newresource(Resource::NameString2Enum("LUT"));
                                resource = database.addResource(newresource);
                            }
                            sitetype->addResource(resource, capacity);
                            for (int i = 6; i > 0; i--) {
                                string lut_name = "LUT" + to_string(i);
                                Master* master = database.getMaster(Master::NameString2Enum(lut_name));
                                if (master == NULL) {
                                    printlog(LOG_ERROR, "Master not found: %s", cur_model->name);
                                } else {
                                    resource->addMaster(master);
                                }
                            }
                            //write lutPerSlice in gpsettings
                            lutPerSlice = capacity * 0.75;

                        } else if (model_name == ".latch") {
                            Resource* resource = database.getResource(Resource::NameString2Enum("FF"));
                            if (resource == NULL) {
                                Resource newresource(Resource::NameString2Enum("FF"));
                                resource = database.addResource(newresource);
                            }
                            sitetype->addResource(resource, capacity);
                            Master* master = database.getMaster(Master::NameString2Enum(cur_model->name));
                            if (master == NULL) {
                                printlog(LOG_ERROR, "Master not found: %s", cur_model->name);
                            } else {
                                resource->addMaster(master);
                            }
                            ffPerSlice = capacity * 0.75;
                        }
                    }
                    cur_model = cur_model->next;
                }
            }
        }
    }

}

bool Database::readScl(string file) {
    ifstream fs(file);
    if (!fs.good()) {
        printlog(LOG_ERROR, "Cannot open %s to read", file.c_str());
        return false;
    }
    vector<string> tokens;
    while (read_line_as_tokens(fs, tokens)) {
        if (tokens[0] == "SITE") {
            SiteType* sitetype = database.getSiteType(SiteType::NameString2Enum(tokens[1]));
            if (sitetype == NULL) {
                SiteType newsitetype(SiteType::NameString2Enum(tokens[1]));
                sitetype = database.addSiteType(newsitetype);
            } else {
                printlog(LOG_WARN, "Duplicated site type: %s", sitetype->name);
            }
            while (read_line_as_tokens(fs, tokens)) {
                if (tokens[0] == "END" && tokens[1] == "SITE") {
                    break;
                }
                Resource* resource = database.getResource(Resource::NameString2Enum(tokens[0]));
                if (resource == NULL) {
                    Resource newresource(Resource::NameString2Enum(tokens[0]));
                    resource = database.addResource(newresource);
                }
                sitetype->addResource(resource, atoi(tokens[1].c_str()));
            }
        }
        if (tokens[0] == "RESOURCES") {
            while (read_line_as_tokens(fs, tokens)) {
                if (tokens[0] == "END" && tokens[1] == "RESOURCES") {
                    break;
                }
                Resource* resource = database.getResource(Resource::NameString2Enum(tokens[0]));
                if (resource == NULL) {
                    Resource newresource(Resource::NameString2Enum(tokens[0]));
                    resource = database.addResource(newresource);
                }
                for (int i = 1; i < (int)tokens.size(); i++) {
                    Master* master = database.getMaster(Master::NameString2Enum(tokens[i]));
                    if (master == NULL) {
                        printlog(LOG_ERROR, "Master not found: %s", tokens[i].c_str());
                    } else {
                        resource->addMaster(master);
                    }
                }
            }
        }
        if (tokens[0] == "SITEMAP") {
            int nx = atoi(tokens[1].c_str());
            int ny = atoi(tokens[2].c_str());
            database.setSiteMap(nx, ny);
            database.setSwitchBoxes(nx / 2 - 1, ny);
            while (read_line_as_tokens(fs, tokens)) {
                if (tokens[0] == "END" && tokens[1] == "SITEMAP") {
                    break;
                }
                int x = atoi(tokens[0].c_str());
                int y = atoi(tokens[1].c_str());
                SiteType* sitetype = database.getSiteType(SiteType::NameString2Enum(tokens[2]));
                if (sitetype == NULL) {
                    printlog(LOG_ERROR, "Site type not found: %s", tokens[2].c_str());
                } else {
                    database.addSite(x, y, sitetype);
                }
            }
        }
        if (tokens[0] == "CLOCKREGIONS") {
            int nx = atoi(tokens[1].c_str());
            int ny = atoi(tokens[2].c_str());

            crmap_nx = nx;
            crmap_ny = ny;
            clkrgns.assign(nx, vector<ClkRgn*>(ny, NULL));
            hfcols.assign(sitemap_nx, vector<HfCol*>(2 * ny, NULL));

            for (int x = 0; x < nx; x++) {
                for (int y = 0; y < ny; y++) {
                    read_line_as_tokens(fs, tokens);
                    string name = tokens[0] + tokens[1];
                    int lx = atoi(tokens[3].c_str());
                    int ly = atoi(tokens[4].c_str());
                    int hx = atoi(tokens[5].c_str());
                    int hy = atoi(tokens[6].c_str());
                    clkrgns[x][y] = new ClkRgn(name, lx, ly, hx, hy, x, y);
                }
            }
        }
    }

    fs.close();
    return true;
}

bool Database::alloc_and_load_Masters(){
    Master* master = NULL;
    const t_model* cur_model;
    cur_model = vpr_setup->user_models;
    while (cur_model) {
        master = database.getMaster(cur_model);
        if (master == NULL) {
            Master* master= new Master(cur_model);
            masters.push_back(master);
        } else {
            printlog(LOG_WARN, "Duplicated master: %s",cur_model->name);
        }
        cur_model = cur_model->next;
    }

    cur_model = vpr_setup->library_models;
    while (cur_model) {
        string cur_model_name = cur_model->name;
        if (cur_model_name == ".names") {
            //////alloc_and_add_lut_Masters();
            for (int i = 6; i > 0; i--) {
                master = database.getMaster(cur_model , i);
                if (master == NULL) {
                    Master* newmaster = new Master(cur_model , i);
                    masters.push_back(newmaster);
                } else {
                    printlog(LOG_WARN, "Duplicated master: %s", cur_model_name.c_str());
                }
            }
            cur_model = cur_model->next;
            continue;
        }
        master = database.getMaster(cur_model);
        if (master == NULL) {
            Master* master= new Master(cur_model);
            masters.push_back(master);
        } else {
            printlog(LOG_WARN, "Duplicated master: %s", cur_model_name.c_str());
        }
        cur_model = cur_model->next;
    }
}

bool Database::readLib(string file) {
    ifstream fs(file);
    if (!fs.good()) {
        printlog(LOG_ERROR, "Cannot open %s to read", file.c_str());
        return false;
    }

    vector<string> tokens;
    Master* master = NULL;
    while (read_line_as_tokens(fs, tokens)) {
        if (tokens[0] == "CELL") {
            master = database.getMaster(Master::NameString2Enum(tokens[1]));
            if (master == NULL) {
                Master newmaster(Master::NameString2Enum(tokens[1]));
                master = database.addMaster(newmaster);
            } else {
                printlog(LOG_WARN, "Duplicated master: %s", tokens[1].c_str());
            }
        } else if (tokens[0] == "PIN") {
            char type = 'x';
            if (tokens.size() >= 4) {
                if (tokens[3] == "CLOCK") {
                    type = 'c';
                } else if (tokens[3] == "CTRL") {
                    type = 'e';
                }
            } else if (tokens.size() >= 3) {
                if (tokens[2] == "OUTPUT") {
                    type = 'o';
                } else if (tokens[2] == "INPUT") {
                    type = 'i';
                }
            }

            RipplePinType pintype(tokens[1], type);
            if (master != NULL) {
                master->addPin(pintype);
            }
        } else if (tokens[0] == "END") {
        }
    }

    fs.close();
    return true;
}

bool Database::readWts(string file) {
    return false;
}

bool Database::writePl(string file) {
    // ofstream fs(file);
    // if (!fs.good()) {
    //     printlog(LOG_ERROR, "Cannot open %s to write", file.c_str());
    //     return false;
    // }

    // vector<Instance*>::iterator ii = database.instances.begin();
    // vector<Instance*>::iterator ie = database.instances.end();
    // int nErrorLimit = 10;
    // for (; ii != ie; ++ii) {
    //     Instance* instance = *ii;
    //     if (instance->pack == NULL || instance->pack->site == NULL) {
    //         if (nErrorLimit > 0) {
    //             printlog(LOG_ERROR, "Instance not placed: %s", instance->name.c_str());
    //             nErrorLimit--;
    //         } else if (nErrorLimit == 0) {
    //             printlog(LOG_ERROR, "(Remaining same errors are not shown)");
    //             nErrorLimit--;
    //         }
    //         continue;
    //     }
    //     Site* site = instance->pack->site;
    //     int slot = instance->slot;
    //     if (instance->IsFF())
    //         slot -= 16;
    //     else if (instance->IsCARRY())
    //         slot -= 32;
    //     fs << instance->name << " " << site->x << " " << site->y << " " << slot;
    //     if (instance->inputFixed) fs << " FIXED";
    //     fs << endl;
    // }

    return true;
}

bool Database::read_from_vpr_database(){
    alloc_and_load_Masters();
    alloc_and_load_Instances();
    alloc_and_load_Resources();
    alloc_and_load_Sites();
    alloc_and_load_Nets();
}

// vector<string> modelname_to_mastername(string modelname){
//     if (modelname==""
// }

bool pb_type_contains_blif_model(const t_pb_type* pb_type, const std::string& blif_model_name, int& capacity) {
    if (!pb_type) {
        return false;
    }

    if (pb_type->blif_model != nullptr) {
        //Leaf pb_type
        VTR_ASSERT(pb_type->num_modes == 0);
        if (blif_model_name == pb_type->blif_model
            || ".subckt " + blif_model_name == pb_type->blif_model) {
            return true;
        } else {
            return false;
        }
    } else {
        for (int imode = 0; imode < pb_type->num_modes; ++imode) {
            const t_mode* mode = &pb_type->modes[imode];
            for (int ichild = 0; ichild < mode->num_pb_type_children; ++ichild) {
                const t_pb_type* pb_type_child = &mode->pb_type_children[ichild];
                if (pb_type_contains_blif_model(pb_type_child, blif_model_name, capacity)) {
                    capacity *= pb_type_child->num_pb;
                    return true;
                }
            }
        }
    }
    return false;
}

void bilf_models_in_pb_type(const t_pb_type* pb_type, map<string, int>& blif_models_and_num) {
    if (!pb_type) {
        return;
    }
    if (pb_type->blif_model != nullptr) {
        //Leaf pb_type
        VTR_ASSERT(pb_type->num_modes == 0);
        string blif_model_name = pb_type->blif_model;
        string::size_type idx;
        idx = blif_model_name.find(".subckt ");
        if (idx == string::npos) {
            blif_model_name.replace(idx, sizeof(".subckt "), "");
        }
        blif_models_and_num[blif_model_name] = 1;
    }
    for (int imode = 0; imode < pb_type->num_modes; ++imode) {
        const t_mode* mode = &pb_type->modes[imode];
        for (int ichild = 0; ichild < mode->num_pb_type_children; ++ichild) {
            const t_pb_type* pb_type_child = &mode->pb_type_children[ichild];
            bilf_models_in_pb_type(pb_type_child, blif_models_and_num);
            map<string, int>::iterator iter;
            for (iter = blif_models_and_num.begin(); iter != blif_models_and_num.end(); iter++) {
                iter->second *= pb_type_child->num_pb;
            }
        }
    }
}

bool Database::readArch() {
    /*
    readlib
    */
    
    Master* master = NULL;
    const t_model* cur_model;
    cur_model = vpr_setup->user_models;
    while (cur_model) {
        string cur_model_name;
        cur_model_name = cur_model->name;
        master = database.getMaster(Master::NameString2Enum(cur_model_name));
        if (master == NULL) {
            Master newmaster(Master::NameString2Enum(cur_model_name));
            master = database.addMaster(newmaster);
        } else {
            printlog(LOG_WARN, "Duplicated master: %s", cur_model_name.c_str());
        }

        const t_model_ports* cur_port;
        cur_port = cur_model->outputs;
        while (cur_port) {
            string cur_port_name = cur_port->name;
            for (int pini = 0; pini < cur_port->size; pini++) {
                RipplePinType pintype(cur_port_name + "[" + to_string(pini) + "]", 'o');
                if (master != NULL) {
                    master->addPin(pintype);
                }
            }
            cur_port = cur_port->next;
        }
        cur_port = cur_model->inputs;
        while (cur_port) {
            string cur_port_name = cur_port->name;
            char type = (cur_port->is_clock) ? 'c' : 'i';
            for (int pini = 0; pini < cur_port->size; pini++) {
                RipplePinType pintype(cur_port_name + "[" + to_string(pini) + "]", type);
                if (master != NULL) {
                    master->addPin(pintype);
                }
            }
            cur_port = cur_port->next;
        }
        cur_model = cur_model->next;
        num_models++;
    }

    cur_model = vpr_setup->library_models;
    while (cur_model) {
        string cur_model_name;
        cur_model_name = cur_model->name;
        if (cur_model_name == ".names") {
            for (int i = 6; i > 0; i--) {
                cur_model_name = "LUT" + to_string(i);
                master = database.getMaster(Master::NameString2Enum(cur_model_name));
                if (master == NULL) {
                    Master newmaster(Master::NameString2Enum(cur_model_name));
                    master = database.addMaster(newmaster);
                } else {
                    printlog(LOG_WARN, "Duplicated master: %s", cur_model_name.c_str());
                }
                string lut_out_pin_name = "out[0]";
                RipplePinType pintype(lut_out_pin_name, 'o');
                if (master != NULL) {
                    master->addPin(pintype);
                }
                string lut_in_port_name = "in";
                for (int j = 0; j < i; j++) {
                    RipplePinType pintype(lut_in_port_name + "[" + to_string(j) + "]", 'i');
                    if (master != NULL) {
                        master->addPin(pintype);
                    }
                }
            }
            cur_model = cur_model->next;
            continue;
        }
        master = database.getMaster(Master::NameString2Enum(cur_model_name));
        if (master == NULL) {
            Master newmaster(Master::NameString2Enum(cur_model_name));
            master = database.addMaster(newmaster);
        } else {
            printlog(LOG_WARN, "Duplicated master: %s", cur_model_name.c_str());
        }

        const t_model_ports* cur_port;
        cur_port = cur_model->outputs;
        while (cur_port) {
            string cur_port_name = cur_port->name;
            for (int pini = 0; pini < cur_port->size; pini++) {
                RipplePinType pintype(cur_port_name + "[" + to_string(pini) + "]", 'o');
                if (master != NULL) {
                    master->addPin(pintype);
                }
            }
            cur_port = cur_port->next;
        }
        cur_port = cur_model->inputs;
        while (cur_port) {
            string cur_port_name = cur_port->name;
            char type = (cur_port->is_clock) ? 'c' : 'i';
            for (int pini = 0; pini < cur_port->size; pini++) {
                RipplePinType pintype(cur_port_name + "[" + to_string(pini) + "]", type);
                if (master != NULL) {
                    master->addPin(pintype);
                }
            }
            cur_port = cur_port->next;
        }
        cur_model = cur_model->next;
        num_models++;
    }
    /*
    readlib
    */

    /*
    readnodes
    */
    //std::set<const t_model*> unique_models;// the set of models , for readscl!!
    auto& atom_ctx = g_vpr_ctx.atom();
    auto blocks = atom_ctx.nlist.blocks();
    for (auto blk_iter = blocks.begin(); blk_iter != blocks.end(); ++blk_iter) {
        auto blk_id = *blk_iter;
        auto model = atom_ctx.nlist.block_model(blk_id);
        //unique_models.insert(model);// the set of models , for readscl!!
        //size_t blk_int_id = size_t(blk_id);
        //string inst_name="inst_" + to_string(blk_int_id);
        //cout<<inst_name<<endl;
        string block_name = atom_ctx.nlist.block_name(blk_id);
        string model_name = model->name;
        if (model_name == ".names") {
            int lut_input_num = atom_ctx.nlist.block_input_pins(blk_id).size();
            model_name = "LUT" + to_string(lut_input_num);
            if (lut_input_num == 0) {
                model_name = "LUT1";
            }
        }
        //Instance *instance = database.getInstance(inst_name);
        Instance* instance = database.getInstance(block_name);
        //cout<<block_name<<endl;
        if (instance != NULL) {
            //printlog(LOG_ERROR, "Instance duplicated: %s", inst_name.c_str());
            printlog(LOG_ERROR, "Instance duplicated: %s", block_name.c_str());
        } else {
            Master* master = database.getMaster(Master::NameString2Enum(model_name));
            if (master == NULL) {
                printlog(LOG_ERROR, "Master not found: %s", model_name.c_str());
            } else {
                //Instance newinstance(inst_name, master);
                Instance newinstance(block_name, master ,blk_id);
                instance = database.addInstance(newinstance);
            }
        }
    }
    /*
    readnodes
    */

    /*
    readscl
    */
    auto& device_ctx = g_vpr_ctx.mutable_device();
    for (auto const& tile : device_ctx.physical_tile_types) {
        string tile_name = tile.name;
        SiteType* sitetype = database.getSiteType(SiteType::NameString2Enum(tile_name));
        if (sitetype == NULL) {
            SiteType newsitetype(SiteType::NameString2Enum(tile_name));
            sitetype = database.addSiteType(newsitetype);
        } else {
            printlog(LOG_WARN, "Duplicated site type: %s", sitetype->name);
        }

        //write dsp&ram area in GPsetting
        if (sitetype->name == SiteType::DSP) {
            gpSetting.dspArea = tile.height * tile.width;
        }

        if (sitetype->name == SiteType::BRAM) {
            gpSetting.ramArea = tile.height * tile.width;
        }
        //
        assert(tile.sub_tiles.size()<=1);
        for (auto const& sub_tile : tile.sub_tiles) {
            int const sub_tile_capacity = sub_tile.capacity.total();
            database.subtile_capacity[sitetype]=sub_tile_capacity;
            for (auto const& pb_type : sub_tile.equivalent_sites) {
                //cout<<equivalent_site->name<<endl;
                t_model* cur_model = vpr_setup->user_models;
                while (cur_model) {
                    int capacity = 1;
                    //string model_name=cur_model->name;
                    if (pb_type_contains_blif_model(pb_type->pb_type, cur_model->name, capacity)) {
                        capacity *= sub_tile_capacity;
                        string resource_name = cur_model->name;
                        Resource* resource = database.getResource(Resource::NameString2Enum(resource_name));
                        if (resource == NULL) {
                            Resource newresource(Resource::NameString2Enum(resource_name));
                            resource = database.addResource(newresource);
                        }
                        sitetype->addResource(resource, capacity);
                        Master* master = database.getMaster(Master::NameString2Enum(cur_model->name));
                        if (master == NULL) {
                            printlog(LOG_ERROR, "Master not found: %s", cur_model->name);
                        } else {
                            resource->addMaster(master);
                        }
                    }
                    cur_model = cur_model->next;
                }
                cur_model = vpr_setup->library_models;
                bool has_io_resource_added = false;
                while (cur_model) {
                    int capacity = 1;
                    //string model_name=cur_model->name;
                    string model_name = cur_model->name;
                    if (pb_type_contains_blif_model(pb_type->pb_type, cur_model->name, capacity)) {
                        capacity *= sub_tile_capacity;
                        if (model_name == ".input" || model_name == ".output") {
                            Resource* resource = database.getResource(Resource::NameString2Enum("IO"));
                            if (resource == NULL) {
                                Resource newresource(Resource::NameString2Enum("IO"));
                                resource = database.addResource(newresource);
                            }
                            if (!has_io_resource_added) {
                                sitetype->addResource(resource, capacity);
                                has_io_resource_added = true;
                            }
                            Master* master = database.getMaster(Master::NameString2Enum(cur_model->name));
                            if (master == NULL) {
                                printlog(LOG_ERROR, "Master not found: %s", cur_model->name);
                            } else {
                                resource->addMaster(master);
                            }
                        } else if (model_name == ".names") {
                            Resource* resource = database.getResource(Resource::NameString2Enum("LUT"));
                            if (resource == NULL) {
                                Resource newresource(Resource::NameString2Enum("LUT"));
                                resource = database.addResource(newresource);
                            }
                            sitetype->addResource(resource, capacity);
                            for (int i = 6; i > 0; i--) {
                                string lut_name = "LUT" + to_string(i);
                                Master* master = database.getMaster(Master::NameString2Enum(lut_name));
                                if (master == NULL) {
                                    printlog(LOG_ERROR, "Master not found: %s", cur_model->name);
                                } else {
                                    resource->addMaster(master);
                                }
                            }
                            //write lutPerSlice in gpsettings
                            lutPerSlice = capacity * 0.75;

                        } else if (model_name == ".latch") {
                            Resource* resource = database.getResource(Resource::NameString2Enum("FF"));
                            if (resource == NULL) {
                                Resource newresource(Resource::NameString2Enum("FF"));
                                resource = database.addResource(newresource);
                            }
                            sitetype->addResource(resource, capacity);
                            Master* master = database.getMaster(Master::NameString2Enum(cur_model->name));
                            if (master == NULL) {
                                printlog(LOG_ERROR, "Master not found: %s", cur_model->name);
                            } else {
                                resource->addMaster(master);
                            }
                            ffPerSlice = capacity * 0.75;
                        }
                    }
                    cur_model = cur_model->next;
                }
            }
        }
    }

    device_ctx.grid = create_device_grid("ripple", database.arch->grid_layouts);
    //auto &grid=device_ctx.grid;
    //cout<<device_ctx.grid.width()<<"\t"<<device_ctx.grid.height()<<"\t"<<endl;
    int nx = device_ctx.grid.width();
    int ny = device_ctx.grid.height();
    database.setSiteMap(nx, ny);
    database.setSwitchBoxes(nx - 1, ny - 1);
    for (int x = 0; x < nx; x++) {
        for (int y = 0; y < ny; y++) {
            //cout<<i<<"\t"<<j<<"\t"<<device_ctx.grid[i][j].type->name<<endl;
            string site_name = device_ctx.grid[x][y].type->name;
            SiteType* sitetype = database.getSiteType(SiteType::NameString2Enum(site_name));
            if (sitetype == NULL) {
                printlog(LOG_ERROR, "Site type not found: %s", site_name.c_str());
            } else {
                database.addSite(x, y, sitetype);
            }
        }
    }

    /*
    readscl
    */

    /*
    readNets
    */
    auto nets = atom_ctx.nlist.nets();
    Net* net = NULL;
    for (auto net_iter = nets.begin(); net_iter != nets.end(); ++net_iter) {
        string net_name = atom_ctx.nlist.net_name(*net_iter);
        //cout<<"net:\t"<<net_name<<endl;
        net = database.getNet(net_name);
        if (net != NULL) {
            printlog(LOG_ERROR, "Net duplicated: %s", net_name.c_str());
        } else {
            Net newnet(net_name);
            net = database.addNet(newnet);
        }
        auto net_pins = atom_ctx.nlist.net_pins(*net_iter);
        for (auto pin_iter = net_pins.begin(); pin_iter != net_pins.end(); ++pin_iter) {
            auto pin_block = atom_ctx.nlist.pin_block(*pin_iter);
            auto model = atom_ctx.nlist.block_model(pin_block);
            string block_name = atom_ctx.nlist.block_name(pin_block);
            string pin_full_name = atom_ctx.nlist.pin_name(*pin_iter);
            size_t dot_pos = pin_full_name.find_last_of(".");
            string pin_name = pin_full_name.substr(dot_pos + 1);
            Instance* instance = database.getInstance(block_name.c_str());
            Pin* pin = NULL;
            if (instance != NULL) {
                pin = instance->getPin(pin_name);
            } else {
                printlog(LOG_ERROR, "Instance not found: %s", pin_name.c_str());
            }
            if (pin == NULL) {
                printlog(LOG_ERROR, "Pin not found: %s", pin_name.c_str());
            } else {
                net->addPin(pin);
                if (pin->type->type == 'o' && pin->instance->master->name == Master::BUFGCE) {
                    net->isClk = true;
                }
            }
            //cout<<block_name<<"\t"<<atom_ctx.nlist.pin_name(*pin_iter)<<endl;
        }
        //cout<<"\n"<<endl;
    }

    cout << "finish" << endl;
}

// bool Database::readNets(string file){
//     ifstream fs(file);
//     if (!fs.good()) {
//         cerr<<"Read net file: "<<file<<" fail"<<endl;
//         return false;
//     }

//     vector<string> tokens;
//     Net *net = NULL;
//     while(read_line_as_tokens(fs, tokens)){
//         if(tokens[0][0] == '#'){
//             continue;
//         }
//         if(tokens[0] == "net"){
//             net = database.getNet(tokens[1]);
//             if(net != NULL){
//                 printlog(LOG_ERROR, "Net duplicated: %s", tokens[1].c_str());
//             }else{
//                 Net newnet(tokens[1]);
//                 net = database.addNet(newnet);
//             }
//         }else if(tokens[0] == "endnet"){
//             net = NULL;
//         }else{
//             Instance *instance = database.getInstance(tokens[0].c_str());
//             Pin *pin = NULL;

//             if(instance != NULL){
//                 pin = instance->getPin(tokens[1]);
//             }else{
//                 printlog(LOG_ERROR, "Instance not found: %s", tokens[0].c_str());
//             }

//             if(pin == NULL){
//                 printlog(LOG_ERROR, "Pin not found: %s", tokens[1].c_str());
//             }else{
//                 net->addPin(pin);
//                 if (pin->type->type=='o' && pin->instance->master->name==Master::BUFGCE){
//                     net->isClk=true;
//                 }
//             }
//         }
//     }

//     fs.close();
//     return false;
// }
