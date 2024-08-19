set(component_srcs "thread_leader.c" "thread_utils.c")

idf_component_register(SRCS "${component_srcs}"
                       INCLUDE_DIRS "include"
                        REQUIRES driver log esp_common openthread nvs_flash vfs lora json)

                       
