
#include <gtest/gtest.h>
#ifdef HAS_YAML_CPP
#include <yaml-cpp/yaml.h>
#include <string>
#include <fstream>
#include <cstdlib>

static bool file_exists(const std::string& p){ std::ifstream f(p); return f.good(); }

static ::testing::AssertionResult ValidateSchema(const YAML::Node& doc){
    if(!doc.IsMap()) return ::testing::AssertionFailure() << "Top-level not a map";

    // world.seed: required int
    if(!doc["world"] || !doc["world"]["seed"] || !doc["world"]["seed"].IsScalar())
        return ::testing::AssertionFailure() << "world.seed missing";
    try { doc["world"]["seed"].as<long long>(); } catch(...) {
        return ::testing::AssertionFailure() << "world.seed not integer";
    }

    // voxels.chunk_size: required int > 0
    if(!doc["voxels"] || !doc["voxels"]["chunk_size"] || !doc["voxels"]["chunk_size"].IsScalar())
        return ::testing::AssertionFailure() << "voxels.chunk_size missing";
    try {
        int cs = doc["voxels"]["chunk_size"].as<int>();
        if(cs <= 0) return ::testing::AssertionFailure() << "chunk_size <= 0";
    } catch(...) { return ::testing::AssertionFailure() << "chunk_size not int"; }

    // voxels.render_distance: optional int >= 0
    if(doc["voxels"]["render_distance"]) {
        try {
            int rd = doc["voxels"]["render_distance"].as<int>();
            if(rd < 0) return ::testing::AssertionFailure() << "render_distance < 0";
        } catch(...) { return ::testing::AssertionFailure() << "render_distance not int"; }
    }

    // biomes: sequence of {name: string, id: int >= 0}
    if(!doc["biomes"] || !doc["biomes"].IsSequence() || doc["biomes"].size()==0)
        return ::testing::AssertionFailure() << "biomes missing/empty";
    for (size_t i=0; i<doc["biomes"].size(); ++i){
        auto n = doc["biomes"][i];
        if(!n.IsMap()) return ::testing::AssertionFailure() << "biomes["<<i<<"] not map";
        if(!n["name"] || !n["name"].IsScalar()) return ::testing::AssertionFailure() << "biomes["<<i<<"].name";
        if(!n["id"]   || !n["id"].IsScalar())   return ::testing::AssertionFailure() << "biomes["<<i<<"].id";
        try {
            int id = n["id"].as<int>();
            if(id < 0) return ::testing::AssertionFailure() << "biomes["<<i<<"].id < 0";
        } catch(...) {
            return ::testing::AssertionFailure() << "biomes["<<i<<"].id not int";
        }
    }

    return ::testing::AssertionSuccess();
}

TEST(YamlSchema, SampleConforms){
#ifdef SOURCE_DIR
    std::string path = std::string(SOURCE_DIR) + "/tests/resources/sample_world.yaml";
#else
    std::string path = "tests/resources/sample_world.yaml";
#endif
    ASSERT_TRUE(file_exists(path));
    YAML::Node doc = YAML::LoadFile(path);
    EXPECT_TRUE(ValidateSchema(doc));
}

TEST(YamlSchema, ExternalConformsOrSkip){
    const char* env = std::getenv("WORLD_CONFIG");
    if(!env || !*env){ GTEST_SKIP() << "WORLD_CONFIG not set"; }
    YAML::Node doc = YAML::LoadFile(env);
    EXPECT_TRUE(ValidateSchema(doc));
}
#else
TEST(YamlSchema, Placeholder){ GTEST_SKIP() << "yaml-cpp not available"; }
#endif
