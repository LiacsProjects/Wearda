//
// Copyright:
//
//   Leiden University,
//   Faculty of Math and Natural Sciences,
//   Leiden Institute of Advanced Computer Science (LIACS)
//   Snellius building | Niels Bohrweg 1 | 2333 CA Leiden
//   The Netherlands
//
// Author:
//
//   Richard M.K. van Dijk
//   Research sofware engineer
//   M: m.k.van.dijk@liacs.leidenuniv.nl
//

#include "sensorapplication.h"

/**
 *
 * @brief All UI widgets used in the front-end.
 *
 */

typedef struct appdata {
    Evas_Object *win;
    Evas_Object *conform;
    Evas_Object *box;
    Evas_Object *spinner_person_id;
    Evas_Object *button_restart;
    Evas_Object *button_clean;
} appdata_s;

float g_person_id = 0;

/**
 *
 * @brief Launch / terminate the sensor service in the background.
 *
 */

static void
launch_sensor_service()
{
    int result;
    app_control_h app_control;

    app_control_create(&app_control);
    app_control_set_operation(app_control, APP_CONTROL_OPERATION_DEFAULT);
    app_control_set_app_id(app_control, "liacs.sensorservice");

    result = app_control_send_launch_request(app_control, NULL, NULL);

    if(result == APP_CONTROL_ERROR_NONE)
        dlog_print(DLOG_INFO, LOG_TAG, "SensorService launched %d", result);
    else
        dlog_print(DLOG_DEBUG, LOG_TAG, "SensorService not launched %d", result);

    app_control_destroy(app_control);

    return;
}

static void
terminate_sensor_service()
{
    int result;
    app_control_h app_control;

    app_control_create(&app_control);
    app_control_set_operation(app_control, APP_CONTROL_OPERATION_DEFAULT);
    app_control_set_app_id(app_control, "liacs.sensorservice");

    result = app_control_send_terminate_request(app_control);

    if(result == APP_CONTROL_ERROR_NONE)
        dlog_print(DLOG_INFO, LOG_TAG, "SensorService terminated");
    else
        dlog_print(DLOG_DEBUG, LOG_TAG, "SensorService not terminated");

    app_control_destroy(app_control);

    return;
}

/**
 *
 * @brief The send_person_identifier_to_service() will sent the person id to
 * the sensor service.
 *
 * @details The service will stop the actual measurement and will start a new measurement.
 * This will also happen if the person identifier of the actual measurement is identical with the
 * new one. In that case new sensor files are created (and the older ones will stay in the folder).
 *
 * There will be no reply from the sensor service expected (fire-and-forget scenario).
 *
 * @param[in] person identifier is an unsigned int between 000 and 999 (3 digit number).
 *
 */

static void
send_person_identifier_to_service(unsigned int personid)
{
    int result = -1;
    app_control_h app_control;
    char personid_string[32];

    app_control_create(&app_control);
    app_control_set_operation(app_control, APP_CONTROL_OPERATION_SEND);
    snprintf(personid_string, 32, "restart %03d", personid);
    app_control_set_uri(app_control, &personid_string);
    app_control_set_app_id(app_control, "liacs.sensorservice");

    result = app_control_send_launch_request(app_control, NULL, NULL);

    if(result == APP_CONTROL_ERROR_NONE)
        dlog_print(DLOG_INFO, LOG_TAG, "Send person identifier %03d %d", personid, result);
    else
        dlog_print(DLOG_DEBUG, LOG_TAG, "Send person identifier %03d not send %d", personid, result);

    app_control_destroy(app_control);

    return;
}

/**
 *
 * @brief The send_delete_all_sensor_files_to_service() will send the command to delete all sensor files
 * created by the sensor service in its data folder, to the sensor service.
 *
 * @details The service will delete all the *aag.dat, *bar.dat, *con.dat and *gps.dat files found in its data folder.
 * In order to do this the service will stop measuring first before deleting all sensor files. So the last
 * sensor file created will also be deleted. So some measurements are lost in that case.
 *
 * There will be no reply from the sensor service expected (fire-and-forget scenario).
 *
 * Deleting files in the Tizen OS can only be done by the app / service which created the files.
 *
 */

static void
send_delete_all_sensor_files_to_service()
{
    int result = -1;
    app_control_h app_control;
    const char *delete_string = "clean";

    app_control_create(&app_control);
    app_control_set_operation(app_control, APP_CONTROL_OPERATION_SEND);
    app_control_set_uri(app_control, delete_string);
    app_control_set_app_id(app_control, "liacs.sensorservice");

    result = app_control_send_launch_request(app_control, NULL, NULL);

    if(result == APP_CONTROL_ERROR_NONE)
        dlog_print(DLOG_INFO, LOG_TAG, "Send delete all sensor files %d", result);
    else
        dlog_print(DLOG_DEBUG, LOG_TAG, "Send delete all sensor files not send %d", result);

    app_control_destroy(app_control);

    return;
}

/**
 *
 * @brief Here follows the callbacks of the UI.
 *
 */

static void
win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
    dlog_print(DLOG_INFO, LOG_TAG, "Delete request call back\n");

    ui_app_exit();
}

static void
win_back_cb(void *data, Evas_Object *obj, void *event_info)
{
    dlog_print(DLOG_INFO, LOG_TAG, "Back button pressed call back\n");

    ui_app_exit();
}

/**
 *
 * @brief Call back called if spinner changed value and is in focus.
 *
 */

static void
spinner_value_change_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
    g_person_id = elm_spinner_value_get(obj);

    dlog_print(DLOG_INFO, LOG_TAG, "Spinner in focus of %03d\n", (int)g_person_id);
}

/**
 *
 * @brief Here follows the call backs of the buttons restart and clean.
 *
 * The restart button will restart the measurement with a given person identifier.
 * It will stop the running measurement.
 *
 * The clean button must be pressed three times in order to have effect.
 * It will stop the running measurement, delete all sensor files
 * on the watch and then continue on with the measurement.
 *
 */

static void
button_restart_clicked_cb(void *data, Evas_Object *obj, void *event_info)
{
    dlog_print(DLOG_INFO, LOG_TAG, "Button restart clicked \n");

    send_person_identifier_to_service(g_person_id);

    sleep(1);

    ui_app_exit();
}

int g_clean_obstacle_counter = 0;
static void
button_clean_clicked_cb(void *data, Evas_Object *obj, void *event_info)
{
    dlog_print(DLOG_INFO, LOG_TAG, "Button clean clicked %d\n", g_clean_obstacle_counter);

    if(g_clean_obstacle_counter < 2) {
	    g_clean_obstacle_counter++;
	    return;
    }

    g_clean_obstacle_counter = 0;
    send_delete_all_sensor_files_to_service();

    sleep(1);

    ui_app_exit();
}

static void
create_base_gui(appdata_s *ad)
{
    // Define Window and box
    ad->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
    elm_win_autodel_set(ad->win, EINA_TRUE);

    if (elm_win_wm_rotation_supported_get(ad->win)) {
        int rots[4] = { 0, 90, 180, 270 };
        elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
    }

    evas_object_smart_callback_add(ad->win, "delete,request", win_delete_request_cb, NULL);
    eext_object_event_callback_add(ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

    ad->box = elm_box_add(ad->win);
    evas_object_show(ad->box);

    // Define a spinner component, part of the box
    ad->spinner_person_id = elm_spinner_add(ad->win);
    elm_object_style_set(ad->spinner_person_id, "vertical");
    elm_spinner_interval_set(ad->spinner_person_id, 0.1);
    elm_spinner_step_set(ad->spinner_person_id, 1);
    elm_spinner_label_format_set(ad->spinner_person_id, "%03d");
    elm_spinner_min_max_set(ad->spinner_person_id, 0, 999);
    evas_object_size_hint_weight_set(ad->spinner_person_id, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(ad->spinner_person_id, EVAS_HINT_FILL, 0.5);
    elm_spinner_editable_set(ad->spinner_person_id, EINA_TRUE);
    evas_object_smart_callback_add(ad->spinner_person_id, "unfocused", spinner_value_change_cb, NULL);
    elm_box_pack_start(ad->box, ad->spinner_person_id);
    evas_object_show(ad->spinner_person_id);

    // Define the start button
    ad->button_restart = elm_button_add(ad->win);
    elm_object_style_set(ad->button_restart, "vertical");
    elm_object_part_text_set(ad->button_restart, NULL, "RESTART");
    evas_object_size_hint_weight_set(ad->button_restart, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(ad->button_restart, EVAS_HINT_FILL, 0.5);
    elm_box_pack_end(ad->box, ad->button_restart);
    evas_object_smart_callback_add(ad->button_restart, "clicked", button_restart_clicked_cb, NULL);
    evas_object_show(ad->button_restart);

    // Define the stop button
    ad->button_clean = elm_button_add(ad->win);
    elm_object_style_set(ad->button_clean, "vertical");
    elm_object_part_text_set(ad->button_clean, NULL, "CLEAN");
    evas_object_size_hint_weight_set(ad->button_clean, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(ad->button_clean, EVAS_HINT_FILL, 0.5);
    elm_box_pack_end(ad->box, ad->button_clean);
    evas_object_smart_callback_add(ad->button_clean, "clicked", button_clean_clicked_cb, NULL);
    evas_object_show(ad->button_clean);

	/* Show window after base gui is set up */
    evas_object_show(ad->win);
}


/**
 *
 * @brief Standard callbacks of the application in control of the Tizen framework.
 *
 */

static bool
app_create(void *data)
{
    dlog_print(DLOG_INFO, LOG_TAG, "SensorApplication created");

    appdata_s *ad = data;

    create_base_gui(ad);

    return true;
}

static void
app_control(app_control_h app_control, void *data)
{
    dlog_print(DLOG_INFO, LOG_TAG, "SensorApplication controlled");

    launch_sensor_service();
}

static void
app_pause(void *data)
{
    dlog_print(DLOG_INFO, LOG_TAG, "SensorApplication paused");
}

static void
app_resume(void *data)
{
    dlog_print(DLOG_INFO, LOG_TAG, "SensorApplication resumed");
}

static void
app_terminate(void *data)
{
    dlog_print(DLOG_INFO, LOG_TAG, "SensorApplication terminated");
}

static void
ui_app_low_battery(app_event_info_h event_info, void *user_data)
{
    dlog_print(DLOG_INFO, LOG_TAG, "SensorApplication low battery");
    return;
}

static void
ui_app_low_memory(app_event_info_h event_info, void *user_data)
{
    dlog_print(DLOG_INFO, LOG_TAG, "SensorApplication low memory");
    return;
}

/**
 *
 * main()
 *
 */

int
main(int argc, char *argv[])
{
    appdata_s ad = {0,};
    int ret = 0;

    ui_app_lifecycle_callback_s event_callback = {0,};
    app_event_handler_h handlers[5] = {NULL, };

    event_callback.create = app_create;
    event_callback.terminate = app_terminate;
    event_callback.pause = app_pause;
    event_callback.resume = app_resume;
    event_callback.app_control = app_control;

    ui_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, ui_app_low_battery, &ad);
    ui_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, ui_app_low_memory, &ad);

    ret = ui_app_main(argc, argv, &event_callback, &ad);
    if (ret != APP_ERROR_NONE) {
        dlog_print(DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
    }

    return ret;
}
