#include <gtest/gtest.h>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <filesystem>

// Mock config loader - in real implementation this would be from src/core/config_loader.hpp
namespace voxelvk {
    struct Config {
        std::map<std::string, std::string> stringValues;
        std::map<std::string, int> intValues;
        std::map<std::string, float> floatValues;
        std::map<std::string, bool> boolValues;
        
        template<typename T>
        T get(const std::string& key, const T& defaultValue = T{}) const;
        
        bool loadFromFile(const std::string& path);
        bool saveToFile(const std::string& path) const;
    };
    
    template<>
    std::string Config::get<std::string>(const std::string& key, const std::string& defaultValue) const {
        auto it = stringValues.find(key);
        return (it != stringValues.end()) ? it->second : defaultValue;
    }
    
    template<>
    int Config::get<int>(const std::string& key, const int& defaultValue) const {
        auto it = intValues.find(key);
        return (it != intValues.end()) ? it->second : defaultValue;
    }
    
    template<>
    float Config::get<float>(const std::string& key, const float& defaultValue) const {
        auto it = floatValues.find(key);
        return (it != floatValues.end()) ? it->second : defaultValue;
    }
    
    template<>
    bool Config::get<bool>(const std::string& key, const bool& defaultValue) const {
        auto it = boolValues.find(key);
        return (it != boolValues.end()) ? it->second : defaultValue;
    }
    
    bool Config::loadFromFile(const std::string& path) {
        std::ifstream file(path);
        if (!file) return false;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;
            
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Simple type inference
            if (value == "true" || value == "false") {
                boolValues[key] = (value == "true");
            } else if (value.find('.') != std::string::npos) {
                try { floatValues[key] = std::stof(value); }
                catch (...) { stringValues[key] = value; }
            } else {
                try { intValues[key] = std::stoi(value); }
                catch (...) { stringValues[key] = value; }
            }
        }
        return true;
    }
    
    bool Config::saveToFile(const std::string& path) const {
        std::ofstream file(path);
        if (!file) return false;
        
        for (const auto& [key, value] : stringValues) {
            file << key << "=" << value << "\n";
        }
        for (const auto& [key, value] : intValues) {
            file << key << "=" << value << "\n";
        }
        for (const auto& [key, value] : floatValues) {
            file << key << "=" << value << "\n";
        }
        for (const auto& [key, value] : boolValues) {
            file << key << "=" << (value ? "true" : "false") << "\n";
        }
        return true;
    }
}

class ConfigLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "voxelvk_config_test";
        std::filesystem::create_directories(testDir);
    }
    
    void TearDown() override {
        if (std::filesystem::exists(testDir)) {
            std::filesystem::remove_all(testDir);
        }
    }
    
    std::filesystem::path testDir;
};

TEST_F(ConfigLoaderTest, LoadValidConfig) {
    std::string configFile = (testDir / "test.config").string();
    
    // Create test config file
    std::ofstream file(configFile);
    file << "window_width=1920\n";
    file << "window_height=1080\n";
    file << "fullscreen=true\n";
    file << "vsync=false\n";
    file << "render_scale=1.5\n";
    file << "app_name=VoxelVK Test\n";
    file << "# This is a comment\n";
    file << "invalid_line_without_equals\n";
    file.close();
    
    voxelvk::Config config;
    ASSERT_TRUE(config.loadFromFile(configFile));
    
    EXPECT_EQ(1920, config.get<int>("window_width"));
    EXPECT_EQ(1080, config.get<int>("window_height"));
    EXPECT_TRUE(config.get<bool>("fullscreen"));
    EXPECT_FALSE(config.get<bool>("vsync"));
    EXPECT_FLOAT_EQ(1.5f, config.get<float>("render_scale"));
    EXPECT_EQ("VoxelVK Test", config.get<std::string>("app_name"));
}

TEST_F(ConfigLoaderTest, DefaultValues) {
    voxelvk::Config config;
    
    EXPECT_EQ(800, config.get<int>("window_width", 800));
    EXPECT_EQ(600, config.get<int>("window_height", 600));
    EXPECT_FALSE(config.get<bool>("fullscreen", false));
    EXPECT_FLOAT_EQ(1.0f, config.get<float>("render_scale", 1.0f));
    EXPECT_EQ("Default App", config.get<std::string>("app_name", "Default App"));
}

TEST_F(ConfigLoaderTest, SaveAndLoadRoundtrip) {
    std::string configFile = (testDir / "roundtrip.config").string();
    
    // Create and populate config
    voxelvk::Config originalConfig;
    originalConfig.intValues["test_int"] = 42;
    originalConfig.floatValues["test_float"] = 3.14159f;
    originalConfig.boolValues["test_bool"] = true;
    originalConfig.stringValues["test_string"] = "Hello World";
    
    // Save config
    ASSERT_TRUE(originalConfig.saveToFile(configFile));
    
    // Load config back
    voxelvk::Config loadedConfig;
    ASSERT_TRUE(loadedConfig.loadFromFile(configFile));
    
    // Verify values
    EXPECT_EQ(42, loadedConfig.get<int>("test_int"));
    EXPECT_FLOAT_EQ(3.14159f, loadedConfig.get<float>("test_float"));
    EXPECT_TRUE(loadedConfig.get<bool>("test_bool"));
    EXPECT_EQ("Hello World", loadedConfig.get<std::string>("test_string"));
}

TEST_F(ConfigLoaderTest, NonExistentFile) {
    std::string nonExistentFile = (testDir / "does_not_exist.config").string();
    
    voxelvk::Config config;
    EXPECT_FALSE(config.loadFromFile(nonExistentFile));
}

TEST_F(ConfigLoaderTest, EmptyFile) {
    std::string emptyFile = (testDir / "empty.config").string();
    
    // Create empty file
    std::ofstream file(emptyFile);
    file.close();
    
    voxelvk::Config config;
    ASSERT_TRUE(config.loadFromFile(emptyFile));
    
    // Should use default values
    EXPECT_EQ(0, config.get<int>("nonexistent", 0));
    EXPECT_EQ("", config.get<std::string>("nonexistent", ""));
}