#include <zephyr/kvss/nvs.h>
#include <zephyr/logging/log.h>
#include <type_traits>
#include <cstdint>
#pragma once

enum class ConfigKey:uint16_t{DEVICE_ID=1,INFUSION_RATE=2,ALARM_THRESHOLD=3,FULL_CHARGE_LOG=4};

class ConfigStore {
    private:
        struct nvs_fs fs;
        bool initialized;
        
        static ConfigStore instance;
        
        ConfigStore();
        ~ConfigStore()=default;
        
        ConfigStore(const ConfigStore&)=delete;
        ConfigStore& operator=(const ConfigStore&)=delete;
        
    public:
        static ConfigStore& getInstance();
        
        bool init();
        
        template <typename T>
        bool set(ConfigKey key,const T& value){
            static_assert(std::is_trivially_copyable<T>::value,"Config data must be trivially copyable");
            
            if (!initialized){
                return false;
            }
            
            uint16_t nvs_id=static_cast<uint16_t>(key);
            ssize_t written=nvs_write(&fs,nvs_id,&value,sizeof(T));
            
            return (written>0||written==0);
          }
          
          template <typename T>
          bool get(ConfigKey key,T& out_value){
              static_assert(std::is_trivially_copyable<T>::value,"Config data must be trivially copyable.");
               
              if (!initialized){
                  return false;
              }
              
              uint16_t nvs_id=static_cast<uint16_t>(key);
              ssize_t bytes_read=nvs_read(&fs,nvs_id,&out_value,sizeof(T));
              
              return (bytes_read==sizeof(T));
           }
           
           bool validateEndurance(ConfigKey key);
};
