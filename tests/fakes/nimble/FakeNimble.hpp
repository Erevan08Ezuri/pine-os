#pragma once
#include <cstdint>
using esp_err_t=int;
constexpr int ESP_OK=0;
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
constexpr int BLE_ADDR_RANDOM_ID=3;
constexpr int BLE_HS_CONN_HANDLE_NONE=0xffff;
constexpr int BLE_HS_EINVAL=3,BLE_HS_ENOTCONN=7,BLE_HS_EALREADY=2;
constexpr int BLE_ERR_REM_USER_CONN_TERM=19;
constexpr int BLE_GAP_EVENT_DISC=1,BLE_GAP_EVENT_DISC_COMPLETE=2,BLE_GAP_EVENT_CONNECT=3;
constexpr int BLE_GAP_EVENT_DISCONNECT=4,BLE_GAP_EVENT_REPEAT_PAIRING=5,BLE_GAP_REPEAT_PAIRING_IGNORE=0;
constexpr int BLE_HCI_ADV_RPT_EVTYPE_ADV_IND=0,BLE_HCI_ADV_RPT_EVTYPE_DIR_IND=1;
struct ble_addr_t{std::uint8_t type{},val[6]{};};
struct ble_gap_disc_params{std::uint16_t itvl{},window{};std::uint8_t passive{},filter_duplicates{};};
struct ble_hs_adv_fields{const std::uint8_t* name{};std::uint8_t name_len{};};
struct ble_gap_event{
    int type{};
    struct{ble_addr_t addr;std::uint8_t event_type{},length_data{};const std::uint8_t* data{};int rssi{};}disc;
    struct{int reason{};}disc_complete;
    struct{int status{};std::uint16_t conn_handle{};}connect;
    struct{struct{std::uint16_t conn_handle{};}conn;}disconnect;
};
using ble_gap_event_fn=int(ble_gap_event*,void*);
struct ble_hs_cfg_t{void(*sync_cb)(){};void(*reset_cb)(int){};};
extern ble_hs_cfg_t ble_hs_cfg;
extern "C" {
esp_err_t nimble_port_init();esp_err_t nimble_port_deinit();int nimble_port_stop();void nimble_port_run();
int ble_hs_util_ensure_addr(int);int ble_hs_id_infer_auto(int,std::uint8_t*);
int ble_hs_adv_parse_fields(ble_hs_adv_fields*,const std::uint8_t*,std::uint8_t);
int ble_gap_disc_active();int ble_gap_disc_cancel();int ble_gap_conn_active();int ble_gap_conn_cancel();
int ble_gap_disc(std::uint8_t,int,const ble_gap_disc_params*,ble_gap_event_fn*,void*);
int ble_gap_connect(std::uint8_t,const ble_addr_t*,int,const void*,ble_gap_event_fn*,void*);
int ble_gap_terminate(std::uint16_t,int);
}
