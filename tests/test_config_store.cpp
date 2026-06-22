#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <cstdint>
#include <cerrno>
#include <sys/types.h>

#include "Persistent_Configuration_System.h"

struct MockNVSEntry{
    uint16_t id;
    uint8_t data[16];
    size_t len;
    bool active;
};

static std::array<MockNVSEntry,10> mock_nvs_storage;
static bool mock_device_ready=true;
static bool mock_nvs_mount_fail=false;
static bool mock_nvs_read_fail=false;
static bool mock_nvs_write_fail=false;
static bool mock_nvs_corrupt_data=false;

extern "C"{
    bool device_is_ready(const struct device *dev){
        return mock_device_ready;
    }
    
    int nvs_mount(struct nvs_fs *fs){
        if (mock_nvs_mount_fail){
            return -ENODEV;
        }
        return 0;
    }
    ssize_t nvs_read(struct nvs_fs *fs,uint16_t id,void *data,size_t len){
        if (mock_nvs_read_fail){
            return -EIO;
        }
        for (const auto& entry:mock_nvs_storage){
            if (entry.active && entry.id==id){
                size_t copy_len=(len<entry.len)? len:entry.len;
                std::memcpy(data,entry.data,copy_len);
                
                if (mock_nvs_corrupt_data && copy_len>0){
                    ((uint8_t*)data)[0]^=0xFF;
                }
                return copy_len;
            }
        }
        return -ENOENT;
    }
    
    ssize_t nvs_write(struct nvs_fs *fs,uint16_t id,const void *data,size_t len){
        if (mock_nvs_write_fail){
            return -EIO;
        }
        
        if (len>16){
            return -EINVAL;
        }
        
        for (auto& entry:mock_nvs_storage){
            if (entry.active && entry.id==id){
                std::memcpy(entry.data,data,len);
                entry.len=len;
                return len;
            }
        }
        
        for (auto& entry : mock_nvs_storage){
            if (!entry.active){
                entry.active=true;
                entry.id=id;
                std::memcpy(entry.data,data,len);
                entry.len=len;
                return len;
            }
        }
        return -ENOSPC;
    }
}

class ConfigStoreTestSuite : public ::testing::Test {
    protected:
          void  SetUp() override{
              for (auto& entry:mock_nvs_storage){
                  entry.active=false;
              }
              mock_device_ready=true;
              mock_nvs_mount_fail=false;
              mock_nvs_write_fail=false;
              mock_nvs_read_fail=false;
              mock_nvs_corrupt_data=false;
          }
};

TEST_F(ConfigStoreTestSuite,SingletonEnforcesStrictUniqueness){
    ConfigStore& instance_a=ConfigStore::getInstance();
    ConfigStore& instance_b=ConfigStore::getInstance();

    EXPECT_EQ(&instance_a,&instance_b)<<"Singleton pattern violated: Multiple instances generated!";
}

TEST_F(ConfigStoreTestSuite,UninitializedStateRejectsOperations){
    ConfigStore& config= ConfigStore::getInstance();
    
    EXPECT_FALSE(config.validateEndurance(ConfigKey::ALARM_THRESHOLD));
    
    uint16_t dummy_val=123;
    EXPECT_FALSE(config.set(ConfigKey::ALARM_THRESHOLD,dummy_val));
    
    uint16_t read_val=0;
    EXPECT_FALSE(config.get(ConfigKey::ALARM_THRESHOLD,read_val));
}

TEST_F(ConfigStoreTestSuite,InitFailsWhenNVSMountFails){
    mock_nvs_mount_fail=true;
    
    testing::internal::CaptureStdout();
    EXPECT_FALSE(ConfigStore::getInstance().init());
    std::string output=testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(output.find("[ERR] NVS Mount failed:")!= std::string::npos)<<"Expected mount failure log missing! Actual: "<< output;
}

TEST_F(ConfigStoreTestSuite, TypedGetSetRoundTrip){
    ConfigStore& config= ConfigStore::getInstance();
    
    testing::internal::CaptureStdout();
    ASSERT_TRUE(config.init());
    std::string output=testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("[INF] NVS Configuration Store Mounted Successfully.")!= std::string::npos)<<"Expected init success log missing!";
    
    uint16_t input_rate=75;    
    EXPECT_TRUE(config.set(ConfigKey::INFUSION_RATE,input_rate));
    
    uint16_t output_rate=0;
    EXPECT_TRUE(config.get(ConfigKey::INFUSION_RATE,output_rate));
    
    EXPECT_EQ(input_rate,output_rate)<<"NVS Data corruption during uint16_t round-trip!";
    
    uint32_t unknown_val=0;
    EXPECT_FALSE(config.get(ConfigKey::DEVICE_ID,unknown_val))<<"NVS read should fail for non-existent keys!";
}

TEST_F(ConfigStoreTestSuite, ValidateEnduranceThresholds){
    ConfigStore& config= ConfigStore::getInstance();
    config.init();
    
    testing::internal::CaptureStdout();
    bool passed=config.validateEndurance(ConfigKey::ALARM_THRESHOLD);
    std::string output=testing::internal::GetCapturedStdout();
  
    EXPECT_TRUE(passed)<<"Flash memory failed the 1,000 write-cycle endurance test";
    EXPECT_TRUE(output.find("[WRN] Starting 1,000 Write-Cycle Endurance Test on Key:3...")!=std::string::npos);
    EXPECT_TRUE(output.find("[INF] 1,000 Write-Cycle Endurance Test passed.Flash is stable")!= std::string::npos);
    
    uint16_t final_value=0;
    EXPECT_TRUE(config.get(ConfigKey::ALARM_THRESHOLD,final_value));
    EXPECT_EQ(final_value,999);
}

TEST_F(ConfigStoreTestSuite,ValidateEnduranceFailsOnWriteError){
    ConfigStore& config=ConfigStore::getInstance();
    config.init();

    mock_nvs_write_fail=true;
    
    testing::internal::CaptureStdout();
    EXPECT_FALSE(config.validateEndurance(ConfigKey::ALARM_THRESHOLD));
    std::string output=testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(output.find("[ERR] Endurance Test failed to write at cycle 0")!= std::string::npos)<<"Expected write failure log missing! Actual: " << output;
}

TEST_F(ConfigStoreTestSuite,ValidateEnduranceFailsOnReadError){
    ConfigStore& config=ConfigStore::getInstance();
    config.init();
    
    mock_nvs_read_fail=true;
    EXPECT_FALSE(config.validateEndurance(ConfigKey::ALARM_THRESHOLD));
}

TEST_F(ConfigStoreTestSuite,RejectsReInitializationIfHardware){
    mock_device_ready=false;
    
    testing::internal::CaptureStdout();
    EXPECT_FALSE(ConfigStore::getInstance().init())<<"ConfigStore bypassed hardware hardware readiness checks during re-init";
    std::string output=testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(output.find("[ERR] Flash device not ready")!=std::string::npos)<<"Expected device not ready log missing! Actual: "<< output;
}

TEST_F(ConfigStoreTestSuite, ValidateEnduranceFailsOnDataMismatch){
    ConfigStore& config=ConfigStore::getInstance();
    config.init();
    
    mock_nvs_corrupt_data=true;

    testing::internal::CaptureStdout();
    EXPECT_FALSE(config.validateEndurance(ConfigKey::ALARM_THRESHOLD));
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(output.find("[ERR] Endurance Test failed to verify at cycle 0")!=std::string::npos);
}
