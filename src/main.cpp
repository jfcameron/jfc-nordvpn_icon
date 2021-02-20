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

    return output;
}

/// \brief type used to model the return of nordvpn status command
using nordvpn_status_type = std::unordered_map</*name*/std::string, 
    /*value*/std::string>;

/// \brief returns the nordvpn program status as a map
const nordvpn_status_type get_nordvpn_status()
{
    auto output(run_command("nordvpn status"));

    nordvpn_status_type state;

    for (std::string line; std::getline(output, line);)
    {
        static const std::string DELIMITER(": ");

        if (auto search(line.find(DELIMITER)); 
            search != std::string::npos && 
            search < line.size() - DELIMITER.size())
        {
            state[line.substr(0, search)] = 
                line.substr(search + DELIMITER.size(), line.size());
        }
    }

    return state;
}

/// \brief type used to model the return of nordvpn countries command
using nordvpn_countries_type = std::vector<std::string>;

/// \brief returns the nordvpn countries as a list
const nordvpn_countries_type get_nordvpn_countries()
{
    auto output(run_command("nordvpn countries"));

    nordvpn_countries_type countries;

    for (std::string line; std::getline(output, line);)
    {
        std::string country;

        for (size_t i(0); i < line.size(); ++i)
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

void nordvpn_connect(const std::string &aCountry, const std::string &aCity)
{
    run_command("nordvpn connect " + aCountry + " " + aCity);
}

void nordvpn_connect(const std::string &aCountry = "")
{
    run_command("nordvpn connect " + aCountry);
}

void nordvpn_disconnect()
{
    run_command("nordvpn disconnect");
}

static bool update()
{
    {
        auto status = get_nordvpn_status();

        for (const auto &[name, value] : status)
        {
            std::cout << name << ", " << value << "\n";
        }

        std::cout << "\n";
    }

    return true;
}

int main(int argc, char *argv[])
{
    {
        const auto countries = get_nordvpn_countries();
        
        for (auto &a : countries) std::cout << a << "\n";
    }

    std::cout << "===============\n";

    gtk_init(&argc, &argv);

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

    return EXIT_SUCCESS;
}

