#include "ResourcePool.h"

/* Global resource manager */
ResourcePool Resource;

extern "C" {
#define IMPORT_FONT(name) \
do{\
    LV_FONT_DECLARE(font_##name)\
    Resource.Font_.AddResource(#name, (void*)&font_##name);\
}while(0)

#define IMPORT_IMG(name) \
do{\
    LV_IMG_DECLARE(img_src_##name)\
    Resource.Image_.AddResource(#name, (void*)&img_src_##name);\
}while (0)

    static void Resource_Init()
    {
        /* Import Fonts */
        IMPORT_FONT(bahnschrift_17);
        IMPORT_FONT(bahnschrift_32);
        IMPORT_FONT(bahnschrift_65);

        /* Settings and the shared rotary indicator are the only runtime images. */
        IMPORT_IMG(dot_blue);
        IMPORT_IMG(setting_wifi);
        IMPORT_IMG(setting_timer);
        IMPORT_IMG(setting_brightness);
    }

} /* extern "C" */

void ResourcePool::Init()
{
    lv_obj_remove_style_all(lv_scr_act());
    lv_disp_set_bg_color(lv_disp_get_default(), lv_color_black());

    Font_.SetDefault((void*)&lv_font_montserrat_14);

    Resource_Init();
}
