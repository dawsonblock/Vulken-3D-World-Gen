#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

// Mock file I/O utilities - in real implementation these would be from src/core/file_io.hpp
namespace voxelvk {
    class FileIO {
    public:
        static bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
            std::ofstream file(path, std::ios::binary);
            if (!file) return false;
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            return file.good();
        }
        
        static bool readFile(const std::string& path, std::vector<uint8_t>& data) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) return false;
            
            auto size = file.tellg();
            data.resize(size);
            file.seekg(0);
            file.read(reinterpret_cast<char*>(data.data()), size);
            return file.good();
        }
        
        static bool fileExists(const std::string& path) {
            return std::filesystem::exists(path);
        }
        
        static size_t getFileSize(const std::string& path) {
            if (!std::filesystem::exists(path)) return 0;
            return std::filesystem::file_size(path);
        }
    };
}

class FileIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "voxelvk_test";
        std::filesystem::create_directories(testDir);
    }
    
    void TearDown() override {
        if (std::filesystem::exists(testDir)) {
            std::filesystem::remove_all(testDir);
        }
    }
    
    std::filesystem::path testDir;
};

TEST_F(FileIOTest, WriteAndReadBinaryFile) {
    std::string testFile = (testDir / "test_binary.dat").string();
    std::vector<uint8_t> originalData = {0x01, 0x02, 0x03, 0xFF, 0x00, 0xAB, 0xCD};
    
    // Write data
    ASSERT_TRUE(voxelvk::FileIO::writeFile(testFile, originalData));
    ASSERT_TRUE(voxelvk::FileIO::fileExists(testFile));
    
    // Read data back
    std::vector<uint8_t> readData;
    ASSERT_TRUE(voxelvk::FileIO::readFile(testFile, readData));
    
    // Verify data integrity
    ASSERT_EQ(originalData.size(), readData.size());
    EXPECT_EQ(originalData, readData);
}

TEST_F(FileIOTest, FileSize) {
    std::string testFile = (testDir / "test_size.dat").string();
    std::vector<uint8_t> data(1024, 0xAA); // 1KB of data
    
    ASSERT_TRUE(voxelvk::FileIO::writeFile(testFile, data));
    EXPECT_EQ(1024, voxelvk::FileIO::getFileSize(testFile));
}

TEST_F(FileIOTest, NonExistentFile) {
    std::string nonExistentFile = (testDir / "does_not_exist.dat").string();
    
    EXPECT_FALSE(voxelvk::FileIO::fileExists(nonExistentFile));
    EXPECT_EQ(0, voxelvk::FileIO::getFileSize(nonExistentFile));
    
    std::vector<uint8_t> data;
    EXPECT_FALSE(voxelvk::FileIO::readFile(nonExistentFile, data));
}

TEST_F(FileIOTest, EmptyFile) {
    std::string testFile = (testDir / "empty.dat").string();
    std::vector<uint8_t> emptyData;
    
    ASSERT_TRUE(voxelvk::FileIO::writeFile(testFile, emptyData));
    EXPECT_TRUE(voxelvk::FileIO::fileExists(testFile));
    EXPECT_EQ(0, voxelvk::FileIO::getFileSize(testFile));
    
    std::vector<uint8_t> readData;
    ASSERT_TRUE(voxelvk::FileIO::readFile(testFile, readData));
    EXPECT_TRUE(readData.empty());
}