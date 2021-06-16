#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>

int main()
{
    double gamma = 0;
    int mode = 0;
    std::string path;

    // std::ifstream is RAII, i.e. no need to call close
    std::ifstream cFile("myConfig.txt");
    if (cFile.is_open())
    {
        std::string line;
        while (getline(cFile, line)) 
        {
            line.erase(std::remove_if(line.begin(), line.end(), isspace),line.end());
            if (line[0] == '#' || line.empty()) continue;

            auto delimiterPos = line.find("=");
            auto name = line.substr(0, delimiterPos);
            auto value = line.substr(delimiterPos + 1);

            //Custom coding
            if (name == "gamma") gamma = std::stod(value);
            else if (name == "mode") mode = std::stoi(value);
            else if (name == "path") path = value;
        }
    }
    else 
    {
        std::cerr << "Couldn't open config file for reading.\n";
    }

    std::cout << "\nGamma=" << gamma;
    std::cout << "\nMode=" << mode;
    std::cout << "\nPath=" << path;
    std::getchar();
}
