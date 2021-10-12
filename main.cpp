#include "port_cmsis_systick.c"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

//#include "boards.h"

#include "mcp9808.hpp"
//#include "NrfLogger.hpp"

#include "SystemTask.hpp"
#include "NotificationManager.hpp"
#include <algorithm>


//#include "SEGGER_SYSVIEW.h"


#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "app_timer.h"
#include "ble_nus.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "bsp_btn_ble.h"
#include "nrf_pwr_mgmt.h"

#include "uart.hpp"
#include "uart_app.hpp"
//#include "ble_cust_service.h"
//extern "C" { 
    //#include "ble_common.h" 
//}

//#include "custom_service.h"
#include "controller.h"


static void clock_init(void)
{
    ret_code_t err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);
}

/* Instantiations */
static constexpr uint8_t queueSize = 10;
static constexpr uint8_t itemSize  = 1;

QueueHandle_t systemQueue = xQueueCreate(queueSize, itemSize); // creating a queue to store bytes

// UART - assigning a uart instance to be static so it could be used inside UART IRQ handler
UartCommParams_t commParams = 
{
        .rxPinNo = RX_PIN_NUMBER,
        .txPinNo = TX_PIN_NUMBER,
        .rtsPinNo = RTS_PIN_NUMBER,
        .ctsPinNo = CTS_PIN_NUMBER,
        .hwFlowCntl = UartHwFcDisabled,
        .useParity = false,
        .baudRate = NRF_UART_BAUDRATE_115200
};


// uart instance
Uart uart(&commParams, NRF_UART0, APP_IRQ_PRIORITY_LOWEST, uartCallback, systemQueue);

// Temp sensor
MCP9808 tmpSensor;

// Notification Manager
NotificationManager notificationManager(&tmpSensor);

// system task
SystemTask systemTask(uart, tmpSensor, notificationManager, systemQueue);

//NrfLogger logger; // TODO - uncomment!



static constexpr uint32_t idleTime = 3000;
int m = 0;
//void IdleTimerCallback(TimerHandle_t xTimer)
//{
//    NRF_LOG_INFO("......Idle timeout -> Going to sleepXxX !!!! ");
//    m++;
//    //NRF_LOG_FLUSH();
//}


extern "C" {
    void vApplicationMallocFailedHook(void)
    {
        NRF_LOG_INFO("Heap allocation failed...\n");
        NRF_LOG_FLUSH();
    }
}

// TODO -- uncomment
// extern "C" {
//    void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName)
//    {
//        NRF_LOG_INFO("Stack overflow ... task name: %s\n", pcTaskName);
//        NRF_LOG_FLUSH();
//    }
//}


void initLeds()
{
    bsp_board_init(BSP_INIT_LEDS); // bsp_init(BSP_INIT_LEDS, bsp_event_handler);	  // configure all LEDs (1-4) to output

    //bsp_board_led_on(3);
    //APP_ERROR_CHECK(err_code);

    //if (timerReturn != NULL)
    //{
    //    timerStarted = xTimerStart(timerReturn, 0);
    //}
    //xTaskNotifyGive(systemTask.taskHandle);
}


/**@brief Function for initializing the timer module.
 */
static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
void bsp_event_handler(bsp_event_t event)
{
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;

        //case BSP_EVENT_DISCONNECT:
        //    err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        //    if (err_code != NRF_ERROR_INVALID_STATE)
        //    {
        //        APP_ERROR_CHECK(err_code);
        //    }
            break;

        case BSP_EVENT_WHITELIST_OFF:
            //if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            //{
            //    err_code = ble_advertising_restart_without_whitelist(&m_advertising);
            //    if (err_code != NRF_ERROR_INVALID_STATE)
            //    {
            //        APP_ERROR_CHECK(err_code);
            //    }
            //}
            break;

        default:
            break;
    }
}

/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    bsp_event_t startup_event;

    uint32_t err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for application main entry.
 */
int main()
{
   //SEGGER_SYSVIEW_Conf();

    const char *openApp = "SmartWatch.";
    uart.PrintUart("Welcome to the App");// %s", openApp);
    
    BleCustomService bleCustomService(&systemTask);
    BleController bleController(bleCustomService);
    bleController.Init();


    //SEGGER_SYSVIEW_Start();
   

    // Initialize modules  
    auto res = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(res);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    /* Initialize clock driver for better time accuracy in FREERTOS */
    //clock_init();	    - TODO - change to camelCase

    initLeds();


    //TimerHandle_t timerReturn = xTimerCreate("ledTimer", pdMS_TO_TICKS(200), pdTRUE, (void *) 0, cb);

    //if (timerReturn != NULL)
    //{
        //BaseType_t Started = xTimerStart(timerReturn, 0);
    //}

    //TimerHandle_t idleTimer = xTimerCreate ("idleTimer", pdMS_TO_TICKS(idleTime), pdFALSE, 0, IdleTimerCallback);
    //xTimerStart(idleTimer, 0);
    
    //while(1);

    vTaskStartScheduler();		// Start FreeRTOS scheduler

    
    //uint8_t tmp[40];
    //tmpSensor.xferData(tmp, 1);

    //while(true) 
    //{
    //    uint16_t data = tmpSensor.read();
    //    notificationManager.unsubscribe(&tmpSensor);
    //    nrf_delay_ms(3000);
    //}
}
