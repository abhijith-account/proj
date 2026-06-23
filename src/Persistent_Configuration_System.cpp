#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include "Persistent_Configuration_System.h"
LOG_MODULE_REGISTER(CONFIG_SYS,LOG_LEVEL_INF);

#define NVS_PARTITION storage_partition
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
ConfigStore ConfigStore::instance;

ConfigStore::ConfigStore():initialized(false){
  fs.flash_device = DEVICE_DT_GET(DT_MTD_FROM_FIXED_PARTITION(DT_NODELABEL(storage_partition)));
  fs.offset = DT_REG_ADDR(DT_NODELABEL(storage_partition));
  fs.sector_size=2048;
  fs.sector_count=4;
}

ConfigStore& ConfigStore::getInstance(){
  return instance;
}

bool ConfigStore::init(){
    if (!device_is_ready(fs.flash_device)){
        LOG_ERR("Flash device not ready");
        return false;
    }
    
    int rc=nvs_mount(&fs);
    if (rc){
        LOG_ERR("NVS Mount failed: %d",rc);
        return false;
    }
    
    initialized=true;
    LOG_INF("NVS Configuration Store Mounted Successfully.");
    return true;
}

bool ConfigStore::validateEndurance(ConfigKey key){
    if (!initialized){
        return false;
    }
    
    LOG_WRN("Starting 1,000 Write-Cycle Endurance Test on Key:%d...",static_cast<int>(key));
    
    uint16_t test_val=0;
    uint16_t read_val=0;
    
    for (int i=0;i<1000;i++){
        test_val=i;
        
        if (!set(key,test_val)){
            LOG_ERR("Endurance Test failed to write at cycle %d",i);
            return false;
        }
        
        if (!get(key,read_val)|| read_val!=test_val){
            LOG_ERR("Endurance Test failed to verify at cycle %d",i);
            return false;
        }
    }
    
    LOG_INF("1,000 Write-Cycle Endurance Test passed.Flash is stable");    
    return true;    
}        
