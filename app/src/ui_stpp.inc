// SPDX-License-Identifier: GPL-3.0-or-later
// Included inside ui.cpp's private namespace: both inputs use the same callbacks.
stpp::Server g_property_server;
unsigned g_remote_pressed = 0;
std::uint64_t g_property_published = 0;
std::string property_number(unsigned value) {
    char text[24]; std::snprintf(text,sizeof text,"%u",value); return text;
}
constexpr const char *category_uri[] = {
    "general", "video", "audio", "subtitles", "languages", "appearance", "bookmarks",
    "metadata", "network", "bittorrent", "about", "developer", "search", "upgrade"
};

struct PropertyBuilder {
    stpp::Graph graph;
    std::vector<std::pair<std::string, uint32_t>> &ids;
    uint32_t &next;
    uint32_t add(const std::string &key, const char *name, stpp::Type type,
                 uint32_t parent = UINT32_MAX) {
        auto it=std::find_if(ids.begin(),ids.end(),[&key](const auto &entry) { return entry.first==key; });
        if(it==ids.end()) { ids.emplace_back(key,next++); it=ids.end()-1; }
        stpp::Node node; node.id = it->second; node.name = name; node.type = type;
        if (parent != UINT32_MAX) at(parent).children.push_back(node.id);
        graph.nodes.push_back(std::move(node));
        return it->second;
    }
    stpp::Node &at(uint32_t id) {
        return *std::find_if(graph.nodes.begin(), graph.nodes.end(),
                            [id](const auto &n) { return n.id == id; });
    }
    uint32_t dir(const std::string &key, const char *name, uint32_t parent) {
        return add(key, name, stpp::Type::Directory, parent);
    }
    void str(uint32_t parent, const std::string &key, const char *name, const char *value) {
        at(add(key, name, stpp::Type::String, parent)).text = value;
    }
    uint32_t integer(uint32_t parent, const std::string &key, const char *name, int value) {
        const auto id = add(key, name, stpp::Type::Integer, parent); at(id).integer = value; return id;
    }
    uint32_t item(uint32_t parent, const std::string &key, const char *title,
                  const char *type, const char *url) {
        auto id = dir(key, "", parent);
        str(id,key+"/type","type",type); str(id,key+"/url","url",url);
        auto md = dir(key+"/metadata","metadata",id);
        str(md,key+"/title","title",title);
        integer(id,key+"/enabled","enabled",1);
        return id;
    }
};

void property_open(const std::string &uri) noexcept {
    g_overlay = Overlay::none;
    if (uri == "page:home" || uri == "movian:home") { g_place=Place::home; return; }
    if (uri == "settings:" || uri.starts_with("settings:")) {
        open_settings();
        for (unsigned i=0;i<std::size(category_uri);++i)
            if (uri == std::string("settings:")+category_uri[i]) {
                g_settings_category=i; g_settings_in_category=true;
                g_settings_row=move_setting_row(-1,1); break;
            }
        return;
    }
    if (uri == "movian:plugins") { g_place=Place::plugins; return; }
    if (uri == "movian:network") { g_overlay=Overlay::home_network; return; }
    if (uri == "movian:library") { open_home_folder(MOVIAN_DATA_DIR,"Movian library"); return; }
    if (uri == "movian:title") { open_home_folder(MOVIAN_DOWNLOAD0,"Title data"); return; }
    if (uri == "movian:usb") { open_home_folder(kUsb0,"USB"); return; }
    if (uri == "movian:bookmarks") { open_home_folder(kBookmarksUri,"Bookmarks"); return; }
    if (uri == "movian:zip") { g_place=Place::plugins; begin_zip_picker(); return; }
    if (uri == "movian:repository") { g_place=Place::plugins; g_repo_categories=true; return; }
    for (int i=0;i<g_installed_count;++i)
        if (uri == std::string("movian:plugin:")+g_installed_plugins[i].id) {
            g_place=Place::plugins; g_active_plugin=i; open_plugin_page(nullptr); return;
        }
    // Only paths already exported by the current browser are accepted here.
    if (g_place==Place::videos)
        for (int i=0;i<g_row_count;++i) if (uri==g_rows[i].path) {
            g_stack[g_depth].selected=i; select_row(); return;
        }
    if (g_place==Place::plugins && g_plugin_depth>0) {
        const auto &page=g_plugin_pages[g_plugin_depth-1];
        for(int i=0;i<page.count;++i) if(uri==page.items[i].url) {
            g_plugin_selection[g_plugin_depth-1]=i; update_plugins(pad::cross); return;
        }
    }
    screen::notify("This remote action is not implemented yet");
}

void property_command(const stpp::Command &command) noexcept {
    if (command.kind==stpp::Command::Kind::OpenURL && command.binding=="nav") {
        property_open(command.text); return;
    }
    if (command.binding.starts_with("setting:") &&
        (command.kind==stpp::Command::Kind::SetInteger || command.kind==stpp::Command::Kind::ToggleInteger)) {
        const int action=std::atoi(command.binding.c_str()+8);
        const int count=setting_choices(action);
        const int value=command.kind==stpp::Command::Kind::ToggleInteger ? !current_choice(action) : command.integer;
        if(count && value>=0 && value<count &&
           (command.kind!=stpp::Command::Kind::ToggleInteger || count==2))
            apply_setting_choice(action,value);
        return;
    }
    if(command.kind!=stpp::Command::Kind::Action && command.kind!=stpp::Command::Kind::Dynamic) return;
    const auto &action=command.text;
    if ((command.binding=="nav" && action=="Quit") ||
        (command.binding=="quit" && (action=="Activate" || action=="Enter" || action=="Quit"))) {
        request_quit(); return;
    }
    if(action=="Back" || action=="back") {
        if(g_overlay==Overlay::none && g_place==Place::home) return;
        g_remote_pressed|=pad::circle;
    } else if(action=="Home" || action=="home") property_open("page:home");
    else if(action=="Up") g_remote_pressed|=pad::up;
    else if(action=="Down") g_remote_pressed|=pad::down;
    else if(action=="Left") g_remote_pressed|=pad::left;
    else if(action=="Right") g_remote_pressed|=pad::right;
    else if(action=="Activate" || action=="Enter") g_remote_pressed|=pad::cross;
    else if(action=="PlayPause") { if(player::active()) player::toggle_pause(); }
    else if(action=="Stop") { if(player::active()) player::close(); }
    else if(action=="SeekForward") { if(player::active()) player::seek_step_by(1); }
    else if(action=="SeekBackward") { if(player::active()) player::seek_step_by(-1); }
    else screen::notify("This remote action is not implemented yet");
}

void publish_properties() {
    static std::vector<std::pair<std::string,uint32_t>> ids{{"global",0}};
    static uint32_t next=1;
    PropertyBuilder b{{},ids,next};
    b.add("global","",stpp::Type::Directory);
    auto app=b.dir("app","app",0);
    b.str(app,"app/name","systemname",network::config().system_name);
    b.str(app,"app/version","version",kVersion);
    auto navs=b.dir("navigators","navigators",0);
    auto nav=b.dir("navigator","current",navs);
    b.at(b.add("nav/event","eventSink",stpp::Type::Void,nav)).binding="nav";
    auto pages=b.dir("pages","pages",nav);
    const bool settings_page=g_overlay==Overlay::settings || g_overlay==Overlay::setting_detail;
    std::string pagekey=settings_page ? (g_settings_in_category ? std::string("settings/")+category_uri[g_settings_category] : "settings") :
        g_place==Place::home ? "home" : g_place==Place::plugins ? "plugins" : g_place==Place::videos ? "files" : "torrents";
    auto page=b.dir("page/"+pagekey,"",pages); b.at(pages).selected=page;
    b.at(b.add("currentpage","currentpage",stpp::Type::Void,nav)).link=page;
    b.at(b.add("page/event/"+pagekey,"eventSink",stpp::Type::Void,page)).binding="nav";
    std::string url=settings_page ? (g_settings_in_category ? std::string("settings:")+category_uri[g_settings_category] : "settings:") :
        g_place==Place::home ? "page:home" : "movian:"+pagekey;
    b.str(page,"page/url/"+pagekey,"url",url.c_str());
    auto model=b.dir("model/"+pagekey,"model",page);
    b.str(model,"model/type/"+pagekey,"type",settings_page ? "settings" : g_place==Place::home ? "home" : "directory");
    b.integer(model,"model/loading/"+pagekey,"loading",0);
    auto md=b.dir("model/md/"+pagekey,"metadata",model);
    const char *title=settings_page ? (g_settings_in_category ? settings_menu::categories[g_settings_category].title : "Settings") :
        g_place==Place::videos ? g_stack[g_depth].title : place_name[static_cast<int>(g_place)];
    b.str(md,"model/title/"+pagekey,"title",title);
    auto nodes=b.dir("model/nodes/"+pagekey,"nodes",model);
    if(settings_page) {
        if(!g_settings_in_category) {
            for(unsigned i=0;i<settings_menu::categories.size();++i) {
                auto uri=std::string("settings:")+category_uri[i];
                b.item(nodes,"category/"+property_number(i),settings_menu::categories[i].title,"settings",uri.c_str());
            }
        } else {
            const auto rows=settings_rows();
            for(unsigned i=0;i<rows.size();++i) {
                const auto &row=rows[i]; auto key=pagekey+"/row/"+property_number(i);
                const int count=setting_choices(row.action);
                auto item=b.item(nodes,key,row.title,row.kind==settings_menu::Kind::section ? "separator" :
                    row.kind==settings_menu::Kind::toggle ? "bool" : count ? "multiopt" : "action","");
                if(count) {
                    auto value=b.integer(item,key+"/value","value",current_choice(row.action));
                    b.at(value).binding="setting:"+property_number(row.action);
                    if(row.kind!=settings_menu::Kind::toggle) {
                        auto options=b.dir(key+"/options","options",item);
                        for(int n=0;n<count;++n) {
                            char label[128]; choice_label(row.action,n,label,sizeof label);
                            auto option=b.dir(key+"/opt/"+property_number(n),property_number(n).c_str(),options);
                            b.str(option,key+"/opt/title/"+property_number(n),"title",label);
                            if(n==current_choice(row.action)) {
                                b.at(options).selected=option;
                                b.at(b.add(key+"/current","current",stpp::Type::Void,item)).link=option;
                            }
                        }
                    }
                } else {
                    const char *binding=row.action==settings_menu::quit ? "quit" : "pending";
                    b.at(b.add(key+"/event","eventSink",stpp::Type::Void,item)).binding=binding;
                }
            }
        }
    } else if(g_place==Place::videos) {
        for(int i=0;i<g_row_count;++i) {
            const auto &row=g_rows[i];
            b.item(nodes,"file/"+property_number(i),row.title,row.kind==RowKind::folder?"directory":"video",row.path);
        }
    } else if(g_place==Place::plugins) {
        if(g_plugin_depth>0) {
            const auto &p=g_plugin_pages[g_plugin_depth-1];
            for(int i=0;i<p.count;++i) b.item(nodes,"plugin/item/"+property_number(i),p.items[i].title,p.items[i].type,p.items[i].url);
        } else {
            b.item(nodes,"plugin/repository","Plugin repository","directory","movian:repository");
            b.item(nodes,"plugin/zip","Install from ZIP","directory","movian:zip");
            for(int i=0;i<g_installed_count;++i) {
                auto uri=std::string("movian:plugin:")+g_installed_plugins[i].id;
                b.item(nodes,"plugin/installed/"+property_number(i),g_installed_plugins[i].title,"directory",uri.c_str());
            }
        }
    }
    auto services=b.dir("services","services",0);
    auto stable=b.dir("services/stable","stable",services);
    constexpr const char *home_uri[]={"movian:plugins","movian:network","settings:","movian:library","movian:title","movian:usb","movian:resume","movian:bookmarks"};
    for(unsigned i=0;i<std::size(home_tiles);++i) {
        auto service=b.item(stable,"service/"+property_number(i),home_tiles[i].title,"other",home_uri[i]);
        b.str(service,"service/title/"+property_number(i),"title",home_tiles[i].title);
    }
    auto clock=b.dir("clock","clock",0); b.integer(clock,"clock/time","unixtime",static_cast<int>(::time(nullptr)));
    b.dir("popups","popups",0);
    auto notifications=b.dir("notifications","notifications",0); b.dir("notifications/nodes","nodes",notifications);
    auto media=b.dir("media","media",0); auto current=b.dir("media/current","current",media);
    b.str(current,"media/type","type",player::active()?"video":"");
    b.str(current,"media/status","playstatus",player::playing()?"play":"pause");
    b.integer(current,"media/seek","canSeek",player::active()); b.integer(current,"media/pause","canPause",player::active());
    b.at(b.add("media/events","eventSink",stpp::Type::Void,current)).binding="nav";
    auto position=b.add("media/position","currenttime",stpp::Type::Float,current); b.at(position).real=player::time();
    auto mediamd=b.dir("media/metadata","metadata",current); b.str(mediamd,"media/title","title",player::title());
    auto queue=b.dir("playqueue","playqueue",0); b.integer(queue,"playqueue/active","active",0);
    auto logs=b.dir("logbuffer","logbuffer",0);
    diagnostics::Entry entries[192]; const auto count=diagnostics::copy_entries(entries,std::size(entries));
    // Log IDs derive from record sequence, so evictions remove the old reference.
    for(std::size_t i=0;i<count;++i) {
        const auto id=static_cast<uint32_t>(0x80000000u | (entries[i].sequence&0x7fffffffu));
        stpp::Node node; node.id=id; node.type=stpp::Type::Directory;
        b.at(logs).children.push_back(id); b.graph.nodes.push_back(std::move(node));
        // Scalar IDs are stable for a retained record, independent of its slot.
        for(int field=0;field<3;++field) {
            stpp::Node value; value.id=static_cast<uint32_t>(0x40000000u+((entries[i].sequence%0x0fffffffu)*3)+field);
            value.type=field==2?stpp::Type::Integer:stpp::Type::String;
            value.name=field==0?"prefix":field==1?"message":"severity";
            value.text=field==0?"PS5":entries[i].text; value.integer=1;
            b.at(id).children.push_back(value.id); b.graph.nodes.push_back(std::move(value));
        }
    }
    g_property_server.publish(std::move(b.graph));
}
