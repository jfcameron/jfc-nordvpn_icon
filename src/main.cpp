// © 2021 Joseph Cameron - All Rights Reserved
#include <optional>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string>
#include <vector>

#include <gtk/gtk.h>
#include <glib.h>

//TODO: refactor graphics
// =-=-=-=-= ICON GRAPHICS LOADING =-=-=-=-=
#include "./nordvpn-32-error.cpp"

enum class icon_name
{
    initializing,
    connected,
    disconnected,
    error
};

using icon_collection_type = std::map<icon_name, GdkPixbuf *>;

/// \brief global, stories icon graphics
icon_collection_type icons; 

/// \brief global, ptr to the gtk icon
GtkStatusIcon *tray_icon;

icon_collection_type init_icons()
{
    static const auto init_icon = [](const icon_type *const pGraphic)
    {
        GdkPixbuf *pixbuf;

        auto &graphic = *pGraphic;

        pixbuf = gdk_pixbuf_new_from_data(
            graphic.pixel_data, //ptr to beginning of data
            GDK_COLORSPACE_RGB, //data has rgb channels (redundant, 
                                //since enum only has 1 value)
            true, //data has an alpha channel
            8, //each channel has 8 bits (hence rgba32)
            graphic.width, //width in pixels
            graphic.height, //height in pixels
            4 * graphic.width, //stride (row length in bytes: 1 
                               //byte per channel * 4 channels * width in pixels)
            nullptr, //destroy functor, use to add user defined clean up
            nullptr //user data for destroy functor
            );

        return pixbuf; 
    };

    icon_collection_type output;

    output[icon_name::initializing] = init_icon(&nordvpn_32_default);
    output[icon_name::connected] = init_icon(&nordvpn_32_connected);
    output[icon_name::disconnected] = init_icon(&nordvpn_32_disconnected);
    output[icon_name::error] = init_icon(&nordvpn_32_error);

    return output;
}

void set_icon(const icon_name &name)
{
    gtk_status_icon_set_from_pixbuf(tray_icon, icons.at(name)); 

}

void set_tooltip(const std::string &aString)
{
    gtk_status_icon_set_tooltip_text(tray_icon, aString.c_str());
}
// =-=-=-=-= END ICON GRAPHICS LOADING =-=-=-=-=

#include <cerrno>
#include <cstring>

/// \brief runs a shell command, returns the standard output 
/// as a stringstream
/// \warn return null if the command failed
std::optional<std::stringstream> run_command(const std::string &aCommand) 
{
    FILE *const pFile([&]()
    {
        if (auto *const p = popen(aCommand.c_str(), "r")) return p;

        throw std::runtime_error("Error opening pipe!\n");
    }());

    std::stringstream output;

    for (char c; c != EOF; c = static_cast<decltype(c)>(std::fgetc(pFile))) 
        output << c;

    if (pclose(pFile)) return {}; //command failed case

    // Skips first line IF its the nord update notice. TODO: move this out to a
    // separate function OR add a vistor
    std::string line;
    std::getline(output, line);

    if (const auto search (line.find("A new version of NordVPN")); 
        search == std::string::npos) 
    {
        output.clear();
        output.seekg(0);
    }

    return output;
}

/// \brief used to skip junk found at the beginning of some output
//TODO: end is trustworthy. walk backward until last nonwhitespace?
// TODO: this is not reliable. The problem are ^ms and whitespace that nord outputs at the beginning of some of its outputs.
size_t find_first_alpha(const std::string &line)
{
    size_t begin_offset(0);

    for (size_t i(0); i < line.size(); ++i)
    {
        if (std::isalpha(line[i])) 
        {
            begin_offset = i;

            break;
        }
    }

    return begin_offset;
}

/// \brief type used to model the return of nordvpn status command
using nordvpn_status_type = std::unordered_map</*name*/std::string, /*value*/std::string>;

/// \brief returns the nordvpn program status as a map
const std::optional<nordvpn_status_type> get_nordvpn_status()
{
    if (auto output(run_command("nordvpn status")); output.has_value())
    {
        nordvpn_status_type state;

        for (std::string line; std::getline(*output, line);)
        {
            static const std::string DELIMITER(": ");

            if (const auto search(line.find(DELIMITER)); 
                search != std::string::npos && 
                search < line.size() - DELIMITER.size())
            {
                const auto offset = find_first_alpha(line);

                state[line.substr(offset, search - offset)] = 
                    line.substr(search + DELIMITER.size(), line.size());
            }
        }

        return {state};
    }
    
    return {};
}

/// \brief type used to model a country
using nordvpn_country_type = std::string;

/// \brief type used to model the return of nordvpn countries command
using nordvpn_countries_type = std::vector<nordvpn_country_type>;

//TODO: consider supporting TTY case
/// \brief returns the nordvpn countries as a list
const std::optional<nordvpn_countries_type> get_nordvpn_countries()
{
    if (auto output(run_command("nordvpn countries")); output.has_value())
    {
        nordvpn_countries_type countries;

        for (std::string line; std::getline(*output, line);)
        {
            for (;;)
            {
                static const std::string DELIMITER = ", ";
                if (auto search = line.find(DELIMITER); search != std::string::npos)
                {
                    countries.push_back(line.substr(0, search));
                    line = line.substr(search+DELIMITER.size(), line.size()-1);
                }
                else
                {
                    countries.push_back(line.substr(0, line.size()-0));
                    break;
                }
            }
        }

        for (auto i = countries[0].size()-1; i > 0; --i)
        {
            if (!std::isalpha(countries[0][i]) && countries[0][i] != '_')
            {
                countries[0] = countries[0].substr(i+1, countries[0].size());

                break;
            }
        }

        return countries;
    }

    return {};
}

//TODO: consider supporting TTY case
/// \brief type used to model the return of nordvpn cities command
using nordvpn_cities_type = std::vector<std::string>;

/// \brief returns the nordvpn cities as a list
const std::optional<nordvpn_cities_type> get_nordvpn_cities(const nordvpn_country_type &aCountry)
{
    nordvpn_cities_type cities;

    std::string command = std::string("nordvpn cities ") + aCountry;
    
    if (auto output = run_command(command); output.has_value())
    {
        for (std::string line; std::getline(*output, line);)
        {
            for (;;)
            {
                static const std::string DELIMITER = ", ";
                if (auto search = line.find(DELIMITER); search != std::string::npos)
                {
                    cities.push_back(line.substr(0, search));
                    line = line.substr(search+DELIMITER.size(), line.size()-1);
                }
                else
                {
                    cities.push_back(line.substr(0, line.size()-0));
                    break;
                }
            }
        }

        for (auto i = cities[0].size()-1; i > 0; --i)
        {
            if (!std::isalpha(cities[0][i]) && cities[0][i] != '_')
            {
                cities[0] = cities[0].substr(i+1, cities[0].size());

                break;
            }
        }

        return {cities};
    }

    return {};
}

/// \brief connects to the specified country and city
void nordvpn_connect(const std::string &aCountry, const std::string &aCity)
{
    run_command("nordvpn connect " + aCountry + " " + aCity);
}

/// \brief connects to the specified country, or default if unspecified
void nordvpn_connect(const std::string &aCountry = "")
{
    run_command("nordvpn connect " + aCountry);
}

/// \brief disconnects from the vpn
void nordvpn_disconnect()
{
    run_command("nordvpn disconnect");
}


/// \brief creates the popup menu
GtkMenu *create_menu()
{
    GtkWidget* widget = gtk_menu_new();

    return GTK_MENU(widget);
}

/// \brief used to offset items when adding to menu
static int h(0);

struct data
{
    std::string foo;
    GtkWidget *entry;
};

std::vector<std::string*> callback_userdata_buffer;

std::string *testy = new std::string("Canada");

void menu_item_click_handler(GtkWidget *widget, gpointer pData)
{
    auto *pString = (std::string *)pData;

    nordvpn_connect(pString ? *pString : "");
}

void add_menu_item(GtkMenu *menu, const std::string &aName, 
    GCallback aOnClick, 
    gpointer aData = nullptr)
{
    GtkWidget* item = gtk_menu_item_new();

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(item), hbox);

    GtkWidget* label = gtk_label_new(nullptr);

    gtk_label_set_markup(GTK_LABEL(label), aName.c_str());

    gtk_container_add(GTK_CONTAINER(hbox), label);

    //if(icon)
    {
        //GtkWidget* image = gtk_image_new_from_icon_name(icon, 
        //GTK_ICON_SIZE_MENU);
        //gtk_container_add(GTK_CONTAINER(hbox), image);
    }

    //if(tooltip) 
    {
        //systray_set_tooltip(item, tooltip);
    }

    if (aOnClick)
    {
        g_signal_connect(G_OBJECT(item), "activate", 
            G_CALLBACK(aOnClick),
            aData);
    }

    gtk_menu_attach(menu, item,
            0,1,
            h,1 + h);

    h+=1;

    gtk_widget_show_all(item);
}

GtkMenu *add_submenu(GtkMenu *menu, const std::string &aName)
{
    // submenu
    GtkWidget* submenu = gtk_menu_new();

    // item
    GtkWidget* item = gtk_menu_item_new();

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(item), hbox);

    GtkWidget* label = gtk_label_new(aName.c_str());
    gtk_container_add(GTK_CONTAINER(hbox), label);

    gtk_menu_attach(menu, item,
        0,1,
        h,1 + h);

    h+=1;

    gtk_widget_show_all(item);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

    return GTK_MENU(submenu);
}

using countries_to_cities_type = std::map<nordvpn_country_type, nordvpn_cities_type>;

countries_to_cities_type country_to_cities_map;

void show_menu()
{
    static GtkMenu *menu;
    
    //TODO separate menu construction from showing, 
    // this should rebuild in the update loop, loop behaviour needs state (enum class likely enough)
    static bool once = true;
    if (once)
    {
        once = false;

        menu = create_menu();

        auto pConnectMenu = add_submenu(menu, "Connect");

        add_menu_item(menu, "Disconnect", 
            []()
            {
                nordvpn_disconnect();
            });

        for (size_t i(0); i < callback_userdata_buffer.size(); ++i)
            delete callback_userdata_buffer[i];
        
        callback_userdata_buffer.clear();

        int i = 0;
            
        add_menu_item(pConnectMenu, "<b>recommended</b>", G_CALLBACK(menu_item_click_handler), nullptr);

        for (auto &[country, cities] : country_to_cities_map)
        {
            if (cities.size() == 1)
            {
                callback_userdata_buffer.push_back(new std::string(country));

                add_menu_item(pConnectMenu, country, G_CALLBACK(menu_item_click_handler), 
                    callback_userdata_buffer[i]);
                
                i++;
            }
            else if (cities.size() > 1)
            {
                auto pCountryMenu = add_submenu(pConnectMenu, country);

                callback_userdata_buffer.push_back(new std::string(country));

                add_menu_item(pCountryMenu, "<b>recommended</b>", G_CALLBACK(menu_item_click_handler), 
                    callback_userdata_buffer[i]);

                i++;

                for (auto &city : cities) 
                {
                    callback_userdata_buffer.push_back(
                        new std::string(country + " " + city));

                    add_menu_item(pCountryMenu, city, G_CALLBACK(menu_item_click_handler), 
                        callback_userdata_buffer[i]);

                    i++;
                }
            }
        }
    }
    
    // show the menu
    gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 0, 0);
}

enum class state_type
{
    initializing,
    status_loop
};

auto STATE = state_type::initializing;

static bool update()
{
    switch(STATE)
    {
        case state_type::initializing:
        { 
            set_tooltip("initializing. If the program loops endlessly here, check that 1: nordvpn is installed, "
                "2: its configured, 3: your internet connection is working");

            if (auto oCountries = get_nordvpn_countries(); oCountries.has_value())
            {
                const auto &countries = *oCountries;

                for (auto &a : countries) 
                {
                    if (const auto oCities = get_nordvpn_cities(a); oCities.has_value())
                    {
                        const auto &cities = *oCities;

                        for (auto &b : cities) 
                        {
                            country_to_cities_map[a].push_back(b);
                        }
                    }
                    else //Connection dropped after fetching countries but before finishing cities; reset
                    {
                        country_to_cities_map.clear();

                        break;
                    }
                }
                
                g_signal_connect(G_OBJECT(tray_icon), 
                    "activate", 
                    G_CALLBACK([]()
                    {
                        show_menu();
                    }),
                    nullptr);

                STATE = state_type::status_loop;
            }
            else
            {
                //TODO: countries failed, go to disconnected loop
            }
        } break;

        case state_type::status_loop:
        {
            if (auto oStatus = get_nordvpn_status(); oStatus.has_value())
            {
                auto &status = *oStatus;

                if (status["Status"] == "Connected") 
                {
                    set_icon(icon_name::connected);
                    
                    set_tooltip(
                        status["Country"] + ", " + status["City"] + "\n" + 
                        status["Current server"] + "\n" +
                        status["Your new IP"] + "\n" +
                        status["Transfer"] + "\n" +
                        "Uptime: " + status["Uptime"]);
                }
                else if (status["Status"] == "Disconnected") 
                {
                    set_icon(icon_name::disconnected);

                    set_tooltip("disconnected");
                }
            }
            else
            {
                set_icon(icon_name::error);

                set_tooltip("status failed. Is the internet connection down?");
            }
        } break;
    }

    return true;
}
// =============== CONFIG STUFF
//header
#include <filesystem>
///
/// \brief: lib for accessing local paths in platform independent way,
/// meant to be used in implementation of datamodel factories etc.
/// linux impl follows freedesktop format for user directories
/// https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html#referencing
//TODO: consider shared directories e.g: /var/log/appname, etc.
namespace jfc
{
    class application_directory_paths final // make const etc
    {
        std::string m_config_dir;
        std::string m_cache_dir; 
        std::string m_data_dir; 

    public:
        /// \brief ~/.conf/appname [on linux]
        const std::string &get_config_dir() const;

        /// \brief ~/.cache/appname [on linux]
        const std::string &get_cache_dir() const;

        /// \brief ~/.local/share/appname [on linux]
        const std::string &get_data_dir() const;

        application_directory_paths(std::string aApplicationName);
    };
}

//cpp
#include <jfcnordvpnicon/buildinfo.h> //TODO: this will change when its a separate lib
namespace jfc
{
    namespace fs = std::filesystem;

    application_directory_paths::application_directory_paths(std::string aApplicationName)
    {
//TODO: since this COULD be a very tiny library, do header only and get rid of my cmake stuff here
#if defined JFC_TARGET_PLATFORM_Linux || defined JFC_TARGET_PLATFORM_Darwin //...
        const std::string home(std::getenv("HOME"));

        m_config_dir = home + "/.config/"      + aApplicationName + "/";
        m_cache_dir  = home + "/.cache/"       + aApplicationName + "/";
        m_data_dir   = home + "/.local/share/" + aApplicationName + "/";
#elif defined JFC_TARGET_PLATFORM_Windows
    //windows api to get user's appdata path...
#error "unimplemented platform"
#else
#error "unsupported platform"
#endif
    };

    const std::string &application_directory_paths::get_config_dir() const 
    { 
        if (!fs::exists(m_config_dir)) fs::create_directories(m_config_dir);

        return m_config_dir; 
    }
    
    const std::string &application_directory_paths::get_cache_dir() const 
    { 
        if (!fs::exists(m_cache_dir)) fs::create_directories(m_cache_dir);

        return m_cache_dir; 
    }
    
    const std::string &application_directory_paths::get_data_dir() const 
    { 
        if (!fs::exists(m_data_dir)) fs::create_directories(m_data_dir);

        return m_data_dir; 
    }
}

// -- usage --
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <ctime>
#include <chrono>

class appdata final
{
    jfc::application_directory_paths path;

public:
    void write_to_log_file(const std::string &aMessage);

    void write_to_version_file();

    appdata();
};

appdata::appdata()
: path("nordvpn_icon")
{}

#include <iomanip>//put_time
void appdata::write_to_log_file(const std::string &aMessage)
{
    std::ofstream m_logfile(path.get_data_dir() + "crash.log", std::ios::app);

    auto itt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    m_logfile << std::put_time(std::gmtime(&itt), "%FT%T: ") << aMessage << "\n";
}

void appdata::write_to_version_file()
{
    std::ofstream f(path.get_data_dir() + "version.txt");

    f 
        << "project remote: " << jfcnordvpnicon_BuildInfo_Git_Remote_URL << "\n"
        << "git hash: " << jfcnordvpnicon_BuildInfo_Git_Commit << "\n"
        << "build date: " << jfcnordvpnicon_BuildInfo_Git_Date << "\n";
}

// =============== END CONFIG STUFF
#define JFC_IS_TTY_IMPLEMENTATION

// === IS_TTY -> tiny header only project to determine if a file refers to a tty
//header
#include <cstdio>

namespace jfc
{
    bool is_tty(decltype(stdout) aStream = stdout);
}

#ifdef JFC_IS_TTY_IMPLEMENTATION
    #if _WIN32
        #include <io.h>
    #else
        #include <unistd.h>
    #endif
bool jfc::is_tty(decltype(stdout) aStream)
{
    #if _WIN32
    return _isatty(_fileno(stdout));
    #else
    return isatty(fileno(stdout));
    #endif
}
#endif

// === END IS_TTY

int main(int argc, char *argv[])
{
    appdata data;

    if (jfc::is_tty()) 
    {
        std::cout << "=== " << jfcnordvpnicon_BuildInfo_ProjectName << " ===\n" 
            "Linux system tray icon frontend for the nordvpn cli.\n"
            "=== WARNING ===\n"
            "You are running this program from a TTY.\n"
            "Basic functionality will work but you will not be able to choose country/city.\n"
            "This is because some of nordvpn's outputs are structured differently when called from a process with a TTY\n"
            "This program has been designed to run \"in the background\", and therefore expects no associated TTY\n"
            "=== build info ===\n"
            "project remote: " << jfcnordvpnicon_BuildInfo_Git_Remote_URL << "\n"
            "git hash: " << jfcnordvpnicon_BuildInfo_Git_Commit << "\n"
            "build date: " << jfcnordvpnicon_BuildInfo_Git_Date << "\n";
    }
    else
    {
        data.write_to_version_file();
    }

    try
    {
        gtk_init(&argc, &argv);

        auto pCombo = gtk_combo_box_new();

        if (tray_icon = gtk_status_icon_new())
        {
            gtk_status_icon_set_visible (tray_icon, true);

            icons = init_icons();

            set_icon(icon_name::initializing);

            set_tooltip("aToolTip.c_str()");
        }
        else throw std::runtime_error("could not init tray icon");

        g_timeout_add_seconds(1, [](void *const vp) 
            {
                auto token = reinterpret_cast<std::string *>(vp);

                update();

                static auto UPDATE_RATE_SECONDS(5);

                g_timeout_add_seconds(UPDATE_RATE_SECONDS, 
                    reinterpret_cast<GSourceFunc>(update), nullptr);

                return int(0);
            }, 
            nullptr);

        gtk_main();
    }
    catch (const std::exception e)
    {
        std::cout << "error: " << e.what() << "\n";
        data.write_to_log_file(e.what());
    }

    return EXIT_SUCCESS;
}

