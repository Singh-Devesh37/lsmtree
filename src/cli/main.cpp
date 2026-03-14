#include "engine/lsm_engine.h"
#include "engine/sstable.h"
#include "config/lsm_config.h"
#include <iostream>
#include <filesystem>

void printHelp() {
    std::cout << "Commands:\n"
              << "  put <key> <value>\n"
              << "  get <key>\n"
              << "  delete <key>\n"
              << "  scan <startKey> <endKey>\n"
              << "  flush\n"
              << "  compact\n"
              << "  read <filename> <key>\n"
              << "  config\n"
              << "  exit\n";
}

int main(int argc, char* argv[])
{
    // Load configuration from file or use defaults
    std::string configPath = "./resources/lsm_config.json";
    if (argc > 1) {
        configPath = argv[1];
    }

    LSMConfig config = LSMConfig::load(configPath);
    config.print();
    std::cout << std::endl;

    // Create directory structure
    for (int i = 0; i < config.maxLevels; ++i)
    {
        std::filesystem::create_directories(config.baseDir + "/ssTables/" + std::to_string(i));
    }

    LSMEngine engine(config.baseDir, config.flushThreshold, config.maxLevels, config.useBlockBasedFormat);

    std::string cmd;
    printHelp();
    while (true)
    {
        std::cout << "> ";
        std::cin >> cmd;

        if (cmd == "put")
        {
            std::string key, value;
            std::cin >> key >> value;
            engine.put(key, value);
        }
        else if (cmd == "get")
        {
            std::string key;
            std::cin >> key;
            std::string value;
            value = engine.get(key);
            if (value.empty())
            {
                std::cout << "Key not found\n";
            }
            else
            {
                std::cout << value << "\n";
            }
        }
        else if (cmd == "delete")
        {
            std::string key;
            std::cin >> key;
            engine.remove(key);
        }
        else if (cmd == "flush")
        {
            engine.flush();
        }
        else if (cmd == "read")
        {
            std::string filename, key;
            std::cin >> filename >> key;
            std::string value = SSTable::readFromFile(filename, key);
            std::cout << value << "\n";
        }
        else if (cmd == "scan")
        {
            std::string startKey, endKey;
            std::cin >> startKey >> endKey;
            auto results = engine.scan(startKey, endKey);

            if (results.empty())
            {
                std::cout << "No results found in range [" << startKey << ", " << endKey << "]\n";
            }
            else
            {
                std::cout << "Found " << results.size() << " results:\n";
                for (const auto& [key, value] : results)
                {
                    std::cout << "  " << key << " -> " << value << "\n";
                }
            }
        }
        else if (cmd == "compact")
        {
            engine.manualCompact();
        }
        else if (cmd == "config")
        {
            config.print();
        }
        else if (cmd == "exit")
        {
            std::cout << "Program Exit.\n";
            break;
        }
        else
        {
            std::cout << "Unknown command\n";
            printHelp();
        }
        // Auto Compaction
        engine.autoCompact();
    }

    return 0;
}