/******************************************************************************/
/* Base Periods                                                               */
/******************************************************************************/
#define defaults_abcd_base_period 10
#define defaults_hijk_base_period 500
#define defaults_lmno_base_period 100
#define defaults_pqrs_base_period 100
#define defaults_wafi_base_period 100
#define defaults_enfi_base_period 10
#define defaults_gzad_base_period 10
#define defaults_unzad_base_period 10
#define defaults_pufi_base_period 10
#define defaults_enfi_base_period 10
#define defaults_cofi_base_period 10
#define defaults_chafi_base_period 10
#define defaults_waps_base_period 10
#define defaults_fifo_base_period 100
#define defaults_califo_base_period 100
#define defaults_replay_base_period 100
#define defaults_tofcalc_base_period 100
#define defaults_waan_base_period 10

/******************************************************************************/
/* Sockets configurations                                                     */
/******************************************************************************/
#define defaults_all_slow_joiner_wait 1000

#define defaults_abcd_ip "127.0.0.1"
#define defaults_abcd_status_address "tcp://*:16180"
#define defaults_abcd_data_address "tcp://*:16181"
#define defaults_abcd_commands_address "tcp://*:16182"
#define defaults_abcd_status_address_sub "tcp://127.0.0.1:16180"
#define defaults_abcd_data_address_sub "tcp://127.0.0.1:16181"

#define defaults_efg_http_port 8080

#define defaults_hijk_ip "127.0.0.1"
#define defaults_hijk_status_address "tcp://*:16183"
#define defaults_hijk_commands_address "tcp://*:16184"

#define defaults_lmno_ip "127.0.0.1"
#define defaults_lmno_status_address "tcp://*:16185"
#define defaults_lmno_commands_address "tcp://*:16186"

#define defaults_pqrs_ip "127.0.0.1"
#define defaults_pqrs_status_address "tcp://*:16187"
#define defaults_pqrs_data_address "tcp://*:16188"
#define defaults_pqrs_commands_address "tcp://*:16189"

#define defaults_wafi_ip "127.0.0.1"
#define defaults_wafi_data_address "tcp://*:16190"

#define defaults_enfi_ip "127.0.0.1"
#define defaults_enfi_data_address "tcp://*:16191"

#define defaults_gzad_ip "127.0.0.1"
#define defaults_gzad_data_address "tcp://*:16192"
#define defaults_gzad_data_address_sub "tcp://127.0.0.1:16192"

#define defaults_unzad_ip "127.0.0.1"
#define defaults_unzad_data_address "tcp://*:16193"

#define defaults_pufi_ip "127.0.0.1"
#define defaults_pufi_data_address "tcp://*:16194"

#define defaults_cofi_ip "127.0.0.1"
#define defaults_cofi_data_address "tcp://*:16195"

#define defaults_chafi_ip "127.0.0.1"
#define defaults_chafi_data_address "tcp://*:16196"

#define defaults_waps_ip "127.0.0.1"
#define defaults_waps_data_address "tcp://*:16197"

#define defaults_fifo_ip "127.0.0.1"
#define defaults_fifo_status_address "tcp://*:16198"
#define defaults_fifo_reply_address "tcp://*:16199"

#define defaults_tofcalc_ip "127.0.0.1"
#define defaults_tofcalc_status_address "tcp://*:16200"
#define defaults_tofcalc_data_address "tcp://*:16201"
#define defaults_tofcalc_commands_address "tcp://*:16202"

#define defaults_califo_ip "127.0.0.1"
#define defaults_califo_status_address "tcp://*:16203"
#define defaults_califo_data_address "tcp://*:16204"
#define defaults_califo_commands_address "tcp://*:16205"

#define defaults_waan_ip "127.0.0.1"
#define defaults_waan_status_address "tcp://*:16206"
#define defaults_waan_data_address "tcp://*:16207"
#define defaults_waan_commands_address "tcp://*:16208"

/******************************************************************************/
/* Topics configurations                                                      */
/******************************************************************************/
#define defaults_abcd_status_topic "status_abcd"
#define defaults_abcd_events_topic "events_abcd"
#define defaults_abcd_data_events_topic "data_abcd_events"
#define defaults_abcd_data_waveforms_topic "data_abcd_waveforms"

#define defaults_hijk_status_topic "status_hijk"
#define defaults_hijk_events_topic "events_hijk"

#define defaults_lmno_status_topic "status_lmno"
#define defaults_lmno_events_topic "events_lmno"

#define defaults_pqrs_status_topic "status_pqrs"
#define defaults_pqrs_events_topic "events_pqrs"
#define defaults_pqrs_data_histograms_topic "data_pqrs_histograms"

#define defaults_tofcalc_status_topic "status_tofcalc"
#define defaults_tofcalc_events_topic "events_tofcalc"
#define defaults_tofcalc_data_histograms_topic "data_tofcalc_histograms"

#define defaults_fifo_status_topic "status_fifo"
#define defaults_fifo_events_topic "events_fifo"

#define defaults_califo_status_topic "status_califo"
#define defaults_califo_events_topic "events_califo"

#define defaults_wafi_data_waveforms_topic "data_wafi_waveforms"
#define defaults_wadi_data_waveforms_topic "data_wadi_waveforms"

#define defaults_waan_status_topic "status_waan"
#define defaults_waan_events_topic "events_waan"

/******************************************************************************/
/* CAEN connections configuration                                             */
/******************************************************************************/
#define defaults_abcd_connection_type 0
#define defaults_abcd_link_number 1
#define defaults_abcd_CONET_node 0
#define defaults_abcd_VME_address 0
#define defaults_hijk_connection_type 0
#define defaults_hijk_link_number 0
#define defaults_hijk_CONET_node 0
#define defaults_hijk_VME_address 0

/******************************************************************************/
/* Generic configurations                                                     */
/******************************************************************************/
#define defaults_all_topic_buffer_size 1024

#define defaults_abcd_config_file "config.json"
#define defaults_abcd_verbosity 0
#define defaults_abcd_negative_limit 0xfffa
#define defaults_abcd_events_buffer_max_size 4096
#define defaults_abcd_publish_timeout 3
#define defaults_abcd_zmq_delay 500

#define defaults_abad2_device_number -1

#define defaults_abrp_scope_samples 1024
#define defaults_abrp_pretrigger 512
#define defaults_abrp_post_start_wait 10

#define defaults_abps5000a_publish_timeout 60
#define defaults_abps5000a_baseline_check_timeout 60

#define defaults_efg_publish_timeout 3

#define defaults_hijk_config_file "config_hv.json"
#define defaults_hijk_verbosity 1
#define defaults_hijk_model 6533

#define defaults_lmno_verbosity 0
#define defaults_lmno_publish_timeout 3
#define defaults_lmno_extenstion_events "ade"
#define defaults_lmno_extenstion_waveforms "adw"
#define defaults_lmno_extenstion_raw "adr"

#define defaults_pqrs_verbosity 1
#define defaults_pqrs_publish_timeout 5
#define defaults_pqrs_bins_qshort 128
#define defaults_pqrs_bins_E 512
#define defaults_pqrs_bins_PSD 128
#define defaults_pqrs_bins_baseline 128
#define defaults_pqrs_bins_rate 128
#define defaults_pqrs_min_qshort 10
#define defaults_pqrs_max_qshort 10000
#define defaults_pqrs_min_E 0
#define defaults_pqrs_max_E 40960
#define defaults_pqrs_min_PSD -0.1
#define defaults_pqrs_max_PSD 1.0
#define defaults_pqrs_max_baseline 4096
#define defaults_pqrs_max_rate 500000

#define defaults_replay_skip 0

#define defaults_enfi_min_energy 400.0
#define defaults_enfi_max_energy 60000.0

#define defaults_cofi_coincidence_window_left 200.0
#define defaults_cofi_coincidence_window_right 200.0
#define defaults_cofi_ns_per_sample (2.0 / 1024.0)
#define defaults_cofi_coincidence_buffer_multiplier 2

#define defaults_chafi_topic_subscribe "data_abcd"

#define defaults_waps_waveforms_buffer_multiplier 2
#define defaults_waps_fixed_point_fractional_bits 10

#define defaults_gzad_topic_subscribe ""
#define defaults_unzad_topic_subscribe "compressed_"
#define defaults_unzad_output_buffer_multiplier 4

#define defaults_tofcalc_verbosity 1
#define defaults_tofcalc_publish_timeout 5
#define defaults_tofcalc_bins_ToF 400
#define defaults_tofcalc_min_ToF -100
#define defaults_tofcalc_max_ToF 100
#define defaults_tofcalc_bins_E 512
#define defaults_tofcalc_min_E 0
#define defaults_tofcalc_max_E 40960
#define defaults_tofcalc_ns_per_sample (2.0 / 1024.0)

#define defaults_fifo_verbosity 1
#define defaults_fifo_publish_timeout 3
#define defaults_fifo_expiration_time (3600 * 1000000000ULL)

#define defaults_califo_config_file "config.json"
#define defaults_califo_verbosity 1
#define defaults_califo_publish_fit_events true
#define defaults_califo_accumulation_time 60
#define defaults_califo_publish_timeout 3
#define defaults_califo_expiration_time (70 * 1000000000ULL)
#define defaults_califo_bins_E 512
#define defaults_califo_min_E 5000
#define defaults_califo_max_E 10000
#define defaults_califo_peak_position 9800.0
#define defaults_califo_peak_width 360.0
#define defaults_califo_peak_height 600.0
#define defaults_califo_peak_alpha 2e-4
#define defaults_califo_peak_beta 4200
#define defaults_califo_peak_tolerance 300.0
#define defaults_califo_background_enable false
#define defaults_califo_background_iterations 15
#define defaults_califo_background_enable_smooth false
#define defaults_califo_background_smooth 3
#define defaults_califo_background_order 2
#define defaults_califo_background_compton false

#define defaults_waan_config_file "config.json"
#define defaults_waan_publish_period 3
#define defaults_waan_waveforms_buffer_multiplier 2
#define defaults_waan_fixed_point_fractional_bits 10
