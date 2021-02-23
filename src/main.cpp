// © 2021 Joseph Cameron - All Rights Reserved
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <gtk/gtk.h>

//TODO: refactor graphics
#include "./nordvpn-32-connected.cpp"
#include "./nordvpn-32-default.cpp"
#include "./nordvpn-32-disconnected.cpp"
#include "./nordvpn-32-error.cpp"

/// \brief runs a shell command, returns the standard output 
/// as a stringstream
std::stringstream run_command(const std::string &aCommand) 
{
    std::unique_ptr<FILE, std::function<void(FILE *const)>> pFile([&]()
        {
            if (auto *const p = popen(aCommand.c_str(), "r")) return p;

            throw std::runtime_error("Error opening pipe!\n");
        }(),
        [](FILE *const p)
        {
            if (pclose(p)) throw std::runtime_error(
                "Command not found or exited with error status\n");
        });

    std::stringstream output;

    for (char c; c != EOF; c = static_cast<decltype(c)>(std::fgetc(pFile.get()))) 
        output << c;

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
const nordvpn_status_type get_nordvpn_status()
{
    auto output(run_command("nordvpn status"));

    nordvpn_status_type state;

    for (std::string line; std::getline(output, line);)
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

    return state;
}

/// \brief type used to model a country
using nordvpn_country_type = std::string;

/// \brief type used to model the return of nordvpn countries command
using nordvpn_countries_type = std::vector<nordvpn_country_type>;

/// \brief returns the nordvpn countries as a list
const nordvpn_countries_type get_nordvpn_countries()
{
    auto output(run_command("nordvpn countries"));

    nordvpn_countries_type countries;

    for (std::string line; std::getline(output, line);)
    {
        std::string country;

        for (size_t i(find_first_alpha(line)); i < line.size(); ++i)
        {
            const auto &c = line[i];

            if (c == '\t' || i >=  line.size() - 1)
            {
                if (country.size()) countries.push_back(country);

                country.clear();
            }
            else country += c;
        }

        countries.back().push_back(line.back());
    }

    return countries;
}

/// \brief type used to model the return of nordvpn cities command
using nordvpn_cities_type = std::vector<std::string>;

/// \brief returns the nordvpn cities as a list
const nordvpn_cities_type get_nordvpn_cities(const nordvpn_country_type &aCountry)
{
    std::string command = std::string("nordvpn cities ") + aCountry;

    auto output = run_command(command);

    nordvpn_cities_type cities;

    for (std::string line; std::getline(output, line);)
    {
        std::string country;

        for (size_t i(find_first_alpha(line)); i < line.size(); ++i)
        {
            const auto &c = line[i];

            if (c == '\t' || i >=  line.size() - 1)
            {
                if (country.size()) cities.push_back(country);

                country.clear();
            }
            else country += c;
        }

        cities.back().push_back(line.back());
    }

    return cities;
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

static bool update()
{
    auto status = get_nordvpn_status();

    std::cout << status["Status"];

    std::cout << "\n";

    return true;
}

int main(int argc, char *argv[])
{
    try
    {
        //status
        { 
            auto status = get_nordvpn_status();

            for (const auto &[name, value] : status)
                std::cout << name  << ", " << value << "\n";

            std::cout << "\n";
        }

        // Get countries list
        { 
            const auto countries = get_nordvpn_countries();
            
            for (auto &a : countries) 
            {
                std::cout << a << "\n"; 

                const auto cities = get_nordvpn_cities(a);

                for (auto &b : cities) 
                {
                    std::cout << " " << b << "\n";
                }

                std::cout << "\n";
            }
        }

        gtk_init(&argc, &argv);

        if (auto tray_icon = gtk_status_icon_new())
        {
            gtk_status_icon_set_visible (tray_icon, true);

            /*g_signal_connect(G_OBJECT(tray_icon), 
                    "activate", 
                    G_CALLBACK([](){
                        system(std::string(config::get_browser_command())
                                .append(" ")
                                .append("https://www.wanikani.com/")
                                .c_str());}),
                   nullptr);*/
            //nordvpn_32_default
            gtk_status_icon_set_from_pixbuf(tray_icon, get_connected_icon_graphic()); 

            gtk_status_icon_set_tooltip_text(tray_icon, "aToolTip.c_str()");
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
    }

    return EXIT_SUCCESS;
}

