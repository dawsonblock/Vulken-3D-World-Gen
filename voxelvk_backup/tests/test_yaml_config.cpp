#include <gtest/gtest.h>
#ifdef HAS_YAML_CPP
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <fstream>
#include <string>

static bool file_exists(const std::string& p){
    std::ifstream f(p);
    return f.good();
}

TEST(Config, ParseWorldYaml) {
#ifdef SOURCE_DIR
    const std::string sample = std::string(SOURCE_DIR) + "/tests/resources/sample_world.yaml";
#else
    const std::string sample = "tests/resources/sample_world.yaml";
#endif
    const char* env = std::getenv("WORLD_CONFIG");
    std::string path = (env && *env) ? std::string(env) : sample;
    if(!file_exists(path)) {
        GTEST_SKIP() << "No WORLD_CONFIG and sample YAML missing; skipping.";
    }
    YAML::Node doc = YAML::LoadFile(path);
    ASSERT_TRUE(doc.IsDefined());
    ASSERT_TRUE(doc.IsMap());

    if (path == sample) {
        ASSERT_TRUE(doc["world"]);
        ASSERT_TRUE(doc["voxels"]);
        ASSERT_TRUE(doc["biomes"]);
        EXPECT_TRUE(doc["world"]["seed"].IsScalar());
        EXPECT_TRUE(doc["voxels"]["chunk_size"].IsScalar());
        EXPECT_TRUE(doc["biomes"].IsSequence());
        EXPECT_GE(doc["biomes"].size(), 1u);
    } else {
        // External config: be permissive — only require a top-level map.
        SUCCEED() << "Parsed external WORLD_CONFIG successfully";
    }
}
#else
TEST(Config, ParseWorldYaml) { GTEST_SKIP() << "yaml-cpp not available"; }
#endif
