// Real page logic and LVGL rendering; hardware, NVS and transport are synthetic.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "lvgl.h"
#include "esp_sleep.h"
#include "RGB.h"
#include "app/Pages/Page.h"
#include "hal/hal.h"
#include "hal/motor.h"
#include "services/GestureFSM.h"
#include "services/SceneStore.h"
#define private public
#include "app/Pages/Face/Face.h"
#include "app/Pages/Smartled/Smartled.h"
#include "app/Pages/Codex/V48Aux.h"
#include "app/Pages/Menu/Menu.h"
#undef private

static uint32_t now_ms = 1000;
static bool dark = false, home_requested = false, wake_press = false;
static bool button_pressed = false;
static int shutdown_requests = 0;
static int command_count = 0;
static bool last_power = false;
static int last_motor_mode = -1, motor_requests = 0;
static HAL::SmartledState light = {};
static HAL::MotorInput input = {};
extern "C" uint32_t millis() { return now_ms; }
extern "C" void vTaskDelay(uint32_t) {}
extern "C" uint32_t esp_random() { return 1; }
extern "C" esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return ESP_SLEEP_WAKEUP_UNDEFINED; }
void strip_set_state(StripMode,uint8_t,uint16_t,uint32_t) {}
ResourcePool Resource;
extern lv_font_t font_bahnschrift_17, font_bahnschrift_32, font_bahnschrift_65;
extern lv_font_t xknob_round_ui_12, xknob_round_ui_20, xknob_round_ui_28;
PageBase* PageManager::Push(const char*, const PageBase::Stash_t*) { return nullptr; }
PageBase* PageManager::Pop() { return nullptr; }
bool PageManager::BackHome() { return true; }
namespace HAL {
bool power_is_dark() { return dark; }
bool power_is_dim() { return false; }
bool power_home_requested() { return home_requested; }
bool take_wake_press() { bool result = wake_press; wake_press = false; return result; }
bool encoder_is_pushed() { return button_pressed; }
void power_off() { ++shutdown_requests; }
int get_motor_position() { return input.position; }
MotorInput motor_input() { return input; }
void update_motor_mode(int mode,int position) {
    last_motor_mode=mode;
    ++motor_requests;
    input.position=position;
}
void motor_exit_feedback() {}
void mqtt_init() {}
bool smartled_command(const char*) { return true; }
bool smartled_set(bool has_power,bool power,bool,int,bool,int,char* id,size_t size) {
    ++command_count;
    if (has_power) last_power = power;
    snprintf(id,size,"synthetic-%d",command_count);
    return true;
}
bool smartled_state(SmartledState* out) { *out = light; return true; }
bool codex_state(CodexState* out) { *out = {}; return true; }
bool power_settings_get(PowerSettings* out) { *out = {15,45,true}; return true; }
bool power_settings_set(const PowerSettings*) { return true; }
void power_note_activity() {}
}

static lv_color_t pixels[240*240], buffer[240*40];
static void flush(lv_disp_drv_t* driver,const lv_area_t* area,lv_color_t* data) {
    for (int y=area->y1;y<=area->y2;++y)
        for (int x=area->x1;x<=area->x2;++x) pixels[y*240+x]=*data++;
    lv_disp_flush_ready(driver);
}
static void read_encoder(lv_indev_drv_t*,lv_indev_data_t* data) {
    data->state=LV_INDEV_STATE_REL; data->enc_diff=0;
}
static void preview(PageBase& page,const std::string& path) {
    lv_scr_load(page.root);
    lv_refr_now(nullptr);
    FILE* file=fopen(path.c_str(),"wb"); assert(file);
    uint8_t header[54]={}; header[0]='B'; header[1]='M';
    uint32_t size=54+240*240*3, offset=54, info=40;
    int32_t width=240,height=-240; uint16_t planes=1,bpp=24;
    memcpy(header+2,&size,4); memcpy(header+10,&offset,4); memcpy(header+14,&info,4);
    memcpy(header+18,&width,4); memcpy(header+22,&height,4);
    memcpy(header+26,&planes,2); memcpy(header+28,&bpp,2);
    fwrite(header,1,54,file);
    for (auto pixel:pixels) {
        uint8_t bgr[]={static_cast<uint8_t>(pixel.ch.blue*255/31),
            static_cast<uint8_t>(((pixel.ch.green_h<<3)|pixel.ch.green_l)*255/63),
            static_cast<uint8_t>(pixel.ch.red*255/31)};
        fwrite(bgr,1,3,file);
    }
    fclose(file);
}

int main(int argc,char** argv) {
    assert(argc==2);
    lv_init();
    static lv_disp_draw_buf_t draw; lv_disp_draw_buf_init(&draw,buffer,nullptr,240*40);
    static lv_disp_drv_t display; lv_disp_drv_init(&display);
    display.hor_res=display.ver_res=240; display.flush_cb=flush; display.draw_buf=&draw;
    lv_disp_drv_register(&display);
    static lv_indev_drv_t encoder; lv_indev_drv_init(&encoder);
    encoder.type=LV_INDEV_TYPE_ENCODER; encoder.read_cb=read_encoder;
    lv_indev_drv_register(&encoder);
    Resource.Font_.AddResource("bahnschrift_17",&font_bahnschrift_17);
    Resource.Font_.AddResource("bahnschrift_32",&font_bahnschrift_32);
    Resource.Font_.AddResource("bahnschrift_65",&font_bahnschrift_65);
    light.online=light.ok=light.modern=true;
    light.brightness=50; light.temperature=4000; light.received_ms=now_ms;

    Page::Face face; face.root=lv_obj_create(nullptr); face.onViewLoad();
    face.light_=light;
    face.sync_control_motor_mode(now_ms);
    assert(last_motor_mode==MOTOR_ON_OFF_STRONG_DETENTS && input.position==0);
    const int initial_motor_requests=motor_requests;
    input.position=1; // One physical snap reaches the ON endpoint.
    face.handle({GestureFSM::ROTATE,1},now_ms); face.send(now_ms);
    assert(command_count==1 && last_power && face.pending_);
    face.sync_control_motor_mode(now_ms);
    assert(motor_requests==initial_motor_requests && input.position==1);
    face.handle({GestureFSM::ROTATE,2},now_ms); face.send(now_ms);
    assert(command_count==1); // Idempotent while ON is pending.
    input.position=0;
    face.handle({GestureFSM::ROTATE,-1},now_ms); face.send(now_ms);
    assert(command_count==2 && !last_power); // OFF survives old reported OFF.
    face.sync_control_motor_mode(now_ms);
    assert(motor_requests==initial_motor_requests && input.position==0);
    strcpy(light.last_command_id,face.pending_id_); light.received_ms=++now_ms;
    face.update(); assert(!face.pending_); // Readback before any ACK.
    face.handle({GestureFSM::ROTATE,1},now_ms); face.send(now_ms);
    light.online=false; face.update(); assert(!face.pending_ && !face.dirty_);
    light.online=true; now_ms+=8000; face.update(); assert(command_count==3);
    face.light_=light; face.screen_=Page::Face::NORMAL;
    face.handle({GestureFSM::HOLD_ROTATE,4},now_ms); face.send(now_ms);
    assert(command_count==3); // Held rotation never adjusts the light on Face.
    face.screen_=Page::Face::TEMPERATURE;
    face.handle({GestureFSM::DOUBLE_CLICK,0},now_ms);
    assert(face.screen_==Page::Face::NORMAL);
    face.screen_=Page::Face::SCENES;
    face.handle({GestureFSM::ROTATE,1},now_ms); face.send(now_ms);
    assert(command_count==3);
    dark=true; face.pending_=face.dirty_=true; face.update();
    assert(!face.pending_ && !face.dirty_ && face.screen_==Page::Face::NORMAL);
    dark=false; wake_press=true; face.update(); assert(!wake_press);
    light.power=true; face.light_=light; input.position=0;
    face.sync_control_motor_mode(now_ms);
    assert(last_motor_mode==MOTOR_ON_OFF_STRONG_DETENTS && input.position==1);
    // An external state change repositions the same two-endpoint mode once.
    const int resynced_requests=motor_requests;
    face.sync_control_motor_mode(now_ms);
    assert(motor_requests==resynced_requests);
    face.screen_=Page::Face::LIGHT_OFF; face.render_labels();
    preview(face,std::string(argv[1])+"/face-off.bmp");

    Page::Smartled app; app.root=lv_obj_create(nullptr); app.onViewLoad();
    app.state=light; app.mode=Page::Smartled::POWER;
    app.desired_power=true; app.dirty=true; app.send();
    strcpy(light.last_ack_id,app.pending_id); light.last_ack_success=true;
    light.ack_received_ms=now_ms; light.power=true;
    strcpy(light.last_command_id,"old-command"); light.received_ms=++now_ms;
    app.update(); assert(app.awaiting); // ACK and old state cannot confirm.
    strcpy(light.last_command_id,app.pending_id); light.received_ms=++now_ms;
    app.update(); assert(!app.awaiting && !app.failed);
    app.dirty=true; app.send(); dark=true; app.update();
    assert(!app.awaiting && !app.dirty);
    dark=false;

    Page::V48Aux power(Page::V48Aux::POWER);
    power.root=lv_obj_create(nullptr); power.onViewLoad();
    preview(power,std::string(argv[1])+"/power.bmp");
    // Match the real navigation lifecycle: only Face and the active child
    // remain resident. Keeping all test pages exhausted LVGL's 32KiB pool.
    lv_scr_load(face.root);
    power.onViewDidUnload(); lv_obj_del(power.root);
    app.onViewDidUnload(); lv_obj_del(app.root);
    const int before_shutdown_commands=command_count;
    Page::Menu menu;
    menu.root=lv_obj_create(nullptr); menu.onViewLoad();
    assert(menu.kItemCount==7);
    menu.selected_=6; menu.render(now_ms);
    assert(strcmp(lv_label_get_text(menu.name_),"关机")==0);
    assert(strcmp(lv_label_get_text(menu.index_),"07 / 07")==0);
    preview(menu,std::string(argv[1])+"/menu-shutdown.bmp");
    // Exercise the actual menu gesture path: rotation only focuses shutdown;
    // one short click plus the select animation requests it exactly once.
    menu.gesture_.update(false,input.position,now_ms);
    button_pressed=true; now_ms+=200; menu.update();
    now_ms+=30; menu.update();
    button_pressed=false; now_ms+=100; menu.update();
    now_ms+=30; menu.update();
    now_ms+=421; menu.update();
    assert(shutdown_requests==0 && menu.select_pending_);
    now_ms+=160; menu.update();
    assert(shutdown_requests==1 && !menu.select_pending_);
    now_ms+=1000; menu.update();
    assert(shutdown_requests==1);
    assert(command_count==before_shutdown_commands); // no light power command
    puts("PASS: actual LVGL Face/SmartLED logic: bounded switch endpoints, idempotence, reverse, readback ordering, offline/dark cancellation, precision exit, scene preview, wake");
    puts("PASS: actual LVGL Menu: seventh shutdown item, click animation, single shutdown request, no light command");
}
